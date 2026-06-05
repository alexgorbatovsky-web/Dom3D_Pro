#include "Dom3DProjectSerializer.h"

#include "CMesh3D.h"
#include "CPolyline.h"
#include "solid/Solid.h"

#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>

#include <QByteArray>
#include <QDomDocument>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStringList>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr const char* kProjectVersion = "1";

QString object_type_name(const CAlfaObject& object) {
    if (dynamic_cast<const CSolid*>(&object)) {
        return "Solid";
    }
    if (dynamic_cast<const CMesh3D*>(&object)) {
        return "Mesh";
    }
    if (dynamic_cast<const CPolyline*>(&object)) {
        return "Curve";
    }
    return "Object";
}

QString default_room_name(const CAlfaObject& object) {
    if (dynamic_cast<const CSolid*>(&object)) {
        return "Solid";
    }
    if (dynamic_cast<const CMesh3D*>(&object)) {
        return "Surfaces";
    }
    if (dynamic_cast<const CPolyline*>(&object)) {
        return "Lines";
    }
    return "Architecture";
}

QString operation_name_for_object(const CAlfaObject& object) {
    const QString name = QString::fromStdString(object.GetName());
    if (name.startsWith("Boolean Union")) {
        return "BooleanUnion";
    }
    if (name.startsWith("Boolean Cut")) {
        return "BooleanCut";
    }
    if (name.startsWith("Boolean Common")) {
        return "BooleanCommon";
    }
    if (name.startsWith("Fillet")) {
        return "Fillet";
    }
    return {};
}

bool read_float_attr(const QDomElement& element, const char* name, float& value, QString& error, bool required = true) {
    if (!element.hasAttribute(name)) {
        if (required) {
            error = QString("Missing XML attribute '%1'.").arg(name);
            return false;
        }
        return true;
    }

    bool ok = false;
    const float parsed = element.attribute(name).toFloat(&ok);
    if (!ok) {
        error = QString("Unsupported float value in XML attribute '%1'.").arg(name);
        return false;
    }
    value = parsed;
    return true;
}

bool read_size_attr(const QDomElement& element, const char* name, size_t& value, QString& error, bool required = true) {
    if (!element.hasAttribute(name)) {
        if (required) {
            error = QString("Missing XML attribute '%1'.").arg(name);
            return false;
        }
        return true;
    }

    bool ok = false;
    const quint64 parsed = element.attribute(name).toULongLong(&ok);
    if (!ok) {
        error = QString("Unsupported integer value in XML attribute '%1'.").arg(name);
        return false;
    }
    value = static_cast<size_t>(parsed);
    return true;
}

void write_material(QXmlStreamWriter& xml, const Material& material) {
    xml.writeStartElement("material");
    xml.writeAttribute("r", QString::number(material.diffuse.r, 'g', 9));
    xml.writeAttribute("g", QString::number(material.diffuse.g, 'g', 9));
    xml.writeAttribute("b", QString::number(material.diffuse.b, 'g', 9));
    xml.writeAttribute("alpha", QString::number(material.alpha, 'g', 9));
    xml.writeAttribute("specular", QString::number(material.specular, 'g', 9));
    xml.writeAttribute("shininess", QString::number(material.shininess, 'g', 9));
    xml.writeEndElement();
}

bool read_material(const QDomElement& object_element, CAlfaObject& object, QString& error) {
    const QDomElement material_element = object_element.firstChildElement("material");
    if (material_element.isNull()) {
        return true;
    }

    Material material = object.GetMaterial();
    if (!read_float_attr(material_element, "r", material.diffuse.r, error, false)
        || !read_float_attr(material_element, "g", material.diffuse.g, error, false)
        || !read_float_attr(material_element, "b", material.diffuse.b, error, false)
        || !read_float_attr(material_element, "alpha", material.alpha, error, false)
        || !read_float_attr(material_element, "specular", material.specular, error, false)
        || !read_float_attr(material_element, "shininess", material.shininess, error, false)) {
        return false;
    }

    object.SetMaterial(material);
    return true;
}

