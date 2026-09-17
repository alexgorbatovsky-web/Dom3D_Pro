#pragma once

// Included in Solid.cpp's private meshing namespace, after the cube-sphere
// builder. This transaction handles spherical skins and rectangular fillets.
// CAD edge identity establishes adjacency; coincident supporting circles only
// combine split arcs of those already shared edges.
bool split_sphere_boundary_cell(const std::vector<Vec3>& vertices,
    const CMesh3D::Face& source, Vec3 normal, std::vector<CMesh3D::Face>& result)
{
    std::vector<size_t> polygon;for(auto c:source.corners)polygon.push_back(c.v);
    double orientation=0;
    for(size_t k=1;k+1<polygon.size();++k)
        orientation+=dot(cross(vertices[polygon[k]]-vertices[polygon[0]],vertices[polygon[k+1]]-vertices[polygon[0]]),normal);
    if(orientation<0)normal=normal*-1.f;
    const auto turn=[&](size_t a,size_t b,size_t c){return double(dot(cross(vertices[b]-vertices[a],vertices[c]-vertices[a]),normal));};
    const auto inside=[&](size_t p,size_t a,size_t b,size_t c){return turn(a,b,p)>=-1.e-9 && turn(b,c,p)>=-1.e-9 && turn(c,a,p)>=-1.e-9;};
    const auto append_cell=[&](const std::vector<size_t>& ids){CMesh3D::Face face;face.sourceFaceId=source.sourceFaceId;for(auto id:ids)face.corners.push_back({id,id,0});result.push_back(std::move(face));};
    while(polygon.size()>3) {
        size_t chosen=polygon.size();double quality=-1;
        for(size_t k=0;k<polygon.size();++k) {
            size_t a=polygon[k],b=polygon[(k+1)%polygon.size()],c=polygon[(k+2)%polygon.size()],d=polygon[(k+3)%polygon.size()];
            double ab=turn(a,b,c),cd=turn(a,c,d);
            if(ab<=1.e-9 || cd<=1.e-9 || turn(b,c,d)<=1.e-9 || turn(d,a,b)<=1.e-9)continue;
            bool clear=true;for(auto p:polygon)if(p!=a&&p!=b&&p!=c&&p!=d&&(inside(p,a,b,c)||inside(p,a,c,d)))clear=false;
            if(!clear)continue;
            double longest=0;
            for(auto p:{a,b,c,d})for(auto q:{a,b,c,d})longest=std::max(longest,double(dot(vertices[p]-vertices[q],vertices[p]-vertices[q])));
            double score=std::min(ab,cd)/std::max(longest,1.e-12);
            if(score>quality){quality=score;chosen=k;}
        }
        if(chosen<polygon.size()) {
            size_t n=polygon.size();append_cell({polygon[chosen],polygon[(chosen+1)%n],polygon[(chosen+2)%n],polygon[(chosen+3)%n]});
            if(n==4)return true;
            std::vector<size_t> rest;for(size_t i=0;i<n;++i)if(i!=(chosen+1)%n&&i!=(chosen+2)%n)rest.push_back(polygon[i]);polygon=std::move(rest);continue;
        }
        bool clipped=false;
        for(size_t k=0;k<polygon.size();++k) {
            size_t a=polygon[k],b=polygon[(k+1)%polygon.size()],c=polygon[(k+2)%polygon.size()];
            if(turn(a,b,c)<=1.e-9)continue;
            bool clear=true;for(auto p:polygon)if(p!=a&&p!=b&&p!=c&&inside(p,a,b,c))clear=false;
            if(!clear)continue;
            append_cell({a,b,c});polygon.erase(polygon.begin()+(k+1)%polygon.size());clipped=true;break;
        }
        if(!clipped)return false;
    }
    if(polygon.size()!=3 || turn(polygon[0],polygon[1],polygon[2])<=1.e-9)return false;
    append_cell(polygon);return true;
}

