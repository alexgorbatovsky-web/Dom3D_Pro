// Shared lifetime and numeric input for gizmo drags and keyboard transforms.
void OpenGLViewport::BeginTransformDrag(TransformAxis axis, const QPoint& point, bool free_transform) {
    if (!document_ || !document_->HasSelection()) return;
    active_transform_axis_ = highlighted_transform_axis_ = axis;
    dragging_transform_ = true;
    free_transform_ = free_transform;
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = {};
    transform_drag_rotation_input_angle_ = transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
    document_->GetTransformGizmoCenter(transform_drag_center_);
    transform_drag_axis_ = AxisVector(axis);
    if (axis == TransformAxis::ScreenPlane && transform_operation_ == TransformOperation::Rotate) {
        Vec3 forward{}, right{}, up{};
        viewport_camera_basis(camera_, forward, right, up);
        transform_drag_axis_ = forward * -1.0f;
    }
    if (selection_mode_ == SelectionMode::Point) CaptureCurvePointChangeBefore();
    last_mouse_ = point;
    update();
}

void OpenGLViewport::BeginFreeTransform(TransformAxis axis) {
    if (!document_ || !document_->HasSelection()) return;
    if (dragging_transform_) FinishTransformDrag(false);
    if (axis == TransformAxis::None) axis = transform_operation_ == TransformOperation::Scale
        ? TransformAxis::UniformScale : TransformAxis::ScreenPlane;
    BeginTransformDrag(axis, mapFromGlobal(QCursor::pos()), true);
    const QString constraint = axis == TransformAxis::X ? "X" : axis == TransformAxis::Y ? "Y"
        : axis == TransformAxis::Z ? "Z" : axis == TransformAxis::UniformScale ? "XYZ" : LanguageManager::Instance().Translate("Screen");
    emit StatusTextChanged(DomText("TransformFreeHelp", "Transform (%1): move mouse; X/Y/Z constrain; Space enters a value; click/Enter confirms; Esc cancels").arg(constraint));
}

void OpenGLViewport::ResetTransformPreview() {
    if (document_ && transform_drag_has_preview_) {
        const bool points = selection_mode_ == SelectionMode::Point;
        if (transform_operation_ == TransformOperation::Move) {
            const Vec3 inverse = transform_drag_move_delta_ * -1.0f;
            if (points) document_->MoveSelectedCurvePoints(inverse, xy_plane_view_enabled_);
            else document_->PreviewMoveSelectedObjects(inverse);
        } else if (transform_operation_ == TransformOperation::Rotate) {
            if (points) document_->RotateSelectedCurvePoints(transform_drag_center_, transform_drag_axis_, -transform_drag_rotation_angle_);
            else document_->PreviewRotateSelectedObjects(transform_drag_center_, transform_drag_axis_, -transform_drag_rotation_angle_);
        } else {
            const Vec3 axis = active_transform_axis_ == TransformAxis::UniformScale ? Vec3{} : transform_drag_axis_;
            const float inverse = 1.0f / transform_drag_scale_factor_;
            if (points) document_->ScaleSelectedCurvePoints(transform_drag_center_, axis, inverse);
            else document_->PreviewScaleSelectedObjects(transform_drag_center_, axis, inverse);
        }
    }
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = {};
    transform_drag_rotation_input_angle_ = transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
}

void OpenGLViewport::FinishTransformDrag(bool commit) {
    const bool unchanged = transform_operation_ == TransformOperation::Move
        ? dot(transform_drag_move_delta_, transform_drag_move_delta_) < 1.e-14f
        : std::fabs(TransformValue() - (transform_operation_ == TransformOperation::Scale ? 1.0 : 0.0)) < 1.e-7;
    if (commit && unchanged)
        commit = false;
    if (commit) CommitTransformDrag();
    else ResetTransformPreview();
    dragging_transform_ = free_transform_ = false;
    transform_drag_has_preview_ = false;
    transform_drag_move_delta_ = transform_drag_axis_ = {};
    transform_drag_rotation_input_angle_ = transform_drag_rotation_angle_ = 0.0f;
    transform_drag_scale_factor_ = 1.0f;
    active_transform_axis_ = highlighted_transform_axis_ = TransformAxis::None;
    if (transform_value_popup_ && transform_value_popup_->isVisible()) {
        transform_value_popup_->hide();
        setFocus(Qt::OtherFocusReason);
    }
    RestoreDefaultToolCursor();
    update();
}

