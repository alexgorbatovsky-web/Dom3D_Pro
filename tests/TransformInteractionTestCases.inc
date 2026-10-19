if (application.arguments().contains("--transform-interaction")) {
    document.Clear();
    auto shape = BRepPrimAPI_MakeBox(40, 25, 15).Shape();
    auto body = std::make_unique<CSolid>(shape);
    require(body->ReBuldMesh(), "Cannot mesh transform fixture");
    auto* fixture = body.get();
    document.AddObject(std::move(body), false);
    const auto id = fixture->m_id;
    document.SelectObjectById(id);
    auto* viewport = window.viewport_;
    window.resize(1100, 800);
    window.show();
    window.activateWindow();
    viewport->FitToDocument();
    viewport->setFocus();
    QApplication::processEvents();
    window.undo_redo_.Reset();
    const auto solid = [&] { return dynamic_cast<CSolid*>(document.FindObjectById(id)); };
    const auto center = [&] {
        GProp_GProps props;
        BRepGProp::VolumeProperties(solid()->m_Shape, props);
        return props.CentreOfMass();
    };
    const auto volume = [&] {
        GProp_GProps props;
        BRepGProp::VolumeProperties(solid()->m_Shape, props);
        return props.Mass();
    };
    const auto key = [&](QWidget* target, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent event(QEvent::KeyPress, code, modifiers);
        QApplication::sendEvent(target, &event);
    };
    const auto mouse = [&](QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(pos), QPointF(viewport->mapToGlobal(pos)), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(viewport, &event);
    };
    const auto action = [&](TransformOperation operation) {
        for (auto* candidate : window.findChildren<QAction*>()) {
            const auto property = candidate->property("dom3dTransformOperation");
            if (property.isValid() && property.toInt() == static_cast<int>(operation)) return candidate;
        }
        return static_cast<QAction*>(nullptr);
    };
    const auto shortcut = [&](TransformOperation operation) {
        auto* command = action(operation);
        require(command && !command->shortcut().isEmpty(), "Missing default transform shortcut");
        QShortcutEvent event(command->shortcut(), 0, false);
        QApplication::sendEvent(command, &event);
    };
    const auto begin = [&](TransformOperation operation, TransformAxis axis) {
        window.BeginTransformTool(operation);
        QPoint picked;
        bool found = false;
        for (int y = 5; y < viewport->height() - 5 && !found; y += 3)
            for (int x = 5; x < viewport->width() - 5 && !found; x += 3)
                if (viewport->HitTestTransformGizmo(QPoint(x, y)) == axis) { picked = {x, y}; found = true; }
        require(found, "Gizmo handle missing");
        mouse(QEvent::MouseButtonPress, picked, Qt::LeftButton, Qt::LeftButton);
        require(viewport->dragging_transform_, "Gizmo press did not start transform");
        mouse(QEvent::MouseMove, picked + QPoint(24, -18), Qt::NoButton, Qt::LeftButton);
        return picked + QPoint(24, -18);
    };
    const auto numeric = [&](QString value) {
        key(viewport, Qt::Key_Space);
        require(viewport->transform_value_popup_ && viewport->transform_value_popup_->isVisible(), "Space did not open numeric input");
        viewport->transform_value_editor_->setText(value);
        key(viewport->transform_value_editor_, Qt::Key_Return);
        require(!viewport->dragging_transform_ && !viewport->transform_value_popup_->isVisible(), "Numeric Enter did not finish transform");
    };

    const auto start = center();
    const QPoint drag = begin(TransformOperation::Move, TransformAxis::X);
    const double dragged = viewport->TransformValue();
    require(std::fabs(dragged) > .001, "Move preview did not accumulate");
    key(viewport, Qt::Key_Space);
    require(viewport->transform_value_editor_->hasFocus(), "Numeric editor has no keyboard focus");
    bool parsed = false;
    const double displayed = NumberInputLocale().toDouble(viewport->transform_value_editor_->text(), &parsed);
    require(parsed && std::fabs(DisplayToMillimeters(displayed, LoadDisplayLengthUnit()) - dragged) < 1.e-4, "Editor did not show accumulated distance");
    mouse(QEvent::MouseButtonRelease, drag, Qt::LeftButton, Qt::NoButton);
    mouse(QEvent::MouseMove, drag + QPoint(90, 90), Qt::NoButton, Qt::NoButton);
    require(viewport->dragging_transform_ && std::fabs(viewport->TransformValue() - dragged) < 1.e-5, "Numeric input did not freeze mouse/release");
    viewport->transform_value_editor_->setText("-12.5");
    key(viewport->transform_value_editor_, Qt::Key_Return);
    const double distance = DisplayToMillimeters(-12.5, LoadDisplayLengthUnit());
    require(std::fabs(center().X() - start.X() - distance) < .001 && std::fabs(center().Y() - start.Y()) < .001,
            "Numeric Move added to preview instead of replacing total distance");
    require(window.undo_redo_.Undo() && center().Distance(start) < .001, "Numeric Move Undo failed");
    require(window.undo_redo_.Redo() && std::fabs(center().X() - start.X() - distance) < .001, "Numeric Move Redo failed");
    require(window.undo_redo_.Undo(), "Cannot restore fixture");
    document.SelectObjectById(id);

    const double initial_volume = volume();
    begin(TransformOperation::Scale, TransformAxis::UniformScale);
    numeric("2");
    require(std::fabs(volume() / initial_volume - 8) < 1.e-4, "Uniform numeric scale does not replace preview");
    begin(TransformOperation::Scale, TransformAxis::X);
    numeric("0,5");
    require(std::fabs(volume() / initial_volume - 4) < 1.e-4, "Axis scale or comma decimal input failed");
    begin(TransformOperation::Rotate, TransformAxis::Z);
    numeric("90");
    Vec3 lo{}, hi{};
    solid()->GetBounds(lo, hi);
    require(std::fabs((hi.x - lo.x) - 50) < .01 && std::fabs((hi.y - lo.y) - 40) < .01,
            "Numeric rotation did not commit exact total angle");

    // Real QAction shortcut events include custom hotkeys and preserve command identity.
    for (auto operation : {TransformOperation::Move, TransformOperation::Rotate, TransformOperation::Scale}) {
        viewport->SetTool(ToolMode::Select);
        viewport->setFocus();
        shortcut(operation);
        require(!viewport->free_transform_ && viewport->transform_operation_ == operation, "First hotkey must show gizmo");
        QCursor::setPos(viewport->mapToGlobal(viewport->rect().center() + QPoint(90, 0)));
        shortcut(operation);
        require(viewport->free_transform_ && viewport->dragging_transform_, "Repeated hotkey did not start free transform");
        const auto expected = operation == TransformOperation::Scale ? TransformAxis::UniformScale : TransformAxis::ScreenPlane;
        require(viewport->active_transform_axis_ == expected, "Wrong unconstrained transform mode");
        const auto anchor = viewport->last_mouse_;
        mouse(QEvent::MouseMove, anchor + QPoint(22, -35), Qt::NoButton, Qt::NoButton);
        require(viewport->transform_drag_has_preview_, "Free transform requires mouse button");
        if (operation == TransformOperation::Rotate) {
            const Vec3 view_axis = rotate(viewport->GetCamera().orientation, {0, 0, 1});
            require(std::fabs(dot(view_axis, viewport->transform_drag_axis_)) > .999f, "Free rotation axis is not perpendicular to screen");
        }
        key(viewport, Qt::Key_Escape);
        require(!viewport->dragging_transform_ && viewport->CurrentTool() == ToolMode::Transform, "Escape did not cancel current transform");
        for (auto pair : {std::pair<int, TransformAxis>{Qt::Key_X, TransformAxis::X},
                          {Qt::Key_Y, TransformAxis::Y}, {Qt::Key_Z, TransformAxis::Z}}) {
            key(viewport, pair.first);
            require(viewport->free_transform_ && viewport->active_transform_axis_ == pair.second, "Axis key did not constrain transform");
            require(viewport->ApplyTransformValue(operation == TransformOperation::Scale ? 1.2 : 12), "Constrained preview failed");
        }
        key(viewport, Qt::Key_Escape);
        Vec3 restored_lo{}, restored_hi{};
        solid()->GetBounds(restored_lo, restored_hi);
        require(dot(restored_lo - lo, restored_lo - lo) < .001 && dot(restored_hi - hi, restored_hi - hi) < .001,
                "Axis switching or cancellation left transformed geometry");
    }
    action(TransformOperation::Move)->setShortcut(QKeySequence(Qt::Key_G));
    viewport->SetTool(ToolMode::Select);
    shortcut(TransformOperation::Move);
    shortcut(TransformOperation::Move);
    require(viewport->free_transform_, "Remapped shortcut cannot enter free mode");
    key(viewport, Qt::Key_Escape);

    // Front-on screen rotation and Ctrl snapping, including modifier-only updates.
    window.BeginTransformTool(TransformOperation::Rotate);
    viewport->BeginFreeTransform();
    DomPoint pivot{};
    require(viewport->renderer_.WorldToScreen(viewport->transform_drag_center_, viewport->camera_, viewport->orthographic_projection_,
        viewport->width(), viewport->height(), pivot), "Cannot project rotation center");
    viewport->last_mouse_ = QPoint(pivot.x + 100, pivot.y);
    const QPoint rotated(pivot.x + 77, pivot.y - 64); // Approximately 40 degrees.
    viewport->HandleTransformDrag(rotated, Qt::ControlModifier);
    require(std::fabs(viewport->TransformValue() - 45) < .01, "Ctrl rotation did not snap to 45 degrees");
    QKeyEvent release_ctrl(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
    QApplication::sendEvent(viewport, &release_ctrl);
    require(viewport->TransformValue() > 38 && viewport->TransformValue() < 42, "Ctrl release did not restore continuous angle");
    key(viewport, Qt::Key_Control, Qt::ControlModifier);
    require(std::fabs(viewport->TransformValue() - 45) < .01, "Ctrl press without motion did not snap");
    key(viewport, Qt::Key_Escape);

    window.BeginTransformTool(TransformOperation::Scale);
    key(viewport, Qt::Key_X);
    key(viewport, Qt::Key_Space);
    viewport->transform_value_editor_->setText("0");
    key(viewport->transform_value_editor_, Qt::Key_Return);
    require(viewport->dragging_transform_ && viewport->transform_value_popup_->isVisible(), "Invalid scale was committed");
    key(viewport->transform_value_editor_, Qt::Key_Escape);
    require(!viewport->dragging_transform_ && !viewport->transform_value_popup_->isVisible(), "Numeric Escape did not cancel");

    window.BeginTransformTool(TransformOperation::Move);
    key(viewport, Qt::Key_X);
    require(viewport->ApplyTransformValue(7), "Cannot preview final click fixture");
    const auto before_click = center();
    mouse(QEvent::MouseButtonPress, viewport->last_mouse_, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, viewport->last_mouse_, Qt::LeftButton, Qt::NoButton);
    require(!viewport->dragging_transform_ && std::fabs(center().X() - before_click.X() - 7) < .001, "Click did not commit free transform");
    key(viewport, Qt::Key_Y);
    require(viewport->ApplyTransformValue(13), "Cannot preview cancel fixture");
    window.BeginTransformTool(TransformOperation::Scale);
    solid()->GetBounds(lo, hi);
    require(std::fabs((lo.y + hi.y) * .5 - center().Y()) < .001, "Tool switch left uncommitted preview");

    const int output = application.arguments().indexOf("--output");
    if (output >= 0 && output + 1 < application.arguments().size()) {
        begin(TransformOperation::Move, TransformAxis::X);
        key(viewport, Qt::Key_Space);
        QApplication::processEvents();
        viewport->grab().save(application.arguments()[output + 1]);
        key(viewport->transform_value_editor_, Qt::Key_Escape);
    }
    // The same interaction also transforms selected curve points and leaves
    // the original points intact when a preview is canceled.
    viewport->SetTool(ToolMode::Select);
    document.Clear();
    auto curve = std::make_unique<CPolyline>("Transform points");
    curve->AddPoint(CPoint3d(10, 0, 0));
    curve->AddPoint(CPoint3d(0, 10, 0));
    curve->AddPoint(CPoint3d(-10, 0, 0));
    auto* line = curve.get();
    document.AddObject(std::move(curve), false);
    document.SelectObjectById(line->m_id);
    require(document.SelectAllPointsOfSelectedCurve(), "Cannot select curve point fixture");
    viewport->selection_mode_ = SelectionMode::Point;
    window.undo_redo_.Reset();
    window.BeginTransformTool(TransformOperation::Move);
    key(viewport, Qt::Key_X);
    numeric("8");
    require(std::fabs(line->GetPoints()[0].x - 10 - DisplayToMillimeters(8, LoadDisplayLengthUnit())) < .001,
            "Numeric Move did not update selected points");
    const auto before_points = line->GetPoints();
    for (auto operation : {TransformOperation::Rotate, TransformOperation::Scale}) {
        window.BeginTransformTool(operation);
        key(viewport, operation == TransformOperation::Rotate ? Qt::Key_Z : Qt::Key_X);
        require(viewport->ApplyTransformValue(operation == TransformOperation::Rotate ? 35 : 1.8), "Point preview failed");
        key(viewport, Qt::Key_Escape);
        for (size_t index = 0; index < before_points.size(); ++index) {
            const auto& a = before_points[index];
            const auto& b = line->GetPoints()[index];
            require(std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z) < .001,
                    "Cancel did not restore curve points");
        }
    }
    std::cout << "Transform gizmo numeric input, shortcut modes, XYZ, Ctrl snap, cancellation and undo/redo passed\n";
    return 0;
}