quadro::BodyMeshAttempt try_build_intersecting_spheres(
    const std::vector<CSurfaceFace*>& surfaces, float deflection, int refinement)
{
    using Attempt = quadro::BodyMeshAttempt;
    if (surfaces.size() < 2) return Attempt::NotApplicable;
    double radius_max = 0;
    for (auto* s : surfaces) {
        if (!s || s->m_Face.IsNull()) return Attempt::NotApplicable;
        if (!s->IsSpherical()) {
            const auto face=TopoDS::Face(s->m_Face);
            if(BRepAdaptor_Surface(face).GetType()!=GeomAbs_BSplineSurface)return Attempt::NotApplicable;
            double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
            const double eps=std::max({u1-u0,v1-v0,1.})*1.e-7;
            int count=0;
            for(TopExp_Explorer it(face,TopAbs_EDGE);it.More();it.Next()) {
                ++count;double first,last;
                auto curve=BRep_Tool::CurveOnSurface(TopoDS::Edge(it.Current()),face,first,last);
                if(curve.IsNull())return Attempt::NotApplicable;
                bool left=true,right=true,bottom=true,top=true;
                for(int k=0;k<=16;++k) {
                    auto uv=curve->Value(first+(last-first)*k/16);
                    left=left&&std::abs(uv.X()-u0)<eps;right=right&&std::abs(uv.X()-u1)<eps;
                    bottom=bottom&&std::abs(uv.Y()-v0)<eps;top=top&&std::abs(uv.Y()-v1)<eps;
                }
                if(!left&&!right&&!bottom&&!top)return Attempt::NotApplicable;
            }
            if(count!=4)return Attempt::NotApplicable;
            continue;
        }
        const auto sphere=BRepAdaptor_Surface(TopoDS::Face(s->m_Face)).Sphere();
        // This branch builds outward union skins. Cavity/mirrored sphere
        // orientations retain the existing general mesher.
        if(s->m_Face.Orientation()!=TopAbs_FORWARD || !sphere.Position().Direct())return Attempt::NotApplicable;
        if(!std::isfinite(sphere.Radius()) || sphere.Radius()<=0)return Attempt::NotApplicable;
        radius_max = std::max(radius_max,sphere.Radius());
    }
    if(radius_max<=0)return Attempt::NotApplicable;
    const double tolerance = std::max(1.e-6, radius_max * 4.e-6);
    // Identification budget for the legacy float/UV clipping pass, separate
    // from master-node deduplication and from the exact final seam contract.
    const double projection_budget = std::max(1.e-5, radius_max * 1.e-4);
    constexpr double tau = 6.2831853071795864769;
    const auto angle_normalize = [](double t) { t = std::fmod(t, tau); return t < 0 ? t + tau : t; };
    const auto delta = [](double a, double b) { return std::remainder(b - a, tau); };
    const auto xyz = [](gp_Pnt p) { return Vec3{float(p.X()), float(p.Y()), float(p.Z())}; };
    const auto pnt = [](Vec3 p) { return gp_Pnt(p.x,p.y,p.z); };
    const auto fail = [&](const char* message) {
        surfaces.front()->m_LastIslandFillError = std::string("Sphere boundary: ") + message;
        return Attempt::Failed;
    };
    struct Master {
        gp_Circ circle; std::vector<double> samples; std::vector<Vec3> points;
        std::shared_ptr<BRepAdaptor_Curve> curve;
        double scale=1;
        bool closed=false, rim=false;
    };
    std::vector<Master> masters;
    TopTools_IndexedMapOfShape edges;
    std::vector<std::vector<size_t>> owners;
    for (size_t f=0; f<surfaces.size(); ++f)
        for (TopExp_Explorer it(surfaces[f]->m_Face,TopAbs_EDGE);it.More();it.Next()) {
            auto e=TopoDS::Edge(it.Current());
            if (BRep_Tool::Degenerated(e)) continue;
            size_t id=edges.Add(e)-1;
            if (id==owners.size()) owners.emplace_back();
            if (std::find(owners[id].begin(),owners[id].end(),f)==owners[id].end()) owners[id].push_back(f);
        }
    std::vector<std::vector<size_t>> face_masters(surfaces.size());
    std::vector<gp_Pnt> anchors;
    for (size_t i=0;i<owners.size();++i) {
        if (owners[i].size()==1) {
            const auto e=TopoDS::Edge(edges(int(i+1)));
            if (!BRep_Tool::IsClosed(e,TopoDS::Face(surfaces[owners[i][0]]->m_Face))) return Attempt::NotApplicable;
            continue;
        }
        if (owners[i].size()!=2) return Attempt::NotApplicable;
        BRepAdaptor_Curve curve(TopoDS::Edge(edges(int(i+1))));
        if (curve.GetType()!=GeomAbs_Circle) {
            // Fillet rims and their short joining profiles can both be splines.

            Master m;m.curve=std::make_shared<BRepAdaptor_Curve>(TopoDS::Edge(edges(int(i+1))));
            GProp_GProps props;BRepGProp::LinearProperties(edges(int(i+1)),props);m.scale=props.Mass();
            if(m.scale<=tolerance)return Attempt::NotApplicable;
            m.closed=curve.Value(curve.FirstParameter()).Distance(curve.Value(curve.LastParameter()))<tolerance;
            m.rim=surfaces[owners[i][0]]->IsSpherical() || surfaces[owners[i][1]]->IsSpherical();
            m.samples={0,1};anchors.push_back(curve.Value(curve.FirstParameter()));anchors.push_back(curve.Value(curve.LastParameter()));
            for(auto f:owners[i])face_masters[f].push_back(masters.size());
            masters.push_back(std::move(m));continue;
        }
        const auto circle=curve.Circle();
        size_t group=masters.size();
        for(size_t j=0;j<masters.size();++j) {
            if(masters[j].curve)continue;
            const auto& other=masters[j].circle;
            if (circle.Location().Distance(other.Location())<tolerance
                && std::abs(circle.Radius()-other.Radius())<tolerance
                && std::abs(circle.Axis().Direction().Dot(other.Axis().Direction()))>1.-1.e-10) { group=j;break; }
        }
        if(group==masters.size()) masters.push_back({circle,{},{}});
        auto& m=masters[group];m.scale=circle.Radius();m.rim=surfaces[owners[i][0]]->IsSpherical() || surfaces[owners[i][1]]->IsSpherical();
        for(double t:{curve.FirstParameter(),curve.LastParameter()}) {
            auto point=curve.Value(t);anchors.push_back(point);
            m.samples.push_back(angle_normalize(ElCLib::Parameter(m.circle,point)));
        }
        for(auto f:owners[i]) if(std::find(face_masters[f].begin(),face_masters[f].end(),group)==face_masters[f].end()) face_masters[f].push_back(group);
    }
    if(masters.empty()) return Attempt::NotApplicable;
    for(size_t f=0;f<surfaces.size();++f)if(!surfaces[f]->IsSpherical()) {
        int rings=0;for(auto g:face_masters[f])rings+=masters[g].rim;
        if(rings!=2)return Attempt::NotApplicable;
    }
    const auto difference=[&](size_t g,double a,double b){return masters[g].curve?(masters[g].closed?std::remainder(b-a,1.):b-a):delta(a,b);};
    const auto parameter=[&](size_t g,double a){return masters[g].curve?(masters[g].closed?a-std::floor(a):std::clamp(a,0.,1.)):angle_normalize(a);};
    const auto evaluate=[&](size_t g,double t){
        const auto& m=masters[g];
        if(m.curve&&m.closed)t-=std::floor(t);
        return m.curve?m.curve->Value(m.curve->FirstParameter()+t*(m.curve->LastParameter()-m.curve->FirstParameter())):ElCLib::Value(t,m.circle);
    };
    const auto project_uncached = [&](size_t g, Vec3 v) {
        if(masters[g].curve) {
            // Bounded closest point, including across a closed spline's seam.
            double best=0,dist=std::numeric_limits<double>::max();
            for(int k=0;k<=32;++k){double t=k/32.;double d=evaluate(g,t).SquareDistance(pnt(v));if(d<dist){dist=d;best=t;}}
            double lo=masters[g].closed?best-1./32:std::max(0.,best-1./32),hi=masters[g].closed?best+1./32:std::min(1.,best+1./32);
            for(int k=0;k<50;++k){double a=lo+(hi-lo)/3,b=hi-(hi-lo)/3;if(evaluate(g,a).SquareDistance(pnt(v))<evaluate(g,b).SquareDistance(pnt(v)))hi=b;else lo=a;}
            double t=(lo+hi)/2;
            for(double end:{0.,1.})if(evaluate(g,end).SquareDistance(pnt(v))<evaluate(g,t).SquareDistance(pnt(v)))t=end;
            return std::make_pair(parameter(g,t),evaluate(g,t).Distance(pnt(v)));
        }
        const auto& c=masters[g].circle;
        double t=angle_normalize(ElCLib::Parameter(c,pnt(v)));
        return std::make_pair(t,ElCLib::Value(t,c).Distance(pnt(v)));
    };
    std::map<std::tuple<size_t,float,float,float>,std::pair<double,double>> projections;
    const auto project=[&](size_t g,Vec3 v){
        const auto key=std::make_tuple(g,v.x,v.y,v.z);
        auto found=projections.find(key);if(found!=projections.end())return found->second;
        auto result=project_uncached(g,v);projections.emplace(key,result);return result;
    };
    struct Route {size_t group;double first,last;};
    using EdgeKey=std::pair<size_t,size_t>;
    struct Draft {
        std::unique_ptr<CSurfaceFace> surface;
        std::map<EdgeKey,std::vector<Route>> routes;
    };
    std::vector<Draft> drafts;
    const int largest_segments=std::clamp(int(std::lround(20./deflection))+refinement,4,128);
    for(size_t f=0;f<surfaces.size();++f) {
        Draft draft;draft.surface=std::make_unique<CSurfaceFace>(surfaces[f]->m_Face);
        if(surfaces[f]->IsSpherical()) {
            double radius=BRepAdaptor_Surface(TopoDS::Face(surfaces[f]->m_Face)).Sphere().Radius();
            int segments=std::max(4,int(std::lround(largest_segments*radius/radius_max)));
            if(!build_sphere_cube_quad_mesh(draft.surface.get(),deflection,false,segments)
                || !trim_sphere_cube_quad_mesh(draft.surface.get())) return fail("Cannot clip background grid");
        } else {
            // A fillet is a rectangular CAD patch, including periodic collars.
            // Retain its exact surface; only its boundary is shared with spheres.
            const auto face=TopoDS::Face(surfaces[f]->m_Face);
            BRepAdaptor_Surface adaptor(face);
            double u0,u1,v0,v1;BRepTools::UVBounds(face,u0,u1,v0,v1);
            auto length=[&](bool u){double sum=0;auto prev=adaptor.Value(u0,v0);for(int k=1;k<=32;++k){auto next=adaptor.Value(u?u0+(u1-u0)*k/32:u0,u?v0:v0+(v1-v0)*k/32);sum+=prev.Distance(next);prev=next;}return sum;};
            const double step=radius_max*1.5/largest_segments;
            const auto curvature_segments=[&](bool along_u){
                double turning=0;gp_Vec previous;
                for(int k=0;k<=64;++k) {
                    gp_Pnt point;gp_Vec du,dv;
                    adaptor.D1(along_u?u0+(u1-u0)*k/64:u0,along_u?v0:v0+(v1-v0)*k/64,point,du,dv);
                    gp_Vec tangent=along_u?du:dv;
                    if(k && tangent.SquareMagnitude()>1.e-20 && previous.SquareMagnitude()>1.e-20)turning+=previous.Angle(tangent);
                    previous=tangent;
                }
                return int(std::ceil(turning/.25));
            };
            int nu=std::clamp(std::max(int(std::ceil(length(true)/step)),curvature_segments(true)),2,256);
            int nv=std::clamp(std::max(int(std::ceil(length(false)/step)),curvature_segments(false)),2,256);
            bool close_u=true,close_v=true;
            for(double t:{0.,.25,.5,.75,1.}) {
                close_u=close_u&&adaptor.Value(u0,v0+(v1-v0)*t).Distance(adaptor.Value(u1,v0+(v1-v0)*t))<tolerance;
                close_v=close_v&&adaptor.Value(u0+(u1-u0)*t,v0).Distance(adaptor.Value(u0+(u1-u0)*t,v1))<tolerance;
            }
            std::vector<Vec3> vertices,normals;std::vector<UV> uvs;std::vector<CMesh3D::Face> cells;
            for(int j=0;j<=nv;++j)for(int i=0;i<=nu;++i) {
                double u=u0+(u1-u0)*i/nu,v=v0+(v1-v0)*j/nv;
                gp_Pnt point;gp_Vec du,dv;adaptor.D1(u,v,point,du,dv);
                if(close_u&&i==nu)point=adaptor.Value(u0,v);
                if(close_v&&j==nv)point=adaptor.Value(u,v0);
                Vec3 n=normalize(xyz(gp_Pnt(du.Crossed(dv).XYZ())));
                if(face.Orientation()==TopAbs_REVERSED)n=n*-1.f;
                vertices.push_back(xyz(point));normals.push_back(n);uvs.push_back({float(u),float(v)});
            }
            // Collapse only the identified periodic seam, not nearby geometry.
            auto id=[&](int i,int j){if(close_u&&i==nu)i=0;if(close_v&&j==nv)j=0;return size_t(j*(nu+1)+i);};
            for(int j=0;j<nv;++j)for(int i=0;i<nu;++i) {
                CMesh3D::Face cell;
                for(auto k:{id(i,j),id(i+1,j),id(i+1,j+1),id(i,j+1)})cell.corners.push_back({k,k,k});
                if(face.Orientation()==TopAbs_REVERSED)std::reverse(cell.corners.begin(),cell.corners.end());
                cells.push_back(std::move(cell));
            }
            if(!draft.surface->pMesh3D->SetGeometry(std::move(vertices),std::move(cells),std::move(uvs),std::move(normals)))return fail("Invalid fillet background");
        }
        auto* mesh=draft.surface->pMesh3D;
        std::map<EdgeKey,int> incidence;
        for(const auto& cell:mesh->GetFaces()) if(!cell.deleted)
            for(size_t k=0;k<cell.corners.size();++k)
                ++incidence[std::minmax(cell.corners[k].v,cell.corners[(k+1)%cell.corners.size()].v)];
        const auto& vertices=mesh->GetVertices();
        for(const auto& item:incidence) {
            if(item.second==2)continue;
            if(item.second!=1)return fail("Nonmanifold background grid");
            auto a=vertices[item.first.first],b=vertices[item.first.second];
            std::vector<Route> best;double score=std::numeric_limits<double>::max();
            for(auto g:face_masters[f]) {
                auto pa=project(g,a),pb=project(g,b);
                if(pa.second<=projection_budget && pb.second<=projection_budget) {
                    double s=std::abs(difference(g,pa.first,pb.first))*masters[g].scale;
                    if(s<score){score=s;best={{g,pa.first,pa.first+difference(g,pa.first,pb.first)}};}
                }
            }
            // A clipped cell can cross a CAD corner between two circles. Keep
            // that topological vertex instead of cutting straight across it.
            if(best.empty()) for(auto ga:face_masters[f]) for(auto gb:face_masters[f]) {
                if(ga==gb)continue;
                auto pa=project(ga,a),pb=project(gb,b);
                if(pa.second>projection_budget || pb.second>projection_budget)continue;
                for(auto anchor:anchors) {
                    auto ca=project(ga,xyz(anchor)),cb=project(gb,xyz(anchor));
                    if(ca.second>tolerance || cb.second>tolerance)continue;
                    double s=std::abs(difference(ga,pa.first,ca.first))*masters[ga].scale+std::abs(difference(gb,cb.first,pb.first))*masters[gb].scale;
                    if(s<score && s<pnt(a).Distance(pnt(b))*1.5+tolerance) {
                        score=s;best={{ga,pa.first,pa.first+difference(ga,pa.first,ca.first)},
                            {gb,cb.first,cb.first+difference(gb,cb.first,pb.first)}};
                    }
                }
            }
            if(best.empty()) {
                // A cube cell can span several tiny CAD arcs at a split fillet.
                // Follow their shared topological vertices, including every arc.
                std::vector<Vec3> junctions={a,b};
                for(auto anchor:anchors) {
                    bool duplicate=false;for(auto q:junctions)duplicate=duplicate||pnt(q).Distance(anchor)<tolerance;
                    if(!duplicate)junctions.push_back(xyz(anchor));
                }
                const size_t n=junctions.size();
                std::vector<std::vector<std::pair<double,double>>> projected(n);
                for(size_t j=0;j<n;++j)for(auto g:face_masters[f])projected[j].push_back(project(g,junctions[j]));
                std::vector<double> distance(n,std::numeric_limits<double>::max());distance[0]=0;
                std::vector<bool> visited(n,false);std::vector<size_t> previous(n,n);std::vector<Route> via(n);
                for(size_t iteration=0;iteration<n;++iteration) {
                    size_t next=n;for(size_t j=0;j<n;++j)if(!visited[j]&&(next==n||distance[j]<distance[next]))next=j;
                    if(next==n||distance[next]==std::numeric_limits<double>::max()||next==1)break;
                    visited[next]=true;
                    for(size_t j=0;j<n;++j)if(!visited[j])for(size_t k=0;k<face_masters[f].size();++k) {
                        auto g=face_masters[f][k];auto pa=projected[next][k],pb=projected[j][k];
                        if(pa.second>(next<2?projection_budget:tolerance*2)||pb.second>(j<2?projection_budget:tolerance*2))continue;
                        double d=difference(g,pa.first,pb.first),length=std::abs(d)*masters[g].scale;
                        if(distance[next]+length<distance[j]){distance[j]=distance[next]+length;previous[j]=next;via[j]={g,pa.first,pa.first+d};}
                    }
                }
                if(distance[1]<pnt(a).Distance(pnt(b))*1.5+tolerance) {
                    for(size_t j=1;j!=0 && j<n;j=previous[j])best.push_back(via[j]);
                    std::reverse(best.begin(),best.end());
                }
            }
            if(best.empty())return fail("Boundary segment does not follow shared CAD curves");
            for(const auto& r:best) {
                masters[r.group].samples.push_back(parameter(r.group,r.first));
                masters[r.group].samples.push_back(parameter(r.group,r.last));
            }
            draft.routes[item.first]=std::move(best);
        }
        drafts.push_back(std::move(draft));
    }
    // Freeze each common curve's nodes before publishing any face. Both sides
    // consume the same coordinates, even when their cube lines cross it at
    // different angles. The circle supplies positions, never chord snapping.
    for(size_t g=0;g<masters.size();++g) {
        auto& m=masters[g];
        for(auto& t:m.samples)t=parameter(g,t);
        std::sort(m.samples.begin(),m.samples.end());
        std::vector<double> unique;
        for(double t:m.samples)if(unique.empty()||(t-unique.back())*m.scale>tolerance)unique.push_back(t);
        if((!m.curve || m.closed) && unique.size()>1 && (unique.front()+(m.curve?1.:tau)-unique.back())*m.scale<=tolerance)unique.pop_back();
        m.samples=std::move(unique);
        for(double t:m.samples) {
            auto p=evaluate(g,t);
            for(auto anchor:anchors)if(p.Distance(anchor)<tolerance*2){p=anchor;break;}
            m.points.push_back(xyz(p));
        }
    }
    for(auto& draft:drafts) {
        auto* mesh=draft.surface->pMesh3D;
        auto vertices=mesh->GetVertices();auto faces=mesh->GetFaces();
        std::map<EdgeKey,std::vector<size_t>> paths;
        std::map<size_t,size_t> aliases;
        const auto node = [&](Vec3 p) {
            for(size_t i=0;i<vertices.size();++i)if(pnt(vertices[i]).Distance(pnt(p))<=tolerance*2){vertices[i]=p;return i;}
            vertices.push_back(p);return vertices.size()-1;
        };
        for(const auto& entry:draft.routes) {
            std::vector<size_t> path;
            for(const auto& r:entry.second) {
                const auto& m=masters[r.group];
                std::vector<std::pair<double,size_t>> ordered;
                double length=r.last-r.first;
                for(size_t j=0;j<m.samples.size();++j) {
                    double fraction=difference(r.group,r.first,m.samples[j])/length;
                    double eps=tolerance*2/(std::abs(length)*m.scale);
                    if(fraction>=-eps && fraction<=1+eps)ordered.push_back({fraction,j});
                }
                std::sort(ordered.begin(),ordered.end());
                for(auto n:ordered) {auto index=node(m.points[n.second]);if(path.empty()||path.back()!=index)path.push_back(index);}
            }
            if(path.empty() || (path.size()==1
                && pnt(vertices[entry.first.first]).Distance(pnt(vertices[entry.first.second]))>tolerance*4))
                return fail("Collapsed shared boundary interval");
            // A float clipping sliver can have both ends canonicalized to the
            // same CAD node. Alias both ends and let polygon cleanup remove it.
            aliases[entry.first.first]=path.front();
            aliases[entry.first.second]=path.back();
            paths[entry.first]=std::move(path);
        }
        for(auto& cell:faces) {
            std::vector<MeshCorner> corners;
            for(size_t k=0;k<cell.corners.size();++k) {
                size_t a=cell.corners[k].v,b=cell.corners[(k+1)%cell.corners.size()].v;
                auto found=paths.find(std::minmax(a,b));
                if(found==paths.end())corners.push_back({a,a,0});
                else {
                    auto path=found->second;if(a>b)std::reverse(path.begin(),path.end());
                    for(size_t j=0;j+1<path.size();++j)corners.push_back({path[j],path[j],0});
                }
            }
            std::vector<MeshCorner> clean;
            for(auto c:corners) {
                auto alias=aliases.find(c.v);
                if(alias!=aliases.end())c.v=alias->second;
                c.n=c.v;c.uv=0;
                if(clean.empty()||clean.back().v!=c.v)clean.push_back(c);
            }
            if(clean.size()>1 && clean.front().v==clean.back().v)clean.pop_back();
            cell.deleted=clean.size()<3;
            cell.corners=std::move(clean);
        }
        BRepAdaptor_Surface adaptor(TopoDS::Face(draft.surface->m_Face));
        SurfaceUVMapping mapping(draft.surface.get());
        auto surface_normal=[&](Vec3 point){
            if(adaptor.GetType()==GeomAbs_Sphere)return normalize(point-xyz(adaptor.Sphere().Location()));
            SurfaceUVPoint uv;if(!mapping.Project(point,uv))return Vec3{};
            gp_Pnt p;gp_Vec du,dv;adaptor.D1(uv.u,uv.v,p,du,dv);
            Vec3 n=normalize(xyz(gp_Pnt(du.Crossed(dv).XYZ())));
            return draft.surface->m_Face.Orientation()==TopAbs_REVERSED?n*-1.f:n;
        };
        std::vector<CMesh3D::Face> split;
        for(const auto& cell:faces)if(!cell.deleted) {
            Vec3 center{};for(auto c:cell.corners)center=center+vertices[c.v];
            center=center/float(cell.corners.size());
            if(!split_sphere_boundary_cell(vertices,cell,surface_normal(center),split))return fail("Cannot form non-folded transition cells");
        }
        faces=std::move(split);
        // Discard the unused interior of each untrimmed background sphere.
        std::vector<Vec3> compact;
        std::map<size_t,size_t> used;
        for(auto& cell:faces)for(auto& c:cell.corners) {
            auto inserted=used.emplace(c.v,compact.size());
            if(inserted.second)compact.push_back(vertices[c.v]);
            c.v=inserted.first->second;c.n=c.v;
        }
        vertices=std::move(compact);
        std::vector<Vec3> normals;for(auto v:vertices)normals.push_back(surface_normal(v));
        std::vector<UV> uvs;
        for(auto& cell:faces) {
            double previous=0;bool first=true;
            for(auto& c:cell.corners) {
                SurfaceUVPoint uv;
                if(!mapping.Project(vertices[c.v],uv))return fail("Cannot project texture coordinates");
                if(!first && mapping.IsUPeriodic())uv.u+=mapping.UPeriod()*std::round((previous-uv.u)/mapping.UPeriod());
                previous=uv.u;first=false;
                c.uv=uvs.size();uvs.push_back({float(uv.u),float(uv.v)});
            }
        }
        if(!mesh->SetGeometry(std::move(vertices),std::move(faces),std::move(uvs),std::move(normals)))return fail("Invalid boundary cells");
    }
    // Exact float coordinates, without the display welder's adaptive tolerance.
    // Each directed edge must have exactly one oppositely oriented neighbour.
    std::map<std::tuple<float,float,float>,size_t> global_nodes;
    std::map<EdgeKey,std::pair<int,int>> incidence;
    for(const auto& draft:drafts) {
        const auto* mesh=draft.surface->pMesh3D;
        for(const auto& cell:mesh->GetFaces())if(!cell.deleted) {
            std::vector<size_t> ids;
            for(auto corner:cell.corners) {
                auto p=mesh->GetVertices()[corner.v];
                auto inserted=global_nodes.emplace(std::make_tuple(p.x,p.y,p.z),global_nodes.size());
                ids.push_back(inserted.first->second);
            }
            for(size_t k=0;k<ids.size();++k) {
                size_t a=ids[k],b=ids[(k+1)%ids.size()];
                auto& edge=incidence[std::minmax(a,b)];++edge.first;edge.second+=a<b?1:-1;
            }
        }
    }
    for(const auto& edge:incidence)if(edge.second.first!=2 || edge.second.second!=0)return fail("Shared boundary is not closed and oriented");
    for(size_t f=0;f<surfaces.size();++f) {
        auto* src=drafts[f].surface->pMesh3D;
        surfaces[f]->pMesh3D->SetGeometry(src->GetVertices(),src->GetFaces(),src->GetUVs(),src->GetNormals());
        surfaces[f]->IsInitMesh=true;surfaces[f]->IsTrimmed=true;surfaces[f]->m_TypeMesh=REGULAR_MESH;
        surfaces[f]->m_LastLowPolyDensity=1.f/deflection;
        surfaces[f]->m_LastIslandFillError.clear();
    }
    return Attempt::Built;
}

quadro::BodyMeshAttempt build_intersecting_spheres(
    const std::vector<CSurfaceFace*>& surfaces, float deflection)
{
    // Curved trims may enter and leave the same coarse cell edge. Retry with
    // finer background demands instead of publishing a folded transition.
    // The bound prevents pathological/tiny features from hanging Low Poly.
    for(int refinement=0;refinement<=8;++refinement) {
        auto result=try_build_intersecting_spheres(surfaces,deflection,refinement);
        if(result!=quadro::BodyMeshAttempt::Failed)return result;
        const auto& error=surfaces.front()->m_LastIslandFillError;
        if(error!="Sphere boundary: Cannot clip background grid"
            && error!="Sphere boundary: Cannot form non-folded transition cells"
            && error!="Sphere boundary: Collapsed shared boundary interval"
            && error!="Sphere boundary: Shared boundary is not closed and oriented")return result;
    }
    return quadro::BodyMeshAttempt::Failed;
}
