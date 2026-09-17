#include "mesh/ImageRelief.h"
#include "ui/ImageReliefDialog.h"
#include "ui/LanguageManager.h"
#include "ui/MainWindow.h"
#include <QTimer>
#include <QTemporaryDir>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QFile>
#include "solid/Solid.h"
#include <QApplication>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFont>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BezierSurface.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <iostream>
#include <map>
#include <cmath>
#include <stdexcept>

int TestImageRelief() {
    int argc=1;char name[]="ImageReliefTests";char* argv[]={name,nullptr};
    QApplication app(argc,argv);
    QTemporaryDir settings_directory;
    QCoreApplication::setOrganizationName("ImageReliefTests");
    QCoreApplication::setApplicationName("ImageReliefTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_directory.path());
    // The offscreen Windows platform has no default system font database.
    int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    if(font>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).front(),10));
    auto check=[](bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);};
    try {
        auto shape=BRepAlgoAPI_Cut(BRepPrimAPI_MakeCylinder(20,40).Shape(),
            BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0,0,6),gp_Dir(0,0,1)),14,40).Shape()).Shape();
        auto face=TopoDS::Face(TopExp_Explorer(shape,TopAbs_FACE).Current());TopLoc_Location location;
        auto original=BRep_Tool::Triangulation(face,location);
        QImage black(32,32,QImage::Format_ARGB32);black.fill(Qt::black);
        QImage white(32,32,QImage::Format_ARGB32);white.fill(Qt::white);
        // Every box face, including the constant-Z top, must map without a vase axis.
        auto box=BRepPrimAPI_MakeBox(24,20,12).Shape();
        for(int fi=0;fi<6;++fi)for(bool preview:{false,true}) {
            image_relief::Options flat;flat.face_index=fi;flat.spacing=2;flat.height=1;
            flat.repeat_u=flat.repeat_v=1;flat.lower_percent=0;flat.upper_percent=100;flat.fade_mm=2;flat.preview=preview;
            check(image_relief::UsesSurfaceMapping(box,fi),"Box did not choose surface mapping");
            auto base=image_relief::Build(box,black,flat),relief=image_relief::Build(box,white,flat);
            check(base.mesh&&relief.mesh,"Box face "+std::to_string(fi)+": "+base.error+relief.error);
            check(relief.closed&&relief.max_base_edge<=2.00001,"Box closure or spacing failed");
            size_t moved=0;
            for(size_t i=0;i<base.mesh->GetVertices().size();++i) {
                auto a=base.mesh->GetVertices()[i],b=relief.mesh->GetVertices()[i];
                double d=std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));
                check(d<=1.00001,"Planar relief exceeded height");if(d>.99)++moved;
            }
            check(moved>3,"Box face received no full-height relief");
            for(const auto& f:base.mesh->GetFaces())if(f.sourceFaceId!=fi)for(auto c:f.corners) {
                auto a=base.mesh->GetVertices()[c.v],b=relief.mesh->GetVertices()[c.v];
                check(a.x==b.x&&a.y==b.y&&a.z==b.z,"Box relief moved another face or its seam");
            }
        }
        TColgp_Array2OfPnt poles(1,3,1,3);
        for(int u=1;u<=3;++u)for(int v=1;v<=3;++v)poles.SetValue(u,v,gp_Pnt((u-1)*12,(v-1)*10,u==2&&v==2?4:0));
        Handle(Geom_BezierSurface) patch=new Geom_BezierSurface(poles);
        auto freeform=BRepBuilderAPI_MakeFace(patch,1e-6).Shape();
        image_relief::Options surface;surface.face_index=0;surface.allow_open=true;surface.spacing=2;
        surface.height=.5;surface.repeat_u=1;surface.lower_percent=0;surface.upper_percent=100;
        auto free_base=image_relief::Build(freeform,black,surface),free_relief=image_relief::Build(freeform,white,surface);
        check(free_base.mesh&&free_relief.mesh&&!free_relief.closed,"Freeform patch: "+free_base.error+free_relief.error);
        double free_displacement=0;
        for(size_t i=0;i<free_base.mesh->GetVertices().size();++i)
            free_displacement=std::max(free_displacement,double(free_relief.mesh->GetVertices()[i].z-free_base.mesh->GetVertices()[i].z));
        check(free_displacement>.45,"Freeform patch received no relief");
        gp_Trsf tilt;tilt.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,2,3)),.7);
        auto tilted=BRepBuilderAPI_Transform(TopExp_Explorer(box,TopAbs_FACE).Current(),tilt,true).Shape();
        QImage stripes(64,64,QImage::Format_ARGB32);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)stripes.setPixel(x,y,x<32?qRgb(0,0,0):qRgb(255,255,255));
        auto tilt_base=image_relief::Build(tilted,black,surface),tilt_white=image_relief::Build(tilted,white,surface);
        auto tilt_pattern=image_relief::Build(tilted,stripes,surface);
        auto rotated_surface=surface;rotated_surface.rotation_degrees=90;
        auto tilt_rotated=image_relief::Build(tilted,stripes,rotated_surface);
        check(tilt_base.mesh&&tilt_white.mesh&&tilt_pattern.mesh&&tilt_rotated.mesh,"Tilted planar image mapping failed");
        size_t dark=0,bright=0,rotated=0;
        auto distance=[](Vec3 a,Vec3 b){return std::sqrt(std::pow(a.x-b.x,2)+std::pow(a.y-b.y,2)+std::pow(a.z-b.z,2));};
        for(size_t i=0;i<tilt_base.mesh->GetVertices().size();++i) {
            auto a=tilt_base.mesh->GetVertices()[i],b=tilt_pattern.mesh->GetVertices()[i];
            if(distance(a,tilt_white.mesh->GetVertices()[i])>.45) {
                if(distance(a,b)<.01)++dark;if(distance(a,b)>.45)++bright;
                if(distance(b,tilt_rotated.mesh->GetVertices()[i])>.4)++rotated;
            }
        }
        check(dark>2&&bright>2&&rotated>2,"Surface image or its rotation was not mapped in two dimensions");
        image_relief::Options o;o.spacing=4;o.height=1;o.repeat_u=3;o.repeat_v=2;o.lower_percent=10;o.upper_percent=90;
        auto plain=image_relief::Build(shape,black,o);check(bool(plain.mesh),plain.error);
        check(plain.max_base_edge<=4.00001,"Mesh spacing not achieved");
        check(BRep_Tool::Triangulation(face,location)==original,"Source triangulation changed");
        auto raised=image_relief::Build(shape,white,o);check(bool(raised.mesh),raised.error);
        int outer_face=-1;
        for(const auto& f:plain.mesh->GetFaces()) {
            bool outside=true;for(auto c:f.corners){auto v=plain.mesh->GetVertices()[c.v];outside=outside&&std::hypot(v.x,v.y)>19.9&&v.z>0&&v.z<40;}
            if(outside){outer_face=f.sourceFaceId;break;}
        }
        check(outer_face>=0,"Could not locate the outer CAD face");
        auto local_options=o;local_options.face_index=outer_face;
        auto local_plain=image_relief::Build(shape,black,local_options);
        auto local_raised=image_relief::Build(shape,white,local_options);
        check(local_plain.mesh&&local_raised.mesh,local_plain.error+local_raised.error);
        check(local_raised.triangles<raised.triangles,"Unselected faces were densely refined");
        check(local_raised.max_base_edge<=4.00001,"Selected-face spacing not reached");
        for(const auto& f:local_plain.mesh->GetFaces())if(f.sourceFaceId!=outer_face)
            for(auto c:f.corners){auto a=local_plain.mesh->GetVertices()[c.v],b=local_raised.mesh->GetVertices()[c.v];check(a.x==b.x&&a.y==b.y&&a.z==b.z,"Relief leaked onto another face");}
        auto preview_options=local_options;preview_options.preview=true;preview_options.spacing=.2;preview_options.max_triangles=4000;
        auto fast=image_relief::Build(shape,black,preview_options);check(bool(fast.mesh),fast.error);check(fast.triangles<=4000,"Preview triangle cap exceeded");
        TopoDS_Shape open_face;int fi=0;for(TopExp_Explorer ex(shape,TopAbs_FACE);ex.More();ex.Next(),++fi)if(fi==outer_face)open_face=ex.Current();
        auto open_options=o;open_options.face_index=0;open_options.allow_open=true;
        auto open_mesh=image_relief::Build(open_face,white,open_options);check(open_mesh.mesh&&!open_mesh.closed,"Open shell was rejected or capped");
        const auto& a=plain.mesh->GetVertices();const auto& b=raised.mesh->GetVertices();
        check(a.size()==b.size(),"Displacement changed topology");
        size_t inner=0,outer=0;
        for(size_t i=0;i<a.size();++i) {
            double r=std::hypot(a[i].x,a[i].y),rb=std::hypot(b[i].x,b[i].y);
            if(a[i].z>10&&a[i].z<30&&r<14.05){check(std::abs(r-rb)<1e-5&&std::abs(a[i].z-b[i].z)<1e-5,"Cavity displaced");++inner;}
            if(a[i].z>10&&a[i].z<30&&r>19.9){check(rb-r>.95&&rb-r<1.01,"White height incorrect");++outer;}
            if(a[i].z<4||a[i].z>36)check(std::abs(r-rb)<1e-5,"Protected height displaced");
        }
        check(inner>20&&outer>20,"Insufficient sampled cavity and outside vertices");
        QImage pattern(128,32,QImage::Format_ARGB32);
        for(int y=0;y<32;++y)for(int x=0;x<128;++x)pattern.setPixel(x,y,qRgb(x*2,x*2,x*2));
        auto patterned=image_relief::Build(shape,pattern,o);check(bool(patterned.mesh),patterned.error);
        double minimum=10,maximum=-1;
        for(size_t i=0;i<a.size();++i)if(a[i].z>12&&a[i].z<28&&std::hypot(a[i].x,a[i].y)>19.9) {
            auto p=patterned.mesh->GetVertices()[i];double delta=std::hypot(p.x,p.y)-std::hypot(a[i].x,a[i].y);
            minimum=std::min(minimum,delta);maximum=std::max(maximum,delta);
        }
        check(maximum-minimum>.7,"Image pattern was not mapped around the Solid");
        std::map<std::pair<size_t,size_t>,std::pair<int,int>> edges;
        for(const auto& f:raised.mesh->GetFaces())for(size_t i=0;i<3;++i) {
            size_t x=f.corners[i].v,y=f.corners[(i+1)%3].v;
            auto& e=edges[{std::min(x,y),std::max(x,y)}];++e.first;e.second+=x<y?1:-1;
        }
        for(const auto& e:edges)check(e.second.first==2&&e.second.second==0,"Open seam or invalid orientation");
        o.invert=true;auto inverted=image_relief::Build(shape,black,o);check(bool(inverted.mesh),inverted.error);
        for(size_t i=0;i<b.size();++i)check(std::abs(inverted.mesh->GetVertices()[i].x-b[i].x)<1e-6,"Image inversion mismatch");
        QImage transparent(4,4,QImage::Format_ARGB32);transparent.fill(Qt::transparent);
        auto alpha=image_relief::Build(shape,transparent,o);check(bool(alpha.mesh),alpha.error);
        for(size_t i=0;i<a.size();++i)check(std::abs(alpha.mesh->GetVertices()[i].x-a[i].x)<1e-6,"Transparent pixels displaced mesh");
        gp_Trsf rotation;rotation.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,1,0)),3.141592653589793/2);
        auto horizontal=BRepBuilderAPI_Transform(shape,rotation,true).Shape();
        o.axis=0;o.invert=false;auto turned=image_relief::Build(horizontal,white,o);check(bool(turned.mesh),turned.error);
        o.spacing=2;auto dense=image_relief::Build(shape,black,o);check(bool(dense.mesh),dense.error);
        check(dense.triangles>plain.triangles&&dense.max_base_edge<2.00001,"Density does not refine mesh");
        o.max_triangles=20;check(!image_relief::Build(shape,black,o).mesh,"Triangle budget not enforced");
        check(image_relief::Build(shape,black,o,[](const char*){return false;}).error=="Cancelled","Cancellation failed");
        o.spacing=0;check(!image_relief::Build(shape,black,o).mesh,"Invalid spacing accepted");
        check(!image_relief::Build(shape,QImage(),o).mesh,"Missing image accepted");
        auto round_shell=BRepAlgoAPI_Cut(BRepPrimAPI_MakeSphere(gp_Pnt(0,0,35),30).Shape(),
            BRepPrimAPI_MakeSphere(gp_Pnt(0,0,35),24).Shape()).Shape();
        auto bowl=BRepAlgoAPI_Common(round_shell,BRepPrimAPI_MakeBox(gp_Pnt(-40,-40,7),80,80,43).Shape()).Shape();
        image_relief::Options curved;curved.spacing=4;curved.height=.8;curved.repeat_u=3;
        auto curved_mesh=image_relief::Build(bowl,pattern,curved);check(bool(curved_mesh.mesh),"Rounded bowl: "+curved_mesh.error);
        LanguageManager::Instance().SetLanguage("Russian");
        {
            CSolid box_solid(box);check(box_solid.InitSurfaces(),"Could not initialise box surfaces");
            ImageReliefDialog box_dialog(box_solid,nullptr,5);
            check(!box_dialog.findChild<QComboBox*>("reliefAxis")->isEnabled(),"Planar dialog still uses vase axis");
            check(box_dialog.findChild<QSpinBox*>("reliefRepeatU")->value()==1,"Planar default is not one image");
            box_dialog.SetImage(stripes);
            box_dialog.findChild<QPushButton*>("previewReliefMesh")->click();
            check(box_dialog.findChild<QPushButton*>("createReliefMesh")->isEnabled(),"Box preview failed");
            box_dialog.findChild<QPushButton*>("createReliefMesh")->click();
            check(bool(box_dialog.TakeMesh()),"Box final build failed");
            QSettings().clear();
        }
        CSolid solid(shape);check(solid.InitSurfaces(),"Could not initialise UI surfaces");int surface_index=-1;
        for(int i=0;i<solid.GetNumSurfaces();++i)if(solid.GetSurfaceFace(i)->m_Face.IsSame(open_face))surface_index=i;
        check(surface_index>=0,"UI face mapping failed");
        ImageReliefDialog dialog(solid,nullptr,surface_index);
        auto* create=dialog.findChild<QPushButton*>("createReliefMesh");
        check(create&&!create->isEnabled(),"Creation enabled without an image");
        check(dialog.findChild<QDoubleSpinBox*>("reliefSpacing")->value()==2,"Unexpected default spacing");
        dialog.SetImage(white);
        check(!create->isEnabled(),"Final build enabled before preview");
        auto* quick=dialog.findChild<QPushButton*>("previewReliefMesh");check(quick&&quick->isEnabled(),"Preview unavailable after image load");
        QTimer::singleShot(0,&dialog,&QDialog::reject);
        quick->click();check(!create->isEnabled()&&!dialog.TakeMesh(),"Cancelling preview committed a result");
        quick->click();check(create->isEnabled(),"Preview did not enable final build");
        const auto screenshot=qEnvironmentVariable("DOM3D_RELIEF_SCREENSHOT");
        if(!screenshot.isEmpty()){dialog.show();app.processEvents();check(dialog.grab().save(screenshot),"Could not save dialog screenshot");}
        dialog.findChild<QDoubleSpinBox*>("reliefHeight")->setValue(1.2);check(!create->isEnabled(),"Parameter edit did not invalidate preview");
        quick->click();check(create->isEnabled(),"Updated preview failed");
        create->click();check(bool(dialog.TakeMesh()),"Final build failed after preview");
        const std::vector<std::pair<const char*,double>> remembered={
            {"reliefSpacing",.75},{"reliefHeight",2.5},{"reliefRotation",37.5},
            {"reliefStart",12},{"reliefEnd",87},{"reliefFade",4.5}};
        for(auto entry:remembered)dialog.findChild<QDoubleSpinBox*>(entry.first)->setValue(entry.second);
        dialog.findChild<QSpinBox*>("reliefRepeatU")->setValue(9);
        dialog.findChild<QSpinBox*>("reliefRepeatV")->setValue(3);
        dialog.findChild<QComboBox*>("reliefAxis")->setCurrentIndex(1);
        dialog.findChild<QCheckBox*>("reliefInvert")->setChecked(true);
        dialog.findChild<QCheckBox*>("reliefHideSource")->setChecked(false);
        const QString remembered_image=settings_directory.path()+"/ornament.png";
        check(pattern.save(remembered_image),"Could not save remembered image fixture");
        QSettings().setValue("tools/imageRelief/imagePath",remembered_image);
        dialog.resize(1080,780);dialog.reject();QSettings().sync();
        ImageReliefDialog restored(solid,nullptr,surface_index);
        for(auto entry:remembered)check(restored.findChild<QDoubleSpinBox*>(entry.first)->value()==entry.second,"Relief numeric parameter was not restored");
        check(restored.findChild<QSpinBox*>("reliefRepeatU")->value()==9&&restored.findChild<QSpinBox*>("reliefRepeatV")->value()==3,"Repeats were not restored");
        check(restored.findChild<QComboBox*>("reliefAxis")->currentIndex()==1,"Axis was not restored");
        check(restored.findChild<QCheckBox*>("reliefInvert")->isChecked()&&!restored.HideSource(),"Checkboxes were not restored");
        check(restored.findChild<QPushButton*>("previewReliefMesh")->isEnabled(),"Last image was not restored");
        check(!restored.findChild<QPushButton*>("createReliefMesh")->isEnabled(),"Restored settings reused a stale preview");
        check(!QSettings().value("tools/imageRelief/geometry").toByteArray().isEmpty(),"Window geometry was not saved");
        QFile::remove(remembered_image);
        ImageReliefDialog missing_image(solid,nullptr,surface_index);
        check(!missing_image.findChild<QPushButton*>("previewReliefMesh")->isEnabled(),"Missing remembered image enabled preview");
        MainWindow window;
        window.document_.ClearSelection();
        window.ShowImageReliefTool();
        check(window.image_relief_pick_pending_,"Relief did not enter body-pick mode");
        window.SetTool(ToolMode::Select,"Select");
        check(!window.image_relief_pick_pending_,"Changing tools did not cancel relief pick");
        auto selected=std::make_unique<CSolid>(shape);
        check(selected->ReBuldMesh(),"Could not initialise pick surfaces");
        window.document_.AddObject(std::move(selected));
        auto selected_id=window.document_.GetSelectedSolid()->m_id;
        window.document_.ClearSelection();
        window.ShowImageReliefTool();
        check(window.document_.SelectObjectById(selected_id),"Could not select test vase");
        check(window.document_.SelectSolidFaceAtScreen({30,200},[](Vec3 p,DomPoint& screen,float& depth){screen={static_cast<int>(p.x*10),static_cast<int>(p.z*10)};depth=p.y;return true;}),"Could not pick a face");
        bool opened=false;
        QTimer close_dialog;
        close_dialog.setInterval(10);
        QObject::connect(&close_dialog,&QTimer::timeout,[&]() {
            for(auto* widget:QApplication::topLevelWidgets()) {
                if(widget->objectName()=="ImageReliefDialog"&&widget->isVisible()) {
                    opened=true;static_cast<QDialog*>(widget)->reject();
                }
            }
        });
        close_dialog.start();
        window.viewport_->SelectionChanged();
        app.processEvents();
        close_dialog.stop();
        check(opened&&!window.image_relief_pick_pending_,"Selecting a vase did not open the image dialog; pending="
            +std::to_string(window.image_relief_pick_pending_)+"; tool="+window.active_tool_key_);
        std::cout<<"Image relief: spacing, closed seams, cavity, height, masks, inversion, alpha, axis, budget and cancellation passed. "<<raised.triangles<<" triangles.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Image relief: "<<e.what()<<'\n';return 1;}
}
