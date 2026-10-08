#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "ui/ToolRegistry.h"
#include <BRep_Tool.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) { std::cerr << message << std::endl; throw std::runtime_error(message); } }
CSolid& body(CAlfaDoc& doc, size_t index) {
    auto* result = dynamic_cast<CSolid*>(doc.GetObjects().at(index).get());
    check(result != nullptr, "Draft body missing");
    return *result;
}
std::vector<gp_Pnt> vertices(const TopoDS_Shape& shape) {
    std::vector<gp_Pnt> result;
    for (TopExp_Explorer it(shape, TopAbs_VERTEX); it.More(); it.Next())
        result.push_back(BRep_Tool::Pnt(TopoDS::Vertex(it.Current())));
    return result;
}
double distance(const std::vector<gp_Pnt>& a, const std::vector<gp_Pnt>& b) {
    double maximum = 0;
    for (const auto& p : a) {
        double minimum = 1.e100;
        for (const auto& q : b) minimum = std::min(minimum, p.Distance(q));
        maximum = std::max(maximum, minimum);
    }
    return maximum;
}
void same(const TopoDS_Shape& expected, const TopoDS_Shape& actual) {
    check(BRepCheck_Analyzer(actual).IsValid(), "Invalid replayed draft");
    const auto a = vertices(expected), b = vertices(actual);
    const auto delta = std::max(distance(a,b), distance(b,a));
    std::cout << "maximum vertex change=" << delta << std::endl;
    check(delta < 1.e-4, "Opening the draft editor changed the saved geometry");
    GProp_GProps first, second;
    BRepGProp::VolumeProperties(expected, first);
    BRepGProp::VolumeProperties(actual, second);
    check(std::abs(first.Mass()-second.Mass()) < std::abs(first.Mass())*1.e-6,
        "Draft replay changed volume");
}
}

int TestDraftFace(const char* path) {
    CAlfaDoc doc;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path), doc, room, view, error), "Cannot load draft fixture");
    ToolRegistry tools;
    size_t index = 0;
    while (index < doc.GetObjects().size() && !dynamic_cast<CSolid*>(doc.GetObjects()[index].get())) ++index;
    check(index < doc.GetObjects().size(), "No draft body");
    const auto body_id = body(doc,index).m_id;
    const TopoDS_Shape expected = body(doc,index).m_Shape;
    for (int i=0; i<3; ++i) {
        check(tools.ReplayOperations(index,doc), "Draft history failed to replay");
        same(expected, body(doc,index).m_Shape);
    }
    auto edit = tools.ActiveObjectFromDocument(index, body(doc,index), 2, &doc);
    auto angle = std::find_if(edit.parameters.begin(),edit.parameters.end(),[](const auto& p){return p.id=="angle";});
    check(angle != edit.parameters.end(), "Draft angle missing");
    const double saved = angle->value;
    angle->value = 0;
    tools.Rebuild(edit,doc);
    check(body(doc,index).GetNumOperations() == 3, "Zero draft angle erased operation history");
    check(tools.ReplayOperations(index,doc), "Zero draft history cannot be opened");
    angle->value = saved;
    tools.Rebuild(edit,doc);
    same(expected,body(doc,index).m_Shape);
    for (size_t operation : {size_t(0)}) {
        auto earlier = tools.ActiveObjectFromDocument(index,body(doc,index),operation,&doc);
        auto width = std::find_if(earlier.parameters.begin(),earlier.parameters.end(),[](const auto& p){return p.id=="width";});
        check(width != earlier.parameters.end(), "Earlier operation width missing");
        const auto originalWidth = width->value;
        width->value = originalWidth*1.01;
        tools.Rebuild(earlier,doc);
        check(body(doc,index).GetNumOperations()==3,"Earlier edit lost Draft Face");
        check(distance(vertices(expected),vertices(body(doc,index).m_Shape))>.01,"Earlier operation became uneditable after Draft Face");
        width->value = originalWidth;
        tools.Rebuild(earlier,doc);
        same(expected,body(doc,index).m_Shape);
    }
    angle->value = std::numeric_limits<double>::quiet_NaN();
    tools.Rebuild(edit,doc);
    check(body(doc,index).GetNumOperations() == 3, "Failed draft edit erased history");
    same(expected,body(doc,index).m_Shape);
    check(tools.ReplayOperations(index,doc), "Failed draft edit damaged saved parameters");
    same(expected,body(doc,index).m_Shape);
    angle->value = saved;
    angle->value = -saved;
    tools.Rebuild(edit,doc);
    const TopoDS_Shape negative = body(doc,index).m_Shape;
    check(distance(vertices(expected),vertices(negative)) > .1, "Changing angle sign did not change the draft");
    check(tools.ReplayOperations(index,doc), "Negative draft replay failed");
    same(negative,body(doc,index).m_Shape);
    angle->value = saved;
    tools.Rebuild(edit,doc);
    same(expected,body(doc,index).m_Shape);
    QTemporaryDir directory;
    check(directory.isValid(), "No temporary directory");
    const auto savedPath = directory.filePath("draft.dom3d");
    check(serializer.Save(savedPath,doc,room,view,{},error), "Draft save failed");
    CAlfaDoc loaded;
    check(serializer.Load(savedPath,loaded,room,view,error), "Draft reload failed");
    index=loaded.FindObjectIndexById(body_id);
    check(tools.ReplayOperations(index,loaded), "Reloaded draft replay failed");
    same(expected,body(loaded,index).m_Shape);
    return 0;
}
