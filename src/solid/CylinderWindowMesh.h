#pragma once

// Included by Solid.cpp inside its private implementation namespace.
// A cylindrical window and its circular fillet must share one angular chart.
// Recognizes one full-period cylinder, one coaxial regular torus, and planar
// neighbours. The window has two constant-U sides and two monotone profiles;
// it must not cross the CAD seam. Other configurations keep their existing path.
quadro::BodyMeshAttempt rebuild_planar_cylinder_window(
    CSolid* owner, const std::vector<CSurfaceFace*>& surfaces, float deflection)
{
    constexpr double tau = 6.28318530717958647692;
    CSurfaceFace* side = nullptr;
    CSurfaceFace* fillet = nullptr;
    for (auto* s : surfaces) {
        if (!s || s->m_Face.IsNull() || !s->pMesh3D) return quadro::BodyMeshAttempt::NotApplicable;
        const auto type = BRepAdaptor_Surface(TopoDS::Face(s->m_Face)).GetType();
        if (type == GeomAbs_Cylinder) { if (side) return quadro::BodyMeshAttempt::NotApplicable; side=s; }
        else if (type == GeomAbs_Torus) { if (fillet) return quadro::BodyMeshAttempt::NotApplicable; fillet=s; }
        else if (type != GeomAbs_Plane) return quadro::BodyMeshAttempt::NotApplicable;
    }
    if (!side || !fillet || fillet->m_TypeMesh != REGULAR_MESH || owner->MeshQuadroHoleSLX)
        return quadro::BodyMeshAttempt::NotApplicable;
    const auto face = TopoDS::Face(side->m_Face);
    BRepAdaptor_Surface cylinder_surface(face);
    const auto cylinder = cylinder_surface.Cylinder();
    const auto fillet_torus = BRepAdaptor_Surface(TopoDS::Face(fillet->m_Face)).Torus();
    const gp_Vec axis(cylinder.Axis().Direction());
    const gp_Vec centres(cylinder.Location(),fillet_torus.Location());
    if (!fillet_torus.Axis().Direction().IsParallel(cylinder.Axis().Direction(),1.e-7)
        || (centres-axis.Multiplied(centres.Dot(axis))).Magnitude()>cylinder.Radius()*1.e-6
        || fillet->pMesh3D->GetNormals().size()!=fillet->pMesh3D->GetVertices().size()
        || fillet->pMesh3D->GetUVs().size()!=fillet->pMesh3D->GetVertices().size())
        return quadro::BodyMeshAttempt::NotApplicable;
    double umin,umax,vmin,vmax;
    BRepTools::UVBounds(face,umin,umax,vmin,vmax);
    if (std::abs(umax-umin-tau)>1.e-6) return quadro::BodyMeshAttempt::NotApplicable;
    const auto uv_at = [&](const gp_Pnt& p) {
        double u,v; ElSLib::Parameters(cylinder,p,u,v);
        u += std::floor((umin-u)/tau)*tau;
        if (u<umin-1.e-7) u+=tau;
        if (std::abs(u-umax)<1.e-7) u=umin;
        return std::pair<double,double>{u,v};
    };
    struct Edge { int index; TopoDS_Edge shape; double u0,u1,v0,v1; int count; };
    std::vector<Edge> rings,verticals,profiles;
    for(int i=0;i<side->GetPreparedPolylineCount();++i) {
        TopoDS_Edge edge;
        if(!side->GetPreparedTopoEdge(i,edge)) return quadro::BodyMeshAttempt::NotApplicable;
        if(BRep_Tool::IsClosed(edge,face)) continue;
        BRepAdaptor_Curve curve(edge);
        const auto a=uv_at(curve.Value(curve.FirstParameter()));
        const auto b=uv_at(curve.Value(curve.LastParameter()));
        Edge e{i,edge,a.first,b.first,a.second,b.second,side->GetPreparedPolylinePointCount(i)};
        if(curve.GetType()==GeomAbs_Circle && std::abs(curve.LastParameter()-curve.FirstParameter()-tau)<1.e-5) rings.push_back(e);
        else if(std::abs(e.u1-e.u0)<1.e-6) verticals.push_back(e);
        else profiles.push_back(e);
    }
    if(rings.size()!=2 || verticals.size()!=2 || profiles.size()!=2) return quadro::BodyMeshAttempt::NotApplicable;
    const double left=std::min(verticals[0].u0,verticals[1].u0);
    const double right=std::max(verticals[0].u0,verticals[1].u0);
    if(left<=umin+1.e-6 || right>=umax-1.e-6 || right-left>=tau-1.e-6) return quadro::BodyMeshAttempt::NotApplicable;
    for(const auto& e:profiles) {
        if(std::abs(std::min(e.u0,e.u1)-left)>1.e-6 || std::abs(std::max(e.u0,e.u1)-right)>1.e-6)
            return quadro::BodyMeshAttempt::NotApplicable;
        BRepAdaptor_Curve curve(e.shape);
        double previous=e.u0;
        for(int k=1;k<=32;++k) {
            double u=uv_at(curve.Value(curve.FirstParameter()+(curve.LastParameter()-curve.FirstParameter())*k/32.)).first;
            if((u-previous)*(e.u1-e.u0)<-1.e-8) return quadro::BodyMeshAttempt::NotApplicable;
            previous=u;
        }
    }
    const auto profile_v = [&](const Edge& e,double u) {
        BRepAdaptor_Curve curve(e.shape); double a=curve.FirstParameter(),b=curve.LastParameter();
        for(int k=0;k<48;++k) {
            const double m=(a+b)*0.5;
            const double x=uv_at(curve.Value(m)).first;
            if((x<u)==(e.u1>e.u0)) a=m; else b=m;
        }
        return uv_at(curve.Value((a+b)*0.5)).second;
    };
    if(profile_v(profiles[0],(left+right)*0.5)>profile_v(profiles[1],(left+right)*0.5)) std::swap(profiles[0],profiles[1]);
    // Preserve the regular neighbour's column count, but redistribute columns
    // between the seam and the two window corners instead of inserting strips.
    std::vector<double> old_angles;
    for(const auto& p:fillet->pMesh3D->GetVertices()) old_angles.push_back(uv_at(gp_Pnt(p.x,p.y,p.z)).first);
    std::sort(old_angles.begin(),old_angles.end());
    old_angles.erase(std::unique(old_angles.begin(),old_angles.end(),[](double a,double b){return std::abs(a-b)<1.e-5;}),old_angles.end());
    const int total=static_cast<int>(old_angles.size());
    const int middle=std::max(profiles[0].count,profiles[1].count)-1;
    if(total<8 || middle<2 || middle>total-4 || std::abs(old_angles.front()-umin)>1.e-5) return quadro::BodyMeshAttempt::NotApplicable;
    const int before=std::clamp(int(std::lround((total-middle)*(left-umin)/(tau-(right-left)))),2,total-middle-2);
    const int after=total-middle-before;
    std::vector<double> angles;
    for(const auto& range:std::vector<std::tuple<double,double,int>>{{umin,left,before},{left,right,middle},{right,umax,after}})
        for(int i=0;i<std::get<2>(range);++i) angles.push_back(std::get<0>(range)+(std::get<1>(range)-std::get<0>(range))*i/std::get<2>(range));
    const int height=std::max(verticals[0].count,verticals[1].count)-1;
    const double step=cylinder.Radius()*tau/total;
    double low_span=0,high_span=0;
    for(double u:angles) {
        const double x=std::clamp(u,left,right),lo=profile_v(profiles[0],x),hi=profile_v(profiles[1],x);
        if(lo<=vmin+1.e-6 || hi>=vmax-1.e-6 || hi<=lo+1.e-6) return quadro::BodyMeshAttempt::NotApplicable;
        low_span=std::max(low_span,lo-vmin);high_span=std::max(high_span,vmax-hi);
    }
    const int below=std::max(2,int(std::lround(low_span/step))),above=std::max(2,int(std::lround(high_span/step)));
    const int rows=below+height+above;
    std::vector<Vec3> vertices,normals;std::vector<UV> uvs;std::vector<CMesh3D::Face> cells;
    for(int r=0;r<=rows;++r)for(double u:angles) {
        const double x=std::clamp(u,left,right),lo=profile_v(profiles[0],x),hi=profile_v(profiles[1],x);
        const double v=r<=below?vmin+(lo-vmin)*r/below:r<=below+height?lo+(hi-lo)*(r-below)/height:hi+(vmax-hi)*(r-below-height)/above;
        CPoint8d p;if(!side->GetPoint(u,v,&p)) return quadro::BodyMeshAttempt::NotApplicable;
        vertices.push_back({float(p.x),float(p.y),float(p.z)});normals.push_back({float(p.l),float(p.m),float(p.n)});uvs.push_back({float(u),float(v)});
    }
    const auto index=[&](int c,int r){return size_t(r*total+(c%total));};
    for(int r=0;r<rows;++r)for(int c=0;c<total;++c) {
        if(c>=before && c<before+middle && r>=below && r<below+height)continue;
        CMesh3D::Face q{index(c,r),index(c+1,r),index(c+1,r+1),index(c,r+1)};
        for(auto& corner:q.corners){corner.n=corner.v;corner.uv=corner.v;}
        if(face.Orientation()==TopAbs_REVERSED)std::reverse(q.corners.begin(),q.corners.end());
        cells.push_back(std::move(q));
    }
    // Snapshot before publishing any mesh or boundary changes.
    struct Saved { CSurfaceFace* s;std::vector<Vec3> v,n;std::vector<UV> uv;std::vector<CMesh3D::Face> f;std::vector<std::vector<CPoint3d>> edges;std::vector<CPoint3d> cap;int qu,qv,type;bool init,trim; };
    std::vector<Saved> saved;
    for(auto* s:surfaces){Saved x{s,s->pMesh3D->GetVertices(),s->pMesh3D->GetNormals(),s->pMesh3D->GetUVs(),s->pMesh3D->GetFaces(),{},s->m_CircularCapMasterBoundary3D,s->m_QtyU,s->m_QtyV,s->m_TypeMesh,s->IsInitMesh,s->IsTrimmed};for(int i=0;i<s->GetPreparedPolylineCount();++i){std::vector<CPoint3d> p;s->GetPreparedPolylinePoints(i,p);x.edges.push_back(p);}saved.push_back(std::move(x));}
    bool committed=false;
    struct Rollback { std::vector<Saved>& saved;bool& committed;~Rollback(){if(committed)return;for(auto& x:saved){x.s->pMesh3D->SetGeometry(x.v,x.f,x.uv,x.n);x.s->SetCircularCapMasterBoundary(x.cap);for(size_t i=0;i<x.edges.size();++i)x.s->SetPreparedPolylinePoints(int(i),x.edges[i]);x.s->m_QtyU=x.qu;x.s->m_QtyV=x.qv;x.s->m_TypeMesh=x.type;x.s->IsInitMesh=x.init;x.s->IsTrimmed=x.trim;}} } rollback{saved,committed};
    // Reparameterize the circular fillet on the same angular columns.
    auto fv=fillet->pMesh3D->GetVertices();auto fn=fillet->pMesh3D->GetNormals();auto fu=fillet->pMesh3D->GetUVs();auto ff=fillet->pMesh3D->GetFaces();
    BRepAdaptor_Surface torus_surface(TopoDS::Face(fillet->m_Face));const auto torus=torus_surface.Torus();
    for(size_t i=0;i<fv.size();++i){
        const gp_Pnt p(fv[i].x,fv[i].y,fv[i].z);const double old=uv_at(p).first;
        const auto it=std::min_element(old_angles.begin(),old_angles.end(),[&](double a,double b){return std::abs(a-old)<std::abs(b-old);});
        const double angle=(angles[size_t(it-old_angles.begin())]-old)*(cylinder.Position().Direct()?1.0:-1.0);
        gp_Trsf turn;turn.SetRotation(cylinder.Axis(),angle);const auto moved=p.Transformed(turn);
        double u,v;ElSLib::Parameters(torus,moved,u,v);CPoint8d point;
        if(!fillet->GetPoint(u,v,&point))return quadro::BodyMeshAttempt::Failed;
        fv[i]={float(point.x),float(point.y),float(point.z)};fn[i]={float(point.l),float(point.m),float(point.n)};fu[i]={float(u),float(v)};
    }
    for(auto& f:ff)for(auto& c:f.corners){c.n=c.v;c.uv=c.v;}
    if(!fillet->pMesh3D->SetGeometry(fv,ff,fu,fn) || !side->pMesh3D->SetGeometry(vertices,cells,uvs,normals))return quadro::BodyMeshAttempt::Failed;
    side->m_QtyU=side->m_QtyV=0;side->IsInitMesh=true;side->IsTrimmed=true;
    const auto share=[&](const TopoDS_Edge& edge,std::vector<CPoint3d> points,bool circular){
        for(auto* s:surfaces)for(int i=0;i<s->GetPreparedPolylineCount();++i){TopoDS_Edge other;if(!s->GetPreparedTopoEdge(i,other)||!other.IsSame(edge))continue;
            std::vector<CPoint3d> old;s->GetPreparedPolylinePoints(i,old);auto aligned=points;
            if(!circular && !old.empty() && old.front().DistTo(&aligned.back())<old.front().DistTo(&aligned.front()))std::reverse(aligned.begin(),aligned.end());
            s->SetPreparedPolylinePoints(i,aligned);
            if(circular && BRepAdaptor_Surface(TopoDS::Face(s->m_Face)).GetType()==GeomAbs_Plane)s->SetCircularCapMasterBoundary(aligned);
        }
    };
    // Every torus boundary (including the inner cap) follows its updated row.
    for(int i=0;i<fillet->GetPreparedPolylineCount();++i){TopoDS_Edge e;std::vector<CPoint3d> p;
        if(fillet->GetPreparedTopoEdge(i,e)&&fillet->GetRegularMeshBoundaryPoints(i,p))share(e,p,BRepAdaptor_Curve(e).GetType()==GeomAbs_Circle);}
    const auto point=[&](int c,int r){const auto& p=vertices[index(c,r)];return CPoint3d(p.x,p.y,p.z);};
    for(const auto& ring:rings){std::vector<CPoint3d> p;const int r=std::abs(ring.v0-vmin)<std::abs(ring.v0-vmax)?0:rows;for(int c=0;c<=total;++c)p.push_back(point(c,r));share(ring.shape,p,true);}
    for(int k=0;k<2;++k){std::vector<CPoint3d> p;for(int c=before;c<=before+middle;++c)p.push_back(point(c,below+(k?height:0)));share(profiles[k].shape,p,false);}
    for(const auto& edge:verticals){std::vector<CPoint3d> p;const int c=std::abs(edge.u0-left)<1.e-6?before:before+middle;for(int r=below;r<=below+height;++r)p.push_back(point(c,r));share(edge.shape,p,false);}
    for(auto* s:surfaces)if(s!=side && s!=fillet && s->m_TypeMesh!=REGULAR_MESH){s->IsInitMesh=false;if(!s->BuildTrimmingMesh(owner,deflection))return quadro::BodyMeshAttempt::Failed;}
    std::vector<const CMesh3D*> meshes;for(auto* s:surfaces)meshes.push_back(s->pMesh3D);
    auto welded=CMesh3D::CreateWelded(meshes);if(!welded)return quadro::BodyMeshAttempt::Failed;
    std::map<std::pair<size_t,size_t>,int> edges;
    for(const auto& f:welded->GetFaces())if(!f.deleted)for(size_t i=0;i<f.corners.size();++i)++edges[std::minmax(f.corners[i].v,f.corners[(i+1)%f.corners.size()].v)];
    int open=0;for(const auto& e:edges)if(e.second!=2)++open;
    if(open){side->m_LastIslandFillError="Cylinder window boundary: "+std::to_string(open)+" unmatched edges";return quadro::BodyMeshAttempt::Failed;}
    committed=true;return quadro::BodyMeshAttempt::Built;
}
