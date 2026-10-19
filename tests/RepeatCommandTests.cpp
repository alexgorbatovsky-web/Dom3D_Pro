#include "ui/MainWindow.h"
#include "CAlfaDoc.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QSettings>
#include <QShortcutEvent>
#include <QStatusBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int TestRepeatCommand(int argc, char** argv) {
    std::cerr << "Initializing Repeat UI test...\n";
    QApplication application(argc, argv);
    QTemporaryDir settings_directory;
    require(settings_directory.isValid(), "Temporary settings directory missing");
    QCoreApplication::setOrganizationName("Dom3DRepeatTests");
    QCoreApplication::setApplicationName("RepeatCommandTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settings_directory.path());

    std::cerr << "Creating the main window...\n";
    MainWindow window;
    std::cerr << "Checking Repeat actions...\n";
    auto* viewport = window.findChild<OpenGLViewport*>();
    auto* repeat = window.findChild<QAction*>("RepeatLastCommandAction");
    require(viewport && repeat, "Repeat action and viewport must exist");
    require(!repeat->isEnabled(), "Repeat must be disabled before the first command");
    require(repeat->shortcut() == QKeySequence(Qt::Key_Space), "Repeat must use Space");
    require(repeat->shortcutContext() == Qt::WidgetWithChildrenShortcut
                && viewport->actions().contains(repeat),
            "Space must belong to the viewport, leaving other editors unaffected");
    require(!repeat->autoRepeat(), "Holding Space must not restart commands repeatedly");

    const auto tool_action = [&window](const QString& key) {
        for (QAction* action : window.findChildren<QAction*>()) {
            if (action->property("toolKey").toString() == key) return action;
        }
        return static_cast<QAction*>(nullptr);
    };
    QAction* curve = tool_action("PolylineCurve");
    QAction* select = tool_action("select");
    QAction* spline = tool_action("BSplineCurve");
    require(curve && select && spline, "Modeling actions must exist");
    curve->trigger();
    require(repeat->isEnabled() && viewport->CurrentTool() == ToolMode::DrawCurve,
            "Starting a curve must enable Repeat");
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(viewport, &escape);
    select->trigger();
    // Exercise QAction's shortcut-event dispatch after cancelling the command.
    QShortcutEvent space(repeat->shortcut(), 0, false);
    QApplication::sendEvent(repeat, &space);
    require(viewport->CurrentTool() == ToolMode::DrawCurve,
            "Space must restart the curve after Escape and Select");

    spline->trigger();
    select->trigger();
    std::cerr << "Checking popup Repeat...\n";
    // Popup actions are transient; the remembered callback must survive the menu.
    QTimer::singleShot(0, &window, [&]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        require(menu && !menu->actions().isEmpty() && menu->actions().first() == repeat,
                "Repeat must be the first right-click menu action");
        repeat->trigger();
        menu->close();
    });
    viewport->ViewportPopupMenuRequested(QPoint(20, 20));
    require(viewport->CurrentTool() == ToolMode::DrawBSpline,
            "Popup Repeat must restart the latest tool with its saved arguments");
    select->trigger();
    repeat->trigger();
    require(viewport->CurrentTool() == ToolMode::DrawBSpline,
            "Repeat must survive popup destruction and multiple invocations");

    // Copy starts Move internally; repeating it must still create another copy.
    std::cerr << "Checking nested Copy/Move...\n";
    curve->trigger();
    viewport->Point3DPicked(CPoint3d(0, 0, 0));
    viewport->Point3DPicked(CPoint3d(10, 0, 0));
    viewport->Point3DPickFinished();
    // Hotkey IDs are derived from translated captions; they are not UI identities.
    auto* copy = window.findChild<QAction*>("DuplicateSelectedObjectAction");
    require(copy, "Copy action must exist");
    auto* document = GetAlfaDoc();
    require(document && document->HasSelection(), "Copy fixture must be selected");
    const size_t before_copy = document->GetObjects().size();
    copy->trigger();
    require(document->GetObjects().size() == before_copy + 1 && viewport->CurrentTool() == ToolMode::Transform,
            "Copy must create geometry for the nested-command regression");
    select->trigger();
    repeat->trigger();
    require(document->GetObjects().size() == before_copy + 2 && viewport->CurrentTool() == ToolMode::Transform,
            "Repeat must preserve Copy instead of remembering its nested Move");

    std::cerr << "Checking retained array parameters...\n";
    select->trigger();
    struct ArrayValues {
        QString name;
        std::vector<int> counts;
        std::vector<double> numbers;
        int direction;
    };
    const std::vector<ArrayValues> initial{
        {"LinearArray", {2}, {-12.125}, 1},
        {"RadialArray", {3}, {-137.25, 1.125, -2.25, 3.5}, 0},
        {"RectangularArray", {2, 3}, {-4.125, 8.25}, 2}
    };
    const std::vector<ArrayValues> confirmed{
        {"LinearArray", {3}, {7.625}, 2},
        {"RadialArray", {2}, {93.75, -7.25, 4.125, -9.5}, 1},
        {"RectangularArray", {3, 2}, {9.625, -6.5}, 1}
    };
    const auto open_array = [&](const ArrayValues& expected, const ArrayValues* edit,
                                bool check, bool accept, bool use_repeat) {
        bool handled = false;
        QTimer::singleShot(0, &window, [&]() {
            auto* dialog = window.findChild<QDialog*>(expected.name + "Dialog");
            require(dialog && dialog->isVisible(),
                    "Expected array dialog missing");
            const auto counts = dialog->findChildren<QSpinBox*>();
            const auto numbers = dialog->findChildren<QDoubleSpinBox*>();
            auto* direction = dialog->findChild<QComboBox*>();
            require(counts.size() == static_cast<qsizetype>(expected.counts.size())
                        && numbers.size() == static_cast<qsizetype>(expected.numbers.size()) && direction,
                    "Array dialog fields missing");
            if (check) {
                for (qsizetype i = 0; i < counts.size(); ++i)
                    require(counts[i]->value() == expected.counts[i], "Array quantity was reset");
                for (qsizetype i = 0; i < numbers.size(); ++i)
                    require(numbers[i]->value() == expected.numbers[i], "Array numeric value was reset");
                require(direction->currentIndex() == expected.direction, "Array axis/plane was reset");
            }
            if (edit) {
                for (qsizetype i = 0; i < counts.size(); ++i) counts[i]->setValue(edit->counts[i]);
                for (qsizetype i = 0; i < numbers.size(); ++i) numbers[i]->setValue(edit->numbers[i]);
                direction->setCurrentIndex(edit->direction);
                // An Escape/Cancel can leave the current editor uncommitted.
                // Exercise that path instead of only setValue() notifications.
                auto* number = numbers.last();
                number->setKeyboardTracking(false);
                number->setValue(0);
                number->findChild<QLineEdit*>()->setText(
                    number->locale().toString(edit->numbers.back(), 'f', 3));
            }
            handled = true;
            if (accept) dialog->accept();
            else dialog->reject();
        });
        if (use_repeat) {
            repeat->trigger();
        } else {
            auto* array = window.findChild<QAction*>(expected.name + "Action");
            require(array, "Array action missing");
            array->trigger();
        }
        require(handled, "Array dialog did not open");
    };
    for (size_t i = 0; i < initial.size(); ++i) {
        open_array(initial[i], &initial[i], false, false, false);
        const size_t before_array = document->GetObjects().size();
        open_array(initial[i], &confirmed[i], true, true, false);
        int quantity = 1;
        for (int count : confirmed[i].counts) quantity *= count;
        require(document->GetObjects().size() == before_array + quantity - 1,
                "Confirmed array must still create copies");
        open_array(confirmed[i], nullptr, true, false, true);
    }
    for (const auto& values : confirmed)
        open_array(values, nullptr, true, false, false);
    std::cout << "Repeat command tests passed.\n";
    return 0;
}