void write_identity_transform(QXmlStreamWriter& xml) {
    xml.writeStartElement("transform");

    xml.writeStartElement("position");
    xml.writeAttribute("x", "0");
    xml.writeAttribute("y", "0");
    xml.writeAttribute("z", "0");
    xml.writeEndElement();

    xml.writeStartElement("rotation");
    xml.writeAttribute("x", "0");
    xml.writeAttribute("y", "0");
    xml.writeAttribute("z", "0");
    xml.writeEndElement();

    xml.writeStartElement("scale");
    xml.writeAttribute("x", "1");
    xml.writeAttribute("y", "1");
    xml.writeAttribute("z", "1");
    xml.writeEndElement();

    xml.writeEndElement();
}

void write_parameters(QXmlStreamWriter& xml, const CAlfaObject& object) {
    const Material material = object.GetMaterial();
    xml.writeStartElement("parameters");
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.r");
    xml.writeAttribute("value", QString::number(material.diffuse.r, 'g', 9));
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.g");
    xml.writeAttribute("value", QString::number(material.diffuse.g, 'g', 9));
    xml.writeEmptyElement("parameter");
    xml.writeAttribute("name", "color.b");
    xml.writeAttribute("value", QString::number(material.diffuse.b, 'g', 9));
    xml.writeEndElement();
}

void write_operation_history(QXmlStreamWriter& xml, const CAlfaObject& object) {
    xml.writeStartElement("operationHistory");
    const QString operation_name = operation_name_for_object(object);
    if (!operation_name.isEmpty()) {
        xml.writeEmptyElement("operation");
        xml.writeAttribute("type", operation_name);
        xml.writeAttribute("status", "baked");
    }
    xml.writeEndElement();
}

QDomElement required_child(const QDomElement& parent, const char* name, QString& error) {
    const QDomElement child = parent.firstChildElement(name);
    if (child.isNull()) {
        error = QString("Missing XML element '%1'.").arg(name);
    }
    return child;
}
}

