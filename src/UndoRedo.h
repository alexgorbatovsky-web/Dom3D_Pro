#pragma once

#include "CAlfaDoc.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Document-level undo/redo history. Commands are stored as immutable
// snapshots so the history is independent of object lifetimes in CAlfaDoc.
class CUndoRedo {
public:
    explicit CUndoRedo(CAlfaDoc& document, std::size_t maximum_commands = 100);

    void Reset();
    bool BeginChange();
    bool CommitChange(std::string command_name);
    // Commits the undo state without cloning the (potentially very large)
    // resulting document.  The redo snapshot is captured on the first Undo.
    bool CommitChangeLazy(std::string command_name);
    void CancelChange();
    bool RecordChange(std::string command_name);
    bool RecordCommand(
        std::string command_name,
        std::function<bool(CAlfaDoc&)> undo_action,
        std::function<bool(CAlfaDoc&)> redo_action);
    bool Undo();
    bool Redo();

    bool CanUndo() const;
    bool CanRedo() const;
    std::string UndoName() const;
    std::string RedoName() const;
    std::size_t UndoCount() const;
    std::size_t RedoCount() const;

private:
    struct Entry {
        std::string name;
        std::shared_ptr<const CAlfaDoc::Snapshot> snapshot;
        std::function<bool(CAlfaDoc&)> undo_action;
        std::function<bool(CAlfaDoc&)> redo_action;

        bool IsCustom() const {
            return static_cast<bool>(undo_action)
                && static_cast<bool>(redo_action);
        }
    };

    CAlfaDoc& document_;
    std::size_t maximum_commands_ = 100;
    std::shared_ptr<const CAlfaDoc::Snapshot> current_;
    std::shared_ptr<const CAlfaDoc::Snapshot> pending_before_;
    std::vector<Entry> undo_stack_;
    std::vector<Entry> redo_stack_;
};