bool OpenGLViewport::IsTransformKey(const QKeyEvent* event) const {
    if (tool_ != ToolMode::Transform) return false;
    const int key = event->key();
    // Preserve the existing Esc-to-finish behavior of an ordinary gizmo drag.
    // Free transforms and the numeric editor have an explicit cancel step.
    if (key == Qt::Key_Escape && !free_transform_
        && !(transform_value_popup_ && transform_value_popup_->isVisible())) return false;
    if (dragging_transform_ && (key == Qt::Key_Escape || key == Qt::Key_Return
        || key == Qt::Key_Enter || key == Qt::Key_Control || key == Qt::Key_Space)) return true;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return false;
    return transform_keyboard_enabled_ && (key == Qt::Key_X || key == Qt::Key_Y || key == Qt::Key_Z);
}

bool OpenGLViewport::HandleTransformKey(QKeyEvent* event) {
    if (!IsTransformKey(event)) return false;
    event->accept();
    if (event->isAutoRepeat()) return true;
    switch (event->key()) {
    case Qt::Key_Space: ShowTransformValueEditor(); break;
    case Qt::Key_Escape: FinishTransformDrag(false); break;
    case Qt::Key_Return:
    case Qt::Key_Enter: FinishTransformDrag(true); break;
    case Qt::Key_X: BeginFreeTransform(TransformAxis::X); break;
    case Qt::Key_Y: BeginFreeTransform(TransformAxis::Y); break;
    case Qt::Key_Z: BeginFreeTransform(TransformAxis::Z); break;
    case Qt::Key_Control:
        if (transform_operation_ == TransformOperation::Rotate)
            HandleTransformDrag(last_mouse_, event->modifiers() | Qt::ControlModifier);
        break;
    }
    return true;
}

double OpenGLViewport::TransformValue() const {
    if (transform_operation_ == TransformOperation::Scale) return transform_drag_scale_factor_;
    if (transform_operation_ == TransformOperation::Rotate) return transform_drag_rotation_angle_ * 180.0 / 3.141592653589793;
    return active_transform_axis_ == TransformAxis::ScreenPlane
        ? std::sqrt(dot(transform_drag_move_delta_, transform_drag_move_delta_))
        : dot(transform_drag_move_delta_, transform_drag_axis_);
}

void OpenGLViewport::ShowTransformValueEditor() {
    if (!dragging_transform_) return;
    if (!transform_value_popup_) {
        auto* frame = new QFrame(this);
        frame->setObjectName("TransformValuePopup");
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setAutoFillBackground(true);
        auto* layout = new QHBoxLayout(frame);
        layout->setContentsMargins(6, 4, 6, 4);
        auto* label = new QLabel(frame);
        label->setObjectName("TransformValueLabel");
        layout->addWidget(label);
        transform_value_editor_ = new QLineEdit(frame);
        transform_value_editor_->setObjectName("TransformValueEditor");
        transform_value_editor_->setFixedWidth(115);
        layout->addWidget(transform_value_editor_);
        transform_value_popup_ = frame;
        connect(transform_value_editor_, &QLineEdit::returnPressed, this, &OpenGLViewport::AcceptTransformValue);
    }
    double value = TransformValue();
    QString label = LanguageManager::Instance().Translate("Scale");
    if (transform_operation_ == TransformOperation::Move) {
        const auto unit = LoadDisplayLengthUnit();
        value = MillimetersToDisplay(value, unit);
        label = LanguageManager::Instance().Translate("Distance") + " (" + DisplayLengthUnitSuffix(unit).trimmed() + ")";
    } else if (transform_operation_ == TransformOperation::Rotate) label = DomText("Angle", "Angle") + QString::fromUtf8(" (°)");
    transform_value_popup_->findChild<QLabel*>("TransformValueLabel")->setText(label);
    transform_value_editor_->setText(NumberInputLocale().toString(value, 'g', 9));
    transform_value_editor_->setStyleSheet({});
    transform_value_editor_->setToolTip(DomText("TransformValueHelp", "Enter confirms; Esc cancels"));
    transform_value_popup_->adjustSize();
    const QPoint anchor = last_mouse_ + QPoint(16, 20);
    transform_value_popup_->move(std::clamp(anchor.x(), 0, std::max(0, width() - transform_value_popup_->width())),
                                 std::clamp(anchor.y(), 0, std::max(0, height() - transform_value_popup_->height())));
    transform_value_popup_->show();
    transform_value_popup_->raise();
    if (QWidget::mouseGrabber() == this) releaseMouse();
    transform_value_editor_->setFocus(Qt::OtherFocusReason);
    transform_value_editor_->selectAll();
}

void OpenGLViewport::AcceptTransformValue() {
    if (!dragging_transform_ || !transform_value_editor_) return;
    bool ok = false;
    const QString input = transform_value_editor_->text().trimmed();
    double value = QString(input).replace(',', '.').toDouble(&ok);
    if (transform_operation_ == TransformOperation::Move) value = DisplayToMillimeters(value, LoadDisplayLengthUnit());
    if (!ok || !std::isfinite(value) || !ApplyTransformValue(value)) {
        transform_value_editor_->setStyleSheet("QLineEdit { border: 1px solid #e65c5c; }");
        transform_value_editor_->setToolTip(DomText("TransformValueInvalid", "Enter a finite number; scale factor: 0.001 to 1000"));
        transform_value_editor_->setFocus();
        return;
    }
    FinishTransformDrag(true);
}

