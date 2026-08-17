#include "UndoRedo.h"

#include <utility>

CUndoRedo::CUndoRedo(CAlfaDoc& document, std::size_t maximum_commands)
    : document_(document),
      maximum_commands_(maximum_commands == 0 ? 1 : maximum_commands) {
    Reset();
}

void CUndoRedo::Reset() {
    undo_stack_.clear();
    redo_stack_.clear();
    pending_before_.reset();
    current_ = document_.CreateSnapshot();
}

bool CUndoRedo::BeginChange() {
    pending_before_ = document_.CreateSnapshot();
    return static_cast<bool>(pending_before_);
}

bool CUndoRedo::CommitChange(std::string command_name) {
    if (!pending_before_) {
        return RecordChange(std::move(command_name));
    }

    std::shared_ptr<const CAlfaDoc::Snapshot> next = document_.CreateSnapshot();
    if (!next) {
        pending_before_.reset();
        return false;
    }
    if (undo_stack_.size() == maximum_commands_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({std::move(command_name), std::move(pending_before_), {}, {}});
    current_ = std::move(next);
    redo_stack_.clear();
    return true;
}

bool CUndoRedo::CommitChangeLazy(std::string command_name) {
    if (!pending_before_) {
        return CommitChange(std::move(command_name));
    }
    if (undo_stack_.size() == maximum_commands_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({std::move(command_name), std::move(pending_before_), {}, {}});
    // A snapshot of the live result is needed only if Undo is requested.
    current_.reset();
    redo_stack_.clear();
    return true;
}

void CUndoRedo::CancelChange() {
    if (pending_before_) {
        current_ = std::move(pending_before_);
    }
}

bool CUndoRedo::RecordChange(std::string command_name) {
    // Live tools can emit many DocumentChanged signals while a preview is
    // rebuilt. They belong to the pending command and must not create
    // intermediate undo entries.
    if (pending_before_) {
        return true;
    }
    std::shared_ptr<const CAlfaDoc::Snapshot> next = document_.CreateSnapshot();
    if (!next) {
        return false;
    }
    if (!current_) {
        current_ = std::move(next);
        redo_stack_.clear();
        return true;
    }

    if (undo_stack_.size() == maximum_commands_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({std::move(command_name), std::move(current_), {}, {}});
    current_ = std::move(next);
    redo_stack_.clear();
    return true;
}

bool CUndoRedo::RecordCommand(
    std::string command_name,
    std::function<bool(CAlfaDoc&)> undo_action,
    std::function<bool(CAlfaDoc&)> redo_action) {
    if (!undo_action || !redo_action) return false;
    pending_before_.reset();
    if (undo_stack_.size() == maximum_commands_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({
        std::move(command_name), {},
        std::move(undo_action), std::move(redo_action)});
    redo_stack_.clear();
    // The document now differs from the last full snapshot. Snapshot-based
    // commands will capture the live state lazily only when they need it.
    current_.reset();
    return true;
}

bool CUndoRedo::Undo() {
    pending_before_.reset();
    if (undo_stack_.empty()) {
        return false;
    }

    if (undo_stack_.back().IsCustom()) {
        Entry entry = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        if (!entry.undo_action(document_)) {
            undo_stack_.push_back(std::move(entry));
            return false;
        }
        redo_stack_.push_back(std::move(entry));
        current_.reset();
        return true;
    }

    if (!current_) {
        current_ = document_.CreateSnapshot();
        if (!current_) {
            return false;
        }
    }

    Entry entry = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    if (!document_.RestoreSnapshot(*entry.snapshot)) {
        undo_stack_.push_back(std::move(entry));
        return false;
    }
    redo_stack_.push_back({entry.name, std::move(current_), {}, {}});
    current_ = std::move(entry.snapshot);
    return true;
}

bool CUndoRedo::Redo() {
    pending_before_.reset();
    if (redo_stack_.empty()) {
        return false;
    }

    if (redo_stack_.back().IsCustom()) {
        Entry entry = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        if (!entry.redo_action(document_)) {
            redo_stack_.push_back(std::move(entry));
            return false;
        }
        if (undo_stack_.size() == maximum_commands_) {
            undo_stack_.erase(undo_stack_.begin());
        }
        undo_stack_.push_back(std::move(entry));
        current_.reset();
        return true;
    }
    if (!current_) {
        current_ = document_.CreateSnapshot();
        if (!current_) return false;
    }

    Entry entry = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!document_.RestoreSnapshot(*entry.snapshot)) {
        redo_stack_.push_back(std::move(entry));
        return false;
    }
    if (undo_stack_.size() == maximum_commands_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({entry.name, std::move(current_), {}, {}});
    current_ = std::move(entry.snapshot);
    return true;
}

bool CUndoRedo::CanUndo() const { return !undo_stack_.empty(); }
bool CUndoRedo::CanRedo() const { return !redo_stack_.empty(); }

std::string CUndoRedo::UndoName() const {
    return CanUndo() ? undo_stack_.back().name : std::string();
}

std::string CUndoRedo::RedoName() const {
    return CanRedo() ? redo_stack_.back().name : std::string();
}

std::size_t CUndoRedo::UndoCount() const { return undo_stack_.size(); }
std::size_t CUndoRedo::RedoCount() const { return redo_stack_.size(); }
