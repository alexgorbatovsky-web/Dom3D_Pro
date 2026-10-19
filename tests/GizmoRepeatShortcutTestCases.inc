if (application.arguments().contains("--gizmo-repeat-shortcuts")) {
    document.Clear();
    auto curve = std::make_unique<CPolyline>("Space priority fixture");
    curve->AddPoint(CPoint3d(0, 0, 0));
    curve->AddPoint(CPoint3d(40, 25, 15));
    auto* source = curve.get();
    document.AddObject(std::move(curve), false);
    const auto source_id = source->m_id;
    document.SelectObjectById(source_id);
    auto* viewport = window.viewport_;
    window.resize(1100, 800);
    window.show();
    window.activateWindow();
    viewport->FitToDocument();
    viewport->setFocus();
    require(QTest::qWaitForWindowActive(window.windowHandle(), 5000), "Keyboard test window is not active");
    QApplication::processEvents();

    auto* repeat = window.findChild<QAction*>("RepeatLastCommandAction");
    auto* copy = window.findChild<QAction*>("DuplicateSelectedObjectAction");
    require(repeat && copy, "Repeat/Copy actions missing");
    int repeat_count = 0;
    const auto repeat_connection = QObject::connect(repeat, &QAction::triggered, &window, [&] { ++repeat_count; });
    const auto command_for = [&](TransformOperation operation) {
        for (auto* action : window.findChildren<QAction*>()) {
            const auto value = action->property("dom3dTransformOperation");
            if (value.isValid() && value.toInt() == static_cast<int>(operation)) return action;
        }
        return static_cast<QAction*>(nullptr);
    };
    const auto press = [&](Qt::Key key) {
        // Send to QWindow through Qt's native key pipeline. This exercises
        // ShortcutOverride and the real shortcut map, not just keyPressEvent.
        QTest::keyClick(window.windowHandle(), key);
    };

    for (auto operation : {TransformOperation::Move, TransformOperation::Rotate, TransformOperation::Scale}) {
        for (bool free_mode : {false, true}) {
            viewport->SetTool(ToolMode::Select);
            document.SelectObjectById(source_id);
            viewport->setFocus();
            auto* command = command_for(operation);
            require(command && !command->shortcut().isEmpty(), "Transform shortcut missing");
            // Copy starts Move internally. An accidental Repeat here would
            // duplicate geometry, and numeric input must not replace Copy
            // with Move as the remembered outer command.
            if (operation == TransformOperation::Move) copy->trigger();
            else command->trigger();
            require(repeat->isEnabled(), "Repeat is disabled in priority test");
            if (free_mode) {
                QTest::keySequence(window.windowHandle(), command->shortcut());
                require(viewport->free_transform_, "Repeated hotkey did not start free mode");
                require(viewport->ApplyTransformValue(operation == TransformOperation::Scale ? 1.4 : 12), "Cannot create free preview");
            } else {
                QPoint picked;
                bool found = false;
                for (int y = 5; y < viewport->height() - 5 && !found; y += 3)
                    for (int x = 5; x < viewport->width() - 5 && !found; x += 3)
                        if (viewport->HitTestTransformGizmo(QPoint(x, y)) == TransformAxis::X) {
                            picked = {x, y}; found = true;
                        }
                require(found, "Cannot locate gizmo handle");
                viewport->HandleTransformClick(picked, false);
                viewport->HandleTransformDrag(picked + QPoint(24, -18), Qt::NoModifier);
            }
            require(viewport->dragging_transform_ && viewport->transform_drag_has_preview_, "Transform preview did not start");
            const double preview = viewport->TransformValue();
            const size_t objects = document.GetObjects().size();
            const int repeats = repeat_count;
            press(Qt::Key_Space);
            auto* editor = viewport->findChild<QLineEdit*>("TransformValueEditor");
            require(editor && editor->isVisible() && editor->hasFocus(), "Real Space shortcut did not focus numeric editor");
            require(repeat_count == repeats && document.GetObjects().size() == objects,
                    "Space repeated a command while opening numeric input");
            require(viewport->dragging_transform_ && std::fabs(viewport->TransformValue() - preview) < 1.e-5,
                    "Space reset the active transform preview");

            editor->setText("25");
            editor->setCursorPosition(editor->text().size());
            press(Qt::Key_Space);
            require(editor->text() == "25 " && editor->hasFocus() && viewport->dragging_transform_,
                    "Space in numeric editor escaped into the viewport");
            require(repeat_count == repeats && document.GetObjects().size() == objects,
                    "Space in numeric editor triggered Repeat");
            if (operation == TransformOperation::Scale) {
                editor->setText("0");
                press(Qt::Key_Return);
                require(editor->isVisible() && viewport->dragging_transform_, "Invalid numeric input committed the transform");
            }
            if (free_mode) {
                editor->setText(operation == TransformOperation::Scale ? "1.25" : "30");
                press(Qt::Key_Return);
            } else {
                press(Qt::Key_Escape);
            }
            require(!viewport->dragging_transform_ && !editor->isVisible() && viewport->hasFocus(),
                    "Numeric commit/cancel did not return focus to viewport");
            require(repeat_count == repeats, "Numeric commit/cancel repeated a command");

            press(Qt::Key_Space);
            require(repeat_count == repeats + 1, "Space did not resume Repeat after numeric input closed");
            const size_t copies_per_repeat = operation == TransformOperation::Move ? 1 : 0;
            require(document.GetObjects().size() == objects + copies_per_repeat,
                    "Numeric input replaced the remembered Copy command with Move");
            require(!viewport->dragging_transform_ && !editor->isVisible()
                && viewport->CurrentTool() == ToolMode::Transform && viewport->transform_operation_ == operation,
                "Idle Space unexpectedly started a drag or opened numeric input");
            press(Qt::Key_Escape);
            require(viewport->CurrentTool() == ToolMode::Select, "Escape did not leave idle transform tool");
            press(Qt::Key_Space);
            require(repeat_count == repeats + 2 && viewport->CurrentTool() == ToolMode::Transform
                && document.GetObjects().size() == objects + 2 * copies_per_repeat,
                "Space did not repeat the remembered command after leaving Transform");
        }
    }
    QLineEdit text_editor(viewport);
    text_editor.show();
    text_editor.setFocus();
    const int repeats = repeat_count;
    press(Qt::Key_Space);
    require(text_editor.text() == " " && repeat_count == repeats,
            "Viewport Repeat intercepted Space in an ordinary text editor");
    QObject::disconnect(repeat_connection);
    std::cout << "Native Space shortcut priority passed for Move/Rotate/Scale gizmos, free modes, numeric editors and nested Copy/Repeat\n";
    return 0;
}