bool OpenGLViewport::ApplyTransformValue(double value) {
    if (!document_ || !document_->HasSelection() || !std::isfinite(value)
        || std::fabs(value) > std::numeric_limits<float>::max()) return false;
    const bool points = selection_mode_ == SelectionMode::Point;
    bool changed = false;
    if (transform_operation_ == TransformOperation::Move) {
        Vec3 direction = transform_drag_axis_;
        if (active_transform_axis_ == TransformAxis::ScreenPlane) {
            direction = normalize(transform_drag_move_delta_);
            if (dot(direction, direction) < 0.5f) {
                Vec3 forward{}, up{};
                viewport_camera_basis(camera_, forward, direction, up);
            }
        }
        const Vec3 target = direction * static_cast<float>(value);
        const Vec3 delta = target - transform_drag_move_delta_;
        if (dot(delta, delta) < 1.e-12f) return true;
        changed = points ? document_->MoveSelectedCurvePoints(delta, xy_plane_view_enabled_)
            : document_->PreviewMoveSelectedObjects(delta);
        if (changed) transform_drag_move_delta_ = target;
    } else if (transform_operation_ == TransformOperation::Rotate) {
        const float target = static_cast<float>(value * 3.141592653589793 / 180.0);
        const float delta = target - transform_drag_rotation_angle_;
        if (std::fabs(delta) < 1.e-7f) return true;
        changed = points ? document_->RotateSelectedCurvePoints(transform_drag_center_, transform_drag_axis_, delta)
            : document_->PreviewRotateSelectedObjects(transform_drag_center_, transform_drag_axis_, delta);
        if (changed) transform_drag_rotation_input_angle_ = transform_drag_rotation_angle_ = target;
    } else {
        // Keep the cumulative factor invertible, including cancellation.
        if (value < 0.001 || value > 1000.0) return false;
        const float target = static_cast<float>(value);
        const float factor = target / transform_drag_scale_factor_;
        if (std::fabs(factor - 1.0f) < 1.e-7f) return true;
        const Vec3 axis = active_transform_axis_ == TransformAxis::UniformScale ? Vec3{} : transform_drag_axis_;
        // Large corrections are applied in valid, bounded steps.
        float remaining = factor;
        do {
            const float step = std::clamp(remaining, 0.05f, 20.0f);
            changed = points ? document_->ScaleSelectedCurvePoints(transform_drag_center_, axis, step)
                : document_->PreviewScaleSelectedObjects(transform_drag_center_, axis, step);
            if (!changed) return false;
            transform_drag_scale_factor_ *= step;
            transform_drag_has_preview_ = true;
            remaining = target / transform_drag_scale_factor_;
        } while (std::fabs(remaining - 1.0f) > 1.e-6f);
        transform_drag_scale_factor_ = target;
    }
    transform_drag_has_preview_ = transform_drag_has_preview_ || changed;
    update();
    return changed;
}

void OpenGLViewport::DrawFreeTransformGuide() {
    if (!free_transform_) return;
    DomPoint center{}, end{};
    if (!renderer_.WorldToScreen(transform_drag_center_, camera_, orthographic_projection_, width(), height(), center)) return;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_LINE_STIPPLE);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = active_transform_axis_ == TransformAxis::X ? QColor(240, 90, 90)
        : active_transform_axis_ == TransformAxis::Y ? QColor(100, 210, 120)
        : active_transform_axis_ == TransformAxis::Z ? QColor(90, 150, 255) : QColor(240, 210, 90);
    painter.setPen(QPen(color, 1.5, Qt::DashLine));
    const QPointF origin(center.x, center.y);
    if (active_transform_axis_ == TransformAxis::X || active_transform_axis_ == TransformAxis::Y || active_transform_axis_ == TransformAxis::Z) {
        if (renderer_.WorldToScreen(transform_drag_center_ + transform_drag_axis_ * camera_.distance,
            camera_, orthographic_projection_, width(), height(), end)) {
            QPointF direction(end.x - center.x, end.y - center.y);
            const double length = std::hypot(direction.x(), direction.y());
            if (length > 1.0) {
                direction *= (width() + height()) / length;
                painter.drawLine(origin - direction, origin + direction);
            }
        }
    } else painter.drawLine(origin, last_mouse_);
    painter.setPen(QPen(color, 1.5));
    painter.drawEllipse(origin, 3, 3);
    painter.end();
    glPopAttrib();
}
