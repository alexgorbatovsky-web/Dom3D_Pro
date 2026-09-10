#include "solid/SurfaceSet.h"
#include "Net.h"
#include "CMesh3D.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BezierSurface.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TColgp_Array2OfPnt.hxx>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

size_t check_net(CSolid& solid, float tolerance)
{
    check(solid.ReBuldMesh(tolerance * 10.0f), "Display mesh build failed");
    auto* surface = solid.GetSurfaceFace(0);
    check(surface && surface->pMesh3D && !surface->IsTrimmed,
          "Natural patch was classified as trimmed");
    const auto& faces = surface->pMesh3D->GetFaces();
    check(!faces.empty(), "Empty display mesh");
    for (const auto& face : faces)
        check(face.deleted || face.corners.size() == 4,
              "Natural spline patch was triangulated");
    CNet reference;
    check(reference.Build(surface, tolerance) == 0, "Reference CNet failed");
    const auto& vertices = surface->pMesh3D->GetVertices();
    check(vertices.size() == reference.GetPoints().size(),
          "Display mesh does not use the supplied CNet tolerance");
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& point = reference.GetPoints()[i];
        check(std::abs(vertices[i].x - point.x) < 1.e-4
                  && std::abs(vertices[i].y - point.y) < 1.e-4
                  && std::abs(vertices[i].z - point.z) < 1.e-4,
              "Display vertices differ from CNet");
    }
    return faces.size();
}
}

int TestSurfaceDisplayNet()
{
    TColgp_Array2OfPnt poles(1, 4, 1, 4);
    for (int u = 1; u <= 4; ++u) {
        for (int v = 1; v <= 4; ++v) {
            const double z = (v == 2 || v == 3 ? 60.0 : 0.0)
                + (u == 2 ? 40.0 : u == 3 ? -40.0 : 0.0);
            poles.SetValue(u, v, gp_Pnt((u - 1) * 100.0, (v - 1) * 40.0, z));
        }
    }
    Handle(Geom_BezierSurface) bezier = new Geom_BezierSurface(poles);
    const Handle(Geom_Surface) surfaces[] = {
        bezier, GeomConvert::SurfaceToBSplineSurface(bezier)
    };
    for (const auto& geometry : surfaces) {
        TopoDS_Shape shape = BRepBuilderAPI_MakeFace(geometry, 1.e-7).Shape();
        CSolid solid(shape);
        check(solid.InitSurfaces() && solid.InitEdges(), "Solid initialization failed");
        const size_t coarse = check_net(solid, 2.0f);
        const size_t fine = check_net(solid, 0.125f);
        check(fine > coarse, "Smaller tolerance did not refine the net");
        check(check_net(solid, 2.0f) == coarse, "Rebuild retained the finer net");
        CSurfaceSet standalone(shape);
        check(!standalone.MeshQuadro, "Standalone display defaults to Low Poly");
        check(standalone.InitSurfaces() && standalone.InitEdges(),
              "Surface initialization failed");
        check_net(standalone, 0.125f);
        std::cout << "Natural spline display: " << coarse << " -> " << fine << " quads\n";
    }
    std::cout << "Surface display CNet tests passed\n";
    return EXIT_SUCCESS;
}


#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include <BRepAdaptor_Surface.hxx>
#include <TopoDS.hxx>
#include <gp_Sphere.hxx>

void TestFilletMeshNormals(const char* path)
{
    CAlfaDoc doc; Dom3DProjectSerializer serializer;
    QString room,error; ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(path),doc,room,view,error),"Cannot load fillet fixture");
    int spherical=0, failures=0, triangulated=0;
    for(const auto& object:doc.GetObjects()) {
        auto* solid=dynamic_cast<CSolid*>(object.get());
        if(!solid)continue;
        check(solid->InitSurfaces(),"Cannot initialize fillet faces");
        solid->MeshQuadro=false;
        for(float deflection:{0.25f,1.f,5.f}) {
        check(solid->ReBuldMesh(deflection),"Cannot rebuild hybrid fillet mesh");
        for(int i=0;i<solid->GetNumSurfaces();++i) {
            auto* face=solid->GetSurfaceFace(i);
            BRepAdaptor_Surface cad(TopoDS::Face(face->m_Face));
            if(cad.GetType()!=GeomAbs_Sphere)continue;
            ++spherical;
            const auto& mesh=*face->pMesh3D;
            int wrong=0,checked=0;
            for(const auto& cell:mesh.GetFaces()) {
                if(cell.deleted||cell.corners.size()!=3)continue;
                const auto a=mesh.GetVertices()[cell.corners[0].v];
                const auto b=mesh.GetVertices()[cell.corners[1].v];
                const auto c=mesh.GetVertices()[cell.corners[2].v];
                const gp_Vec n=gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(b.x,b.y,b.z)).Crossed(gp_Vec(gp_Pnt(a.x,a.y,a.z),gp_Pnt(c.x,c.y,c.z)));
                if(n.SquareMagnitude()<1.e-20)continue;
                for(const auto& corner:cell.corners) {
                    check(corner.n<mesh.GetNormals().size(),"Missing corner normal");
                    const auto normal=mesh.GetNormals()[corner.n];
                    ++checked;
                    if(n.Dot(gp_Vec(normal.x,normal.y,normal.z))<=0)++wrong;
                }
            }
            std::cout<<"sphere face="<<i<<" direct="<<cad.Sphere().Position().Direct()<<" reversed="<<(face->m_Face.Orientation()==TopAbs_REVERSED)<<" checked="<<checked<<" wrong="<<wrong<<std::endl;
            failures+=wrong;
            if(checked>0)++triangulated;
        }
    }
    }
    check(spherical>=4&&triangulated>=12,"Missing triangulated rounded corners in fixture");
    check(failures==0,"Fillet normals oppose triangle winding");
}
