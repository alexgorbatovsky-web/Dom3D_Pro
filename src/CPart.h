#pragma once

#include "CGroup.h"

#include <string>

// A document-level container created when a project or catalog item is
// inserted into another document.  It behaves like a group for selection and
// transforms while retaining the optional source-file relationship.
class CPart : public CGroup {
public:
    explicit CPart(std::string name = "Part");
    CPart(std::string name, std::vector<unsigned long> element_ids);

    bool IsFileLinked() const;
    void SetFileLinked(bool linked);
    const std::string& GetSourcePath() const;
    void SetSourcePath(std::string path);

    std::unique_ptr<CAlfaObject> Clone() const override;

private:
    bool file_linked_ = false;
    std::string source_path_;
};