bool Dom3DProjectSerializer::Save(const QString& path, const CAlfaDoc& document, const QString& active_room, QString& error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument("1.0");
    xml.writeStartElement("dom3dProject");
    xml.writeAttribute("version", kProjectVersion);

    xml.writeStartElement("metadata");
    xml.writeTextElement("activeRoom", active_room);
    xml.writeEndElement();

    xml.writeStartElement("objects");
    const auto& objects = document.GetObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
        const CAlfaObject& object = *objects[i];
        const QString type = object_type_name(object);

        xml.writeStartElement("object");
        xml.writeAttribute("id", QString::number(i + 1));
        xml.writeAttribute("type", type);
        xml.writeAttribute("name", QString::fromStdString(object.GetName()));
        xml.writeAttribute("room", default_room_name(object));
        if (!object.GetGroupName().empty()) {
            xml.writeAttribute("group", QString::fromStdString(object.GetGroupName()));
        }
        xml.writeAttribute("visible", object.IsVisible() ? "true" : "false");

        write_material(xml, object.GetMaterial());
        write_identity_transform(xml);
        write_parameters(xml, object);
        write_operation_history(xml, object);

        if (const auto* polyline = dynamic_cast<const CPolyline*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "polyline");
            for (const CurvePoint& point : polyline->GetPoints()) {
                xml.writeEmptyElement("point");
                xml.writeAttribute("x", QString::number(point.x, 'g', 9));
                xml.writeAttribute("z", QString::number(point.z, 'g', 9));
            }
            xml.writeEndElement();
        } else if (const auto* mesh = dynamic_cast<const CMesh3D*>(&object)) {
            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "mesh");
            xml.writeStartElement("vertices");
            for (const Vec3& vertex : mesh->GetVertices()) {
                xml.writeEmptyElement("vertex");
                xml.writeAttribute("x", QString::number(vertex.x, 'g', 9));
                xml.writeAttribute("y", QString::number(vertex.y, 'g', 9));
                xml.writeAttribute("z", QString::number(vertex.z, 'g', 9));
            }
            xml.writeEndElement();

            xml.writeStartElement("faces");
            for (const CMesh3D::Face& face : mesh->GetFaces()) {
                QStringList indices;
                for (size_t index : face) {
                    indices.push_back(QString::number(index));
                }
                xml.writeTextElement("face", indices.join(' '));
            }
            xml.writeEndElement();
            xml.writeEndElement();
        } else if (const auto* solid = dynamic_cast<const CSolid*>(&object)) {
            QByteArray step_data;
            if (!SaveSolidStep(*solid, step_data, error)) {
                return false;
            }

            xml.writeStartElement("geometry");
            xml.writeAttribute("kind", "brep-step");
            xml.writeAttribute("encoding", "base64");
            xml.writeCharacters(QString::fromLatin1(step_data.toBase64()));
            xml.writeEndElement();
        }

        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();

    if (xml.hasError()) {
        error = "Could not write XML project.";
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool Dom3DProjectSerializer::Load(const QString& path, CAlfaDoc& document, QString& active_room, QString& error) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }

    QDomDocument dom;
    QString parse_error;
    int error_line = 0;
    int error_column = 0;
    if (!dom.setContent(&file, &parse_error, &error_line, &error_column)) {
        error = QString("XML parse error at line %1, column %2: %3").arg(error_line).arg(error_column).arg(parse_error);
        return false;
    }

    const QDomElement root = dom.documentElement();
    if (root.tagName() != "dom3dProject") {
        error = "Unsupported XML root element.";
        return false;
    }
    if (root.attribute("version").isEmpty()) {
        error = "Missing project version.";
        return false;
    }
    if (root.attribute("version") != kProjectVersion) {
        error = QString("Unsupported Dom3D project version '%1'.").arg(root.attribute("version"));
        return false;
    }

    const QDomElement metadata = root.firstChildElement("metadata");
    active_room = metadata.firstChildElement("activeRoom").text();
    if (active_room.isEmpty()) {
        active_room = "Architecture";
    }

    const QDomElement objects_element = required_child(root, "objects", error);
    if (objects_element.isNull()) {
        return false;
    }

    CAlfaDoc::ObjectList loaded_objects;
    for (QDomElement object_element = objects_element.firstChildElement("object");
         !object_element.isNull();
         object_element = object_element.nextSiblingElement("object")) {
        const QString type = object_element.attribute("type");
        if (type.isEmpty()) {
            error = "Object is missing type.";
            return false;
        }

        std::unique_ptr<CAlfaObject> object;
        if (type == "Curve") {
            auto polyline = std::make_unique<CPolyline>(object_element.attribute("name", "Curve").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }

            for (QDomElement point_element = geometry.firstChildElement("point");
                 !point_element.isNull();
                 point_element = point_element.nextSiblingElement("point")) {
                CurvePoint point{};
                if (!read_float_attr(point_element, "x", point.x, error)
                    || !read_float_attr(point_element, "z", point.z, error)) {
                    return false;
                }
                polyline->AddPoint(point);
            }
            object = std::move(polyline);
        } else if (type == "Mesh") {
            auto mesh = std::make_unique<CMesh3D>(object_element.attribute("name", "Mesh3D").toStdString());
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }

            std::vector<Vec3> vertices;
            const QDomElement vertices_element = required_child(geometry, "vertices", error);
            if (vertices_element.isNull()) {
                return false;
            }
            for (QDomElement vertex_element = vertices_element.firstChildElement("vertex");
                 !vertex_element.isNull();
                 vertex_element = vertex_element.nextSiblingElement("vertex")) {
                Vec3 vertex{};
                if (!read_float_attr(vertex_element, "x", vertex.x, error)
                    || !read_float_attr(vertex_element, "y", vertex.y, error)
                    || !read_float_attr(vertex_element, "z", vertex.z, error)) {
                    return false;
                }
                vertices.push_back(vertex);
            }

            std::vector<CMesh3D::Face> faces;
            const QDomElement faces_element = required_child(geometry, "faces", error);
            if (faces_element.isNull()) {
                return false;
            }
            for (QDomElement face_element = faces_element.firstChildElement("face");
                 !face_element.isNull();
                 face_element = face_element.nextSiblingElement("face")) {
                CMesh3D::Face face;
                const QStringList parts = face_element.text().split(' ', Qt::SkipEmptyParts);
                for (const QString& part : parts) {
                    bool ok = false;
                    const quint64 index = part.toULongLong(&ok);
                    if (!ok) {
                        error = "Mesh face contains an unsupported vertex index.";
                        return false;
                    }
                    face.push_back(static_cast<size_t>(index));
                }
                if (face.size() < 3) {
                    error = "Mesh face has fewer than 3 vertices.";
                    return false;
                }
                faces.push_back(std::move(face));
            }

            if (!mesh->SetGeometry(std::move(vertices), std::move(faces))) {
                error = "Mesh geometry is invalid.";
                return false;
            }
            object = std::move(mesh);
        } else if (type == "Solid") {
            const QDomElement geometry = required_child(object_element, "geometry", error);
            if (geometry.isNull()) {
                return false;
            }
            if (geometry.attribute("kind") != "brep-step" || geometry.attribute("encoding") != "base64") {
                error = "Unsupported solid geometry encoding.";
                return false;
            }

            std::unique_ptr<CSolid> solid;
            const QByteArray step_data = QByteArray::fromBase64(geometry.text().toLatin1());
            if (step_data.isEmpty() || !LoadSolidStep(step_data, solid, error)) {
                return false;
            }
            solid->SetName(object_element.attribute("name", "Solid").toStdString());
            object = std::move(solid);
        } else {
            error = QString("Unsupported object type '%1'.").arg(type);
            return false;
        }

        if (!read_material(object_element, *object, error)) {
            return false;
        }
        object->SetGroupName(object_element.attribute("group").toStdString());
        object->SetVisible(object_element.attribute("visible", "true") != "false");
        loaded_objects.push_back(std::move(object));
    }

    document.Clear();
    document.GetObjects() = std::move(loaded_objects);
    if (document.GetObjects().empty()) {
        document.CreatePolyline();
    }
    document.ClearSelection();
    return true;
}

