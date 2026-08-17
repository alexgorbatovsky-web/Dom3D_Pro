#include "CPart.h"

CPart::CPart(std::string name)
    : CGroup(std::move(name)) {
}

CPart::CPart(std::string name, std::vector<unsigned long> element_ids)
    : CGroup(std::move(name), std::move(element_ids)) {
}

bool CPart::IsFileLinked() const {
    return file_linked_;
}

void CPart::SetFileLinked(bool linked) {
    file_linked_ = linked;
}

const std::string& CPart::GetSourcePath() const {
    return source_path_;
}

void CPart::SetSourcePath(std::string path) {
    source_path_ = std::move(path);
}

std::unique_ptr<CAlfaObject> CPart::Clone() const {
    auto copy = std::make_unique<CPart>(GetName() + " Copy", GetElementIds());
    copy->SetFileLinked(IsFileLinked());
    copy->SetSourcePath(GetSourcePath());
    copy->SetGroupName(GetGroupName());
    copy->SetVisible(IsVisible());
    copy->CAlfaObject::SetColor(GetColor());
    copy->SetMaterial(GetMaterial());
    copy->SetMaterialId(GetMaterialId());
    copy->m_LayerID = m_LayerID;
    return copy;
}