bool Dom3DProjectSerializer::SaveSolidStep(const CSolid& solid, QByteArray& step_data, QString& error) const {
    if (solid.m_Shape.IsNull()) {
        error = "Solid has no BRep shape.";
        return false;
    }

    QTemporaryFile temp_file(QDir::tempPath() + "/Dom3DProjectSolidXXXXXX.step");
    temp_file.setAutoRemove(true);
    if (!temp_file.open()) {
        error = temp_file.errorString();
        return false;
    }
    const QString temp_path = temp_file.fileName();
    temp_file.close();

    try {
        STEPControl_Writer writer;
        const IFSelect_ReturnStatus transfer_status = writer.Transfer(solid.m_Shape, STEPControl_AsIs);
        if (transfer_status != IFSelect_RetDone) {
            error = "Could not transfer solid shape to STEP.";
            return false;
        }
        const IFSelect_ReturnStatus write_status = writer.Write(temp_path.toLocal8Bit().constData());
        if (write_status != IFSelect_RetDone) {
            error = "Could not write temporary STEP data.";
            return false;
        }
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.isEmpty()) {
            error = "OpenCascade failed while saving solid STEP data.";
        }
        return false;
    }

    QFile step_file(temp_path);
    if (!step_file.open(QIODevice::ReadOnly)) {
        error = step_file.errorString();
        return false;
    }
    step_data = step_file.readAll();
    return !step_data.isEmpty();
}

bool Dom3DProjectSerializer::LoadSolidStep(const QByteArray& step_data, std::unique_ptr<CSolid>& solid, QString& error) const {
    QTemporaryFile temp_file(QDir::tempPath() + "/Dom3DProjectSolidXXXXXX.step");
    temp_file.setAutoRemove(true);
    if (!temp_file.open()) {
        error = temp_file.errorString();
        return false;
    }
    if (temp_file.write(step_data) != step_data.size()) {
        error = temp_file.errorString();
        return false;
    }
    temp_file.flush();

    try {
        STEPControl_Reader reader;
        const IFSelect_ReturnStatus read_status = reader.ReadFile(temp_file.fileName().toLocal8Bit().constData());
        if (read_status != IFSelect_RetDone) {
            error = "Could not read STEP data from XML project.";
            return false;
        }

        const Standard_Integer transferred = reader.TransferRoots();
        if (transferred <= 0) {
            error = "STEP data does not contain transferable shapes.";
            return false;
        }

        TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            error = "STEP data does not contain a valid shape.";
            return false;
        }

        auto loaded = std::make_unique<CSolid>(shape);
        loaded->InitSurfaces();
        loaded->InitEdges();
        loaded->BuldMesh(0.1f);
        solid = std::move(loaded);
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.isEmpty()) {
            error = "OpenCascade failed while loading solid STEP data.";
        }
        return false;
    }
}
