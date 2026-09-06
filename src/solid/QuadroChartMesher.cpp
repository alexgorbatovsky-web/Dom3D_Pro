#include "QuadroChartMesher.h"
#include "QuadroPatchMesh.h"
#include "../ContourQuadrangulator3DCoat.h"
#include "../SurfacePatchBuilder.h"
#include "../../third_party/earcut/earcut.hpp"
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <sstream>
#include <Standard_Failure.hxx>

namespace quadro {
namespace {
using MeshEdge=std::pair<Id,Id>;
double cross(const gp_Pnt2d& a,const gp_Pnt2d& b,const gp_Pnt2d& c) {
    return (b.X()-a.X())*(c.Y()-a.Y())-(b.Y()-a.Y())*(c.X()-a.X());
}
std::vector<gp_Pnt2d> macroKernel(const std::array<gp_Pnt2d,3>& corners,const std::array<gp_Pnt2d,3>& mids) {
    double u0=corners[0].X(),u1=u0,v0=corners[0].Y(),v1=v0;
    for(const auto& points:{corners,mids})for(const auto& p:points){u0=std::min(u0,p.X());u1=std::max(u1,p.X());v0=std::min(v0,p.Y());v1=std::max(v1,p.Y());}
    std::vector<gp_Pnt2d> kernel={{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
    const auto clip=[&](const gp_Pnt2d& p,const gp_Pnt2d& q) {
        std::vector<gp_Pnt2d> next;
        for(Id i=0;i<kernel.size();++i) {
            const auto x=kernel[i],y=kernel[(i+1)%kernel.size()];const double dx=cross(p,q,x),dy=cross(p,q,y);
            if(dx>=0)next.push_back(x);
            if((dx<0)!=(dy<0)){const double f=dx/(dx-dy);next.emplace_back(x.X()+f*(y.X()-x.X()),x.Y()+f*(y.Y()-x.Y()));}
        }
        kernel=std::move(next);
    };
    for(Id k=0;k<3;++k){clip(corners[k],mids[k]);clip(mids[(k+2)%3],corners[k]);clip(mids[(k+2)%3],mids[k]);}
    return kernel;
}

}
static ChartFillResult FillCadChartCandidate(const FaceBoundaryInput& chart,bool metric,bool exact,bool donorOnly=false) {
    ChartFillResult r;r.face=chart.face;r.stage="boundary";r.strategy=std::string(metric?"surface-metric":"native-uv")+(exact?"/reference":"/dom3d");
    const auto fail=[&](const char* message){r.error=message;return r;};
    if(!chart.ready||!chart.owner||chart.face>=chart.owner->faces().size())return fail("Validated CAD chart required");
    std::vector<std::vector<std::array<double,2>>> polygon;
    std::vector<Id> next;
    std::set<MeshEdge> boundary;
    double expectedArea=0;
    for(bool outer:{true,false})for(const auto& loop:chart.loops)if(loop.outer==outer) {
        if(loop.vertices.size()<3)return fail("Boundary loop too small");
        const Id start=r.sourceUV.size();polygon.emplace_back();
        for(const auto& v:loop.vertices) {
            r.sourceUV.push_back(v.uv);r.sourceNodes.push_back(v.node);
            polygon.back().push_back({v.uv.X(),v.uv.Y()});
        }
        for(Id i=start;i<r.sourceUV.size();++i) {
            const Id j=i+1==r.sourceUV.size()?start:i+1;next.push_back(j);boundary.insert(std::minmax(i,j));
        }
        expectedArea+=loop.signedArea;
    }
    if(r.sourceUV.size()>4000||!std::isfinite(expectedArea)||expectedArea<=0)return fail("Chart size or area outside budget");
    double xmin=r.sourceUV[0].X(),xmax=xmin,ymin=r.sourceUV[0].Y(),ymax=ymin;
    for(const auto& p:r.sourceUV){xmin=std::min(xmin,p.X());xmax=std::max(xmax,p.X());ymin=std::min(ymin,p.Y());ymax=std::max(ymax,p.Y());}
    const double scale=std::max(xmax-xmin,ymax-ymin),cx=(xmin+xmax)*.5,cy=(ymin+ymax)*.5;
    if(!(scale>0)||!std::isfinite(scale))return fail("Invalid UV extent");
    r.stage="triangulation";
    // Condition the triangulator's coordinates and remove collinear input
    // only in its private index list. The master boundary is restored below.
    std::vector<gp_Pnt2d> conditioned;
    for(const auto& uv:r.sourceUV)conditioned.emplace_back((uv.X()-xmin)/(xmax-xmin),(uv.Y()-ymin)/(ymax-ymin));
    std::vector<Id> triangulationSources;polygon.clear();
    std::vector<bool> visited(next.size(),false);
    for(Id start=0;start<next.size();++start)if(!visited[start]) {
        std::vector<Id> ring;Id i=start;do{ring.push_back(i);visited[i]=true;i=next[i];}while(i!=start);
        bool changed=true;
        while(changed&&ring.size()>3) {
            changed=false;
            for(Id k=0;k<ring.size();++k) {
                const auto& a=conditioned[ring[(k+ring.size()-1)%ring.size()]];const auto& b=conditioned[ring[k]];const auto& c=conditioned[ring[(k+1)%ring.size()]];
                if(std::abs(cross(a,b,c))<=1.e-14 && (b.X()-a.X())*(b.X()-c.X())+(b.Y()-a.Y())*(b.Y()-c.Y())<=1.e-14) {
                    ring.erase(ring.begin()+k);changed=true;break;
                }
            }
        }
        polygon.emplace_back();for(Id v:ring){polygon.back().push_back({conditioned[v].X(),conditioned[v].Y()});triangulationSources.push_back(v);}
    }
    const auto indices=mapbox::earcut<Id>(polygon);
    for(Id i=0;i+2<indices.size();i+=3) {
        std::array<Id,3> t={triangulationSources[indices[i]],triangulationSources[indices[i+1]],triangulationSources[indices[i+2]]};
        if(cross(r.sourceUV[t[0]],r.sourceUV[t[1]],r.sourceUV[t[2]])==0)continue;
        if(cross(r.sourceUV[t[0]],r.sourceUV[t[1]],r.sourceUV[t[2]])<0)std::swap(t[1],t[2]);
        r.triangles.push_back(t);
    }
    if(r.triangles.empty())return fail("No donor triangles");
    // Earcut may remove collinear vertices. Restore every original boundary
    // segment by splitting its incident triangle, using source indices only.
    for(Id pass=0;pass<=r.sourceUV.size();++pass) {
        std::map<MeshEdge,std::pair<Id,unsigned>> uses;
        std::vector<bool> used(r.sourceUV.size(),false);
        for(const auto& t:r.triangles)for(Id v:t)used[v]=true;
        for(Id i=0;i<r.triangles.size();++i)for(Id k=0;k<3;++k){auto& u=uses[std::minmax(r.triangles[i][k],r.triangles[i][(k+1)%3])];u.first=i;++u.second;}
        bool repaired=false;
        for(const auto& [edge,use]:uses)if(use.second==1&&!boundary.count(edge)) {
            std::vector<Id> chain;
            for(bool reverse:{false,true}) {
                const Id a=reverse?edge.second:edge.first,b=reverse?edge.first:edge.second;
                std::vector<Id> candidate{a};Id i=next[a];
                while(i!=a&&i!=b&&candidate.size()<r.sourceUV.size()) {
                    // Restore an omitted source chain by its loop identity.
                    // Never consume an already occupied vertex (in particular
                    // the opposite vertex of this triangle) into the fan.
                    if(used[i])break;
                    candidate.push_back(i);i=next[i];
                }
                if(i==b&&candidate.size()>1){candidate.push_back(b);chain=std::move(candidate);break;}
            }
            if(chain.empty())return fail("Triangulation skipped an occupied CAD boundary chain");
            const auto old=r.triangles[use.first];Id opposite=invalidId;
            for(Id v:old)if(v!=edge.first&&v!=edge.second)opposite=v;
            if(opposite==invalidId)return fail("Invalid donor triangle");
            for(Id i=0;i+1<chain.size();++i) {
                std::array<Id,3> t={chain[i],chain[i+1],opposite};
                if(cross(r.sourceUV[t[0]],r.sourceUV[t[1]],r.sourceUV[t[2]])<0)std::swap(t[1],t[2]);
                if(i==0)r.triangles[use.first]=t;else r.triangles.push_back(t);
            }
            repaired=true;break;
        }
        if(!repaired)break;
        if(pass==r.sourceUV.size())return fail("Boundary restoration budget exceeded");
    }
    std::map<MeshEdge,std::pair<unsigned,int>> donorEdges;double area=0;
    for(const auto& t:r.triangles) {
        const auto& a=r.sourceUV[t[0]];const auto& b=r.sourceUV[t[1]];const auto& c=r.sourceUV[t[2]];
        const double twice=cross(a,b,c);
        if(twice<=0||!ContainsUV(chart,gp_Pnt2d((a.X()+b.X()+c.X())/3,(a.Y()+b.Y()+c.Y())/3)))return fail("Invalid donor triangle or covered hole");
        area+=twice*.5;
        for(Id k=0;k<3;++k){Id x=t[k],y=t[(k+1)%3];auto& e=donorEdges[std::minmax(x,y)];++e.first;e.second+=x<y?1:-1;}
    }
    for(const auto& [edge,use]:donorEdges)if(boundary.count(edge)?use.first!=1:(use.first!=2||use.second!=0))return fail("Donor violates CAD boundary incidence");
    for(const auto& edge:boundary)if(!donorEdges.count(edge))return fail("Donor dropped a CAD boundary segment");
    if(std::abs(area-expectedArea)>expectedArea*1.e-8)return fail("Donor does not cover the chart area");
    r.donorReady=true;
    if(donorOnly){r.stage="donor";return r;}
    r.stage="quadrangulator";
    // The legacy front becomes prohibitively expensive on densely constrained
    // contours (including the refined F9). Route them to bounded patch fills.
    if(chart.loops.size()==1&&r.sourceUV.size()>384)return fail("Legacy front boundary budget exceeded; patch fill required");
    double mu=1,mv=1;
    if(metric)try {
        const double hu=(xmax-xmin)*1.e-4,hv=(ymax-ymin)*1.e-4;
        mu=chart.owner->evaluateSurface(chart.face,gp_Pnt2d(cx+hu,cy)).Distance(chart.owner->evaluateSurface(chart.face,gp_Pnt2d(cx-hu,cy)))/(2*hu);
        mv=chart.owner->evaluateSurface(chart.face,gp_Pnt2d(cx,cy+hv)).Distance(chart.owner->evaluateSurface(chart.face,gp_Pnt2d(cx,cy-hv)))/(2*hv);
        if(!std::isfinite(mu)||!std::isfinite(mv)||mu<=0||mv<=0)return fail("Singular local surface metric");
    }catch(const Standard_Failure&){return fail("Cannot evaluate local surface metric");}
    const double workScale=std::max((xmax-xmin)*mu,(ymax-ymin)*mv);
    const auto toWork=[&](const gp_Pnt2d& uv){return Vec3{float((uv.X()-cx)*mu/workScale),float((uv.Y()-cy)*mv/workScale),0};};
    std::vector<Vec3> input;std::vector<CMesh3D::Face> triangles;
    std::map<std::pair<float,float>,Id> sourceByPosition;
    for(Id i=0;i<r.sourceUV.size();++i) {
        const Vec3 p=toWork(r.sourceUV[i]);input.push_back(p);
        if(!sourceByPosition.emplace(std::make_pair(p.x,p.y),i).second)return fail("Float input merges distinct CAD boundary instances");
    }
    for(const auto& t:r.triangles){CMesh3D::Face f;for(Id v:t)f.corners.push_back({v,0,0});triangles.push_back(std::move(f));}
    CMesh3D candidate;
    if(chart.loops.size()==1) {
        r.patchCount=1;
        if(!Build3DCoatQuadrangulation(input,triangles,&candidate,exact))return fail("Quadrangulator did not create cells");
    } else {
        r.stage="patches";
        std::vector<std::vector<SurfacePatchPoint>> contours,parts;
        for(const auto& loop:chart.loops){contours.emplace_back();for(const auto& v:loop.vertices)contours.back().push_back({v.uv.X(),v.uv.Y(),true});}
        std::string reason;
        if(!BuildSurfacePatchesWithoutHoles(contours,parts,&reason))return fail("Cannot decompose CAD chart around holes");
        r.patchCount=parts.size();
        std::map<std::pair<double,double>,Id> original;
        for(Id i=0;i<r.sourceUV.size();++i)if(!original.emplace(std::make_pair(r.sourceUV[i].X(),r.sourceUV[i].Y()),i).second)return fail("Ambiguous source UV identity");
        std::map<std::tuple<Id,Id,Id,Id>,gp_Pnt2d> cutNodes;
        std::map<std::pair<double,double>,Id> assembled;
        std::vector<Vec3> points;std::vector<CMesh3D::Face> faces;
        Id partIndex=0;
        for(const auto& part:parts) {
            std::vector<Id> anchors(part.size(),invalidId);
            for(Id i=0;i<part.size();++i){const auto it=original.find({part[i].u,part[i].v});if(it!=original.end())anchors[i]=it->second;}
            FaceBoundaryInput patch;patch.owner=chart.owner;patch.face=chart.face;patch.ready=true;patch.reversed=false;
            ChartLoop loop;loop.outer=true;loop.wire=invalidId;
            for(Id i=0;i<part.size();++i) {
                gp_Pnt2d uv(part[i].u,part[i].v);Id node=invalidId;
                if(anchors[i]!=invalidId){uv=r.sourceUV[anchors[i]];node=r.sourceNodes[anchors[i]];}
                else {
                    Id prev=i,nextIndex=i,back=0,forward=0;
                    do{prev=(prev+part.size()-1)%part.size();++back;}while(anchors[prev]==invalidId&&back<part.size());
                    do{nextIndex=(nextIndex+1)%part.size();++forward;}while(anchors[nextIndex]==invalidId&&forward<part.size());
                    if(back==part.size()||forward==part.size())return fail("Cut has no CAD anchors");
                    const Id a=anchors[prev],b=anchors[nextIndex],n=back+forward,k=a<b?back:forward;
                    const auto key=std::make_tuple(std::min(a,b),std::max(a,b),n,k);
                    const auto& x=r.sourceUV[std::min(a,b)];const auto& y=r.sourceUV[std::max(a,b)];
                    const auto canonical=gp_Pnt2d(x.X()+(y.X()-x.X())*double(k)/n,x.Y()+(y.Y()-x.Y())*double(k)/n);
                    if(uv.Distance(canonical)>scale*1.e-10)return fail("Cut point is not on its identified shared bridge");
                    uv=cutNodes.emplace(key,canonical).first->second;
                }
                loop.vertices.push_back({node,invalidId,0,uv});
            }
            const auto origin=loop.vertices.front().uv;
            for(Id i=0;i<loop.vertices.size();++i)loop.signedArea+=cross(origin,loop.vertices[i].uv,loop.vertices[(i+1)%loop.vertices.size()].uv)*.5;
            if(loop.signedArea<0){std::reverse(loop.vertices.begin(),loop.vertices.end());loop.signedArea=-loop.signedArea;}
            r.patchContours.emplace_back();for(const auto& v:loop.vertices)r.patchContours.back().push_back(v.uv);
            patch.loops.push_back(std::move(loop));
            const auto filled=FillCadChart(patch);
            if(!filled.ready()){
                r.failedPatch=partIndex;r.candidateUV=filled.candidateUV;r.candidateCells=filled.candidateCells;
                r.error="Cut patch "+std::to_string(partIndex)+": "+filled.stage+": "+filled.error;return r;
            }
            std::vector<Id> remap(filled.uv.size());
            for(Id i=0;i<filled.uv.size();++i) {
                const auto uv=filled.uv[i];const auto [it,inserted]=assembled.emplace(std::make_pair(uv.X(),uv.Y()),points.size());
                if(inserted)points.push_back(toWork(uv));
                remap[i]=it->second;
            }
            for(const auto& q:filled.quads){CMesh3D::Face f;for(Id i:q)f.corners.push_back({remap[i],0,0});faces.push_back(std::move(f));}
            ++partIndex;
        }
        if(!candidate.SetGeometry(std::move(points),std::move(faces)))return fail("Could not assemble cut patches");
    }
    r.stage="admission";
    std::map<MeshEdge,std::pair<unsigned,int>> edges;
    for(const auto& p:candidate.GetVertices())if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return fail("Non-finite quadrangulator vertex");
    for(const auto& p:candidate.GetVertices())r.candidateUV.emplace_back(double(p.x)*workScale/mu+cx,double(p.y)*workScale/mv+cy);
    for(const auto& f:candidate.GetFaces())if(!f.deleted){r.candidateCells.emplace_back();for(const auto& c:f.corners)r.candidateCells.back().push_back(c.v);}
    for(const auto& f:candidate.GetFaces())if(!f.deleted) {
        if(f.corners.size()!=4)return fail("Quadrangulator output contains non-quad cells");
        std::array<Id,4> q;for(Id k=0;k<4;++k){q[k]=f.corners[k].v;if(q[k]>=candidate.GetVertices().size())return fail("Invalid output index");}
        r.quads.push_back(q);
        for(Id k=0;k<4;++k){Id x=q[k],y=q[(k+1)%4];auto& e=edges[std::minmax(x,y)];++e.first;e.second+=x<y?1:-1;}
    }
    std::set<MeshEdge> foundBoundary;std::vector<Id> source(candidate.GetVertices().size(),invalidId);
    for(const auto& [edge,use]:edges) {
        if(use.first==2&&use.second==0)continue;
        if(use.first!=1)return fail("Non-manifold or inconsistently oriented output");
        for(Id v:{edge.first,edge.second}) {
            const auto p=candidate.GetVertices()[v];const auto it=sourceByPosition.find({p.x,p.y});
            if(it==sourceByPosition.end())return fail("Quadrangulator moved or inserted a boundary node");
            source[v]=it->second;
        }
        if(!foundBoundary.insert(std::minmax(source[edge.first],source[edge.second])).second)return fail("Duplicated output boundary segment");
    }
    if(foundBoundary!=boundary)return fail("Quadrangulator changed CAD boundary segmentation or holes");
    try {
        for(Id i=0;i<source.size();++i) {
            const auto p=candidate.GetVertices()[i];
            auto uv=gp_Pnt2d(double(p.x)*workScale/mu+cx,double(p.y)*workScale/mv+cy);
            Id node=invalidId;
            if(source[i]!=invalidId){uv=r.sourceUV[source[i]];node=r.sourceNodes[source[i]];}
            r.uv.push_back(uv);r.masterNodes.push_back(node);
            r.xyz.push_back(node==invalidId?chart.owner->evaluateSurface(chart.face,uv):chart.owner->nodes()[node].xyz);
            const auto& xyz=r.xyz.back();if(!std::isfinite(xyz.X())||!std::isfinite(xyz.Y())||!std::isfinite(xyz.Z()))return fail("Non-finite CAD vertex");
        }
    }catch(const Standard_Failure&){return fail("CAD evaluation failed");}
    area=0;
    for(auto& q:r.quads) {
        double x=0,y=0;
        // Normalize a globally reversed output to the intrinsic forward chart.
        if(cross(r.uv[q[0]],r.uv[q[1]],r.uv[q[2]])<0)std::reverse(q.begin(),q.end());
        for(Id k=0;k<4;++k) {
            if(cross(r.uv[q[k]],r.uv[q[(k+1)%4]],r.uv[q[(k+2)%4]])<=0)return fail("Folded or concave UV quad");
            x+=r.uv[q[k]].X();y+=r.uv[q[k]].Y();
        }
        if(!ContainsUV(chart,gp_Pnt2d(x/4,y/4)))return fail("Quad covers a hole or leaves the chart");
        area+=(cross(r.uv[q[0]],r.uv[q[1]],r.uv[q[2]])+cross(r.uv[q[0]],r.uv[q[2]],r.uv[q[3]]))*.5;
        const gp_Vec a(r.xyz[q[0]],r.xyz[q[1]]),b(r.xyz[q[0]],r.xyz[q[2]]),c(r.xyz[q[0]],r.xyz[q[3]]);
        if(a.Crossed(b).Dot(b.Crossed(c))<=0)return fail("Folded spatial quad");
        if(chart.reversed)std::swap(q[1],q[3]);
    }
    if(std::abs(area-expectedArea)>expectedArea*1.e-7)return fail("Quad mesh does not cover the chart area");
    r.stage="ready";return r;
}
static StructuredPatch BuildRuledPatch(const FaceBoundaryInput& chart,const std::array<Id,4>& corners) {
    auto p=BuildStructuredPatchAtCorners(chart,corners);
    if(p.uv.empty()||p.uv.size()>16384)return p;
    const Id nu=p.columns-1,nv=p.rows-1;
    const auto index=[&](Id i,Id j){return j*(nu+1)+i;};
    const auto fraction=[](const gp_Pnt2d& p,const gp_Pnt2d& a,const gp_Pnt2d& b){const double length=a.SquareDistance(b);return length>0?((p.X()-a.X())*(b.X()-a.X())+(p.Y()-a.Y())*(b.Y()-a.Y()))/length:0.0;};
    const auto a=p.uv[index(0,0)],b=p.uv[index(nu,0)],c=p.uv[index(nu,nv)],d=p.uv[index(0,nv)];
    for(Id j=1;j<nv;++j)for(Id i=1;i<nu;++i) {
        gp_Pnt2d left,right;double weight;
        if(nv>=nu) {
            const double v=double(j)/nv;
            weight=(1-v)*fraction(p.uv[index(i,0)],a,b)+v*fraction(p.uv[index(i,nv)],d,c);
            left=p.uv[index(0,j)];right=p.uv[index(nu,j)];
        }else {
            const double u=double(i)/nu;
            weight=(1-u)*fraction(p.uv[index(0,j)],a,d)+u*fraction(p.uv[index(nu,j)],b,c);
            left=p.uv[index(i,0)];right=p.uv[index(i,nv)];
        }
        p.uv[index(i,j)]=gp_Pnt2d(left.X()+weight*(right.X()-left.X()),left.Y()+weight*(right.Y()-left.Y()));
    }
    return ValidateStructuredPatch(chart,std::move(p));
}

static StructuredPatch BuildInverseHarmonicPatch(const FaceBoundaryInput& chart,const std::array<Id,4>& corners) {
    auto patch=BuildStructuredPatchAtCorners(chart,corners);
    const auto fail=[&](const char* error){patch.error=error;return patch;};
    if(patch.uv.empty()||patch.uv.size()>16384)return fail("Inverse chart grid outside budget");
    const auto donor=FillCadChartCandidate(chart,true,false,true);
    if(!donor.donorReady||donor.triangles.size()>2000)return fail("Inverse chart donor unavailable");
    const Id nu=patch.columns-1,nv=patch.rows-1,n=donor.sourceUV.size();
    std::map<std::pair<double,double>,gp_Pnt2d> boundary;
    for(Id j=0;j<=nv;++j)for(Id i=0;i<=nu;++i)if(i==0||j==0||i==nu||j==nv) {
        const auto& p=patch.uv[j*(nu+1)+i];boundary[{p.X(),p.Y()}]=gp_Pnt2d(double(i)/nu,double(j)/nv);
    }
    std::vector<gp_Pnt2d> native=donor.sourceUV,rectangle;
    for(const auto& p:native){const auto it=boundary.find({p.X(),p.Y()});if(it==boundary.end())return fail("Inverse chart boundary instance missing");rectangle.push_back(it->second);}
    for(const auto& t:donor.triangles) {
        const double wa=native[t[1]].Distance(native[t[2]]),wb=native[t[2]].Distance(native[t[0]]),wc=native[t[0]].Distance(native[t[1]]),sum=wa+wb+wc;
        const auto average=[&](const auto& points){return gp_Pnt2d((wa*points[t[0]].X()+wb*points[t[1]].X()+wc*points[t[2]].X())/sum,(wa*points[t[0]].Y()+wb*points[t[1]].Y()+wc*points[t[2]].Y())/sum);};
        native.push_back(average(native));rectangle.push_back(average(rectangle));
    }
    struct Use {Id a,b,center;};std::map<MeshEdge,std::vector<Use>> uses;
    for(Id i=0;i<donor.triangles.size();++i)for(Id k=0;k<3;++k){const auto& t=donor.triangles[i];uses[std::minmax(t[k],t[(k+1)%3])].push_back({t[k],t[(k+1)%3],n+i});}
    std::vector<std::array<Id,3>> triangles;
    for(const auto& [edge,adjacent]:uses) {
        const auto& u=adjacent[0];
        if(adjacent.size()==2) {
            const auto& v=adjacent[1];
            if(cross(native[u.a],native[v.center],native[u.center])>0&&cross(native[u.b],native[u.center],native[v.center])>0) {
                triangles.push_back({u.a,v.center,u.center});triangles.push_back({u.b,u.center,v.center});continue;
            }
        }
        for(const auto& v:adjacent)triangles.push_back({v.a,v.b,v.center});
    }
    std::vector<std::map<Id,double>> neighbors(native.size());
    for(const auto& t:triangles)for(Id k=0;k<3;++k) {
        const Id i=t[k],j=t[(k+1)%3],l=t[(k+2)%3];if(i<n)continue;
        const auto& a=native[i];const auto& b=native[j];const auto& c=native[l];
        const double ab=a.Distance(b),ac=a.Distance(c),denominator=ab*ac+(b.X()-a.X())*(c.X()-a.X())+(b.Y()-a.Y())*(c.Y()-a.Y());
        if(!(ab>0&&ac>0&&denominator>0))return fail("Degenerate mean-value chart angle");
        const double tangent=std::abs(cross(a,b,c))/denominator;
        neighbors[i][j]+=tangent/ab;neighbors[i][l]+=tangent/ac;
    }
    bool converged=false;
    for(unsigned iteration=0;iteration<10000;++iteration) {
        double residual=0;
        for(Id i=n;i<rectangle.size();++i) {
            double x=0,y=0,sum=0;for(const auto& [j,weight]:neighbors[i]){x+=weight*rectangle[j].X();y+=weight*rectangle[j].Y();sum+=weight;}
            if(!(sum>0))return fail("Disconnected inverse chart node");
            const auto old=rectangle[i];const gp_Pnt2d next(x/sum,y/sum);
            residual=std::max(residual,old.Distance(next));rectangle[i]=next;
        }
        if(residual<1.e-11){converged=true;break;}
    }
    if(!converged)return fail("Inverse chart iteration budget exceeded");
    for(const auto& t:triangles)if(!(cross(rectangle[t[0]],rectangle[t[1]],rectangle[t[2]])>0))return fail("Inverse chart has a collapsed triangle");
    for(Id j=1;j<nv;++j)for(Id i=1;i<nu;++i) {
        const gp_Pnt2d p(double(i)/nu,double(j)/nv);bool located=false;
        for(const auto& t:triangles) {
            const auto& a=rectangle[t[0]];const auto& b=rectangle[t[1]];const auto& c=rectangle[t[2]];const double area=cross(a,b,c);
            const double wa=cross(p,b,c)/area,wb=cross(p,c,a)/area,wc=1-wa-wb;
            if(std::min({wa,wb,wc})<-1.e-10)continue;
            patch.uv[j*(nu+1)+i]=gp_Pnt2d(wa*native[t[0]].X()+wb*native[t[1]].X()+wc*native[t[2]].X(),wa*native[t[0]].Y()+wb*native[t[1]].Y()+wc*native[t[2]].Y());located=true;break;
        }
        if(!located)return fail("Inverse chart grid point not located");
    }
    return ValidateStructuredPatch(chart,std::move(patch));
}

static StructuredPatch UntangleStructuredPatch(const FaceBoundaryInput& chart,StructuredPatch p) {
    if(p.uv.empty()||p.uv.size()>2048)return p;
    const Id nx=p.columns,ny=p.rows,n=p.uv.size();
    double u0=p.uv[0].X(),u1=u0,v0=p.uv[0].Y(),v1=v0;
    for(const auto& v:p.uv){u0=std::min(u0,v.X());u1=std::max(u1,v.X());v0=std::min(v0,v.Y());v1=std::max(v1,v.Y());}
    if(!(u1>u0&&v1>v0))return p;
    std::vector<gp_Pnt2d> points;std::vector<bool> fixed(n);
    for(Id i=0;i<n;++i){points.emplace_back((p.uv[i].X()-u0)/(u1-u0),(p.uv[i].Y()-v0)/(v1-v0));fixed[i]=i%nx==0||i%nx==nx-1||i<nx||i>=n-nx;}
    std::vector<std::array<Id,4>> cells;
    for(Id j=0;j+1<ny;++j)for(Id i=0;i+1<nx;++i)cells.push_back({j*nx+i,j*nx+i+1,(j+1)*nx+i+1,(j+1)*nx+i});
    constexpr double target=1.e-10;
    std::vector<std::vector<std::array<Id,3>>> incident(n);
    for(const auto& q:cells)for(Id k=0;k<4;++k){const std::array<Id,3> t={q[k],q[(k+1)%4],q[(k+2)%4]};for(Id i:t)if(!fixed[i])incident[i].push_back(t);}
    for(unsigned sweep=0;sweep<160;++sweep) {
        double movement=0;
        for(Id order=0;order<n;++order) {
            const Id i=sweep%2?n-1-order:order;if(fixed[i])continue;
            std::vector<std::array<double,3>> planes;double minimum=1;
            for(const auto& t:incident[i]) {
                const auto& a=points[t[0]];const auto& b=points[t[1]];const auto& c=points[t[2]];const double area=cross(a,b,c);
                minimum=std::min(minimum,area);double dx,dy;
                if(i==t[0]){dx=b.Y()-c.Y();dy=c.X()-b.X();}else if(i==t[1]){dx=c.Y()-a.Y();dy=a.X()-c.X();}else{dx=a.Y()-b.Y();dy=b.X()-a.X();}
                planes.push_back({dx,dy,area-dx*points[i].X()-dy*points[i].Y()});
            }
            if(minimum>=target)continue;
            double requested=target;std::vector<gp_Pnt2d> polygon;
            for(unsigned relaxation=0;relaxation<8;++relaxation) {
                polygon={{0,0},{1,0},{1,1},{0,1}};
                for(const auto& plane:planes) {
                    std::vector<gp_Pnt2d> next;
                    for(Id k=0;k<polygon.size();++k) {
                        const auto a=polygon[k],b=polygon[(k+1)%polygon.size()];const double da=plane[0]*a.X()+plane[1]*a.Y()+plane[2]-requested,db=plane[0]*b.X()+plane[1]*b.Y()+plane[2]-requested;
                        if(da>=0)next.push_back(a);
                        if((da<0)!=(db<0)){const double t=da/(da-db);next.emplace_back(a.X()+t*(b.X()-a.X()),a.Y()+t*(b.Y()-a.Y()));}
                    }
                    polygon=std::move(next);if(polygon.empty())break;
                }
                if(!polygon.empty())break;requested=(requested+minimum)*.5;
            }
            if(polygon.empty())continue;
            gp_Pnt2d closest=polygon[0];double distance=closest.SquareDistance(points[i]),x=0,y=0;
            for(Id k=0;k<polygon.size();++k) {
                const auto a=polygon[k],b=polygon[(k+1)%polygon.size()];x+=a.X();y+=a.Y();const double length=a.SquareDistance(b);
                const double t=length>0?std::clamp(((points[i].X()-a.X())*(b.X()-a.X())+(points[i].Y()-a.Y())*(b.Y()-a.Y()))/length,0.0,1.0):0;
                const gp_Pnt2d candidate(a.X()+t*(b.X()-a.X()),a.Y()+t*(b.Y()-a.Y()));const double d=candidate.SquareDistance(points[i]);if(d<distance){distance=d;closest=candidate;}
            }
            const gp_Pnt2d next(.99*closest.X()+.01*x/polygon.size(),.99*closest.Y()+.01*y/polygon.size());movement=std::max(movement,next.Distance(points[i]));points[i]=next;
        }
        if(movement<1.e-13)break;
    }
    using Gradient=std::vector<std::array<double,2>>;
    const auto energy=[&](const auto& x,Gradient* gradient) {
        if(gradient)gradient->assign(n,{0,0});double loss=0;
        for(const auto& q:cells)for(Id k=0;k<4;++k) {
            const Id ia=q[k],ib=q[(k+1)%4],ic=q[(k+2)%4];const auto& a=x[ia];const auto& b=x[ib];const auto& c=x[ic];
            const double deficit=std::min(0.0,cross(a,b,c)-target);loss+=deficit*deficit;
            if(!gradient||deficit==0)continue;
            const auto add=[&](Id id,double dx,double dy){if(!fixed[id]){(*gradient)[id][0]+=2*deficit*dx;(*gradient)[id][1]+=2*deficit*dy;}};
            add(ia,b.Y()-c.Y(),c.X()-b.X());add(ib,c.Y()-a.Y(),a.X()-c.X());add(ic,a.Y()-b.Y(),b.X()-a.X());
        }
        return loss;
    };
    Gradient gradient,direction;double loss=energy(points,&gradient);direction=gradient;for(auto& v:direction){v[0]=-v[0];v[1]=-v[1];}
    bool converged=false;
    for(unsigned iteration=0;iteration<3000;++iteration) {
        double minimum=1;for(const auto& q:cells)for(Id k=0;k<4;++k)minimum=std::min(minimum,cross(points[q[k]],points[q[(k+1)%4]],points[q[(k+2)%4]]));
        if(minimum>target*.1){converged=true;break;}
        double slope=0,maximum=0;for(Id i=0;i<n;++i)for(unsigned k=0;k<2;++k)slope+=gradient[i][k]*direction[i][k];
        if(slope>=0){direction=gradient;for(auto& v:direction){v[0]=-v[0];v[1]=-v[1];}}
        for(const auto& v:direction)maximum=std::max({maximum,std::abs(v[0]),std::abs(v[1])});
        if(!(maximum>0))break;
        double step=.02/maximum,nextLoss=loss;std::vector<gp_Pnt2d> trial;bool improved=false;
        for(unsigned search=0;search<40;++search) {
            trial=points;for(Id i=0;i<n;++i)if(!fixed[i])trial[i].SetCoord(points[i].X()+step*direction[i][0],points[i].Y()+step*direction[i][1]);
            nextLoss=energy(trial,nullptr);if(nextLoss<loss){improved=true;break;}step*=.5;
        }
        if(!improved)break;
        Gradient nextGradient;energy(trial,&nextGradient);double numerator=0,denominator=0;
        for(Id i=0;i<n;++i)for(unsigned k=0;k<2;++k){numerator+=nextGradient[i][k]*(nextGradient[i][k]-gradient[i][k]);denominator+=gradient[i][k]*gradient[i][k];}
        const double beta=denominator>0?std::clamp(numerator/denominator,0.0,10.0):0;
        for(Id i=0;i<n;++i)for(unsigned k=0;k<2;++k)direction[i][k]=-nextGradient[i][k]+beta*direction[i][k];
        points=std::move(trial);gradient=std::move(nextGradient);loss=nextLoss;
    }
    if(!converged){p.error="Structured untangling did not converge";return p;}
    for(Id i=0;i<n;++i)if(!fixed[i])p.uv[i]=gp_Pnt2d(u0+points[i].X()*(u1-u0),v0+points[i].Y()*(v1-v0));
    return ValidateStructuredPatch(chart,std::move(p));
}

static ChartFillResult FillTriangularPatches(const FaceBoundaryInput& chart,unsigned refinement=0,unsigned phase=0,bool boundaryAware=false) {
    ChartFillResult r;r.face=chart.face;r.strategy="triangle-patches";r.stage="triangle-patches";
    const auto fail=[&](const char* reason){r.error=reason;return r;};
    FaceBoundaryInput coarse=chart;
    std::map<MeshEdge,ChartVertex> boundaryMidpoints;
    Id offset=0;
    // A macro edge consists of exactly two existing boundary segments. Its
    // midpoint is the original CAD sample, never a new face-local projection.
    for(bool outer:{true,false})for(auto& loop:coarse.loops)if(loop.outer==outer) {
        auto ring=loop.vertices;
        if(ring.size()<6||ring.size()%2)return fail("Macro boundary requires even segment counts");
        if(phase)std::rotate(ring.begin(),ring.begin()+1,ring.end());
        loop.vertices.clear();loop.signedArea=0;
        for(Id i=0;i<ring.size();i+=2) {
            loop.vertices.push_back(ring[i]);
            boundaryMidpoints[std::minmax(offset+i/2,offset+(i/2+1)%(ring.size()/2))]=ring[i+1];
        }
        for(Id i=0;i<loop.vertices.size();++i)loop.signedArea+=cross(loop.vertices[0].uv,loop.vertices[i].uv,loop.vertices[(i+1)%loop.vertices.size()].uv)*.5;
        offset+=loop.vertices.size();
    }
    auto donor=FillCadChartCandidate(coarse,true,false,true);
    if(!donor.donorReady){r.error="Macro triangulation failed: "+donor.error;return r;}
    // Remove skinny donor ears by flipping internal diagonals only. Boundary
    // chains restored by the donor can be almost collinear in CAD UV.
    const auto optimize=[&]() {
    double u0=donor.sourceUV[0].X(),u1=u0,v0=donor.sourceUV[0].Y(),v1=v0;
    for(const auto& p:donor.sourceUV){u0=std::min(u0,p.X());u1=std::max(u1,p.X());v0=std::min(v0,p.Y());v1=std::max(v1,p.Y());}
    std::vector<gp_Pnt2d> normalized;for(const auto& p:donor.sourceUV)normalized.emplace_back((p.X()-u0)/(u1-u0),(p.Y()-v0)/(v1-v0));
    const auto quality=[&](const std::array<Id,3>& t){const auto& a=normalized[t[0]];const auto& b=normalized[t[1]];const auto& c=normalized[t[2]];return cross(a,b,c)/(a.SquareDistance(b)+b.SquareDistance(c)+c.SquareDistance(a));};
    const auto macroQuality=[&](const std::array<Id,3>& t) {
        if(!boundaryAware)return quality(t);
        double score=quality(t);std::array<gp_Pnt2d,3> mid;
        for(Id k=0;k<3;++k) {
            const auto it=boundaryMidpoints.find(std::minmax(t[k],t[(k+1)%3]));
            if(it==boundaryMidpoints.end()){const auto a=normalized[t[k]],b=normalized[t[(k+1)%3]];mid[k]=gp_Pnt2d((a.X()+b.X())*.5,(a.Y()+b.Y())*.5);}
            else mid[k]=gp_Pnt2d((it->second.uv.X()-u0)/(u1-u0),(it->second.uv.Y()-v0)/(v1-v0));
        }
        for(Id k=0;k<3;++k) {
            const auto& a=mid[(k+2)%3];const auto& b=normalized[t[k]];const auto& c=mid[k];
            score=std::min(score,cross(a,b,c)/(a.SquareDistance(b)+b.SquareDistance(c)+c.SquareDistance(a)));
        }
        if(score>0&&macroKernel({normalized[t[0]],normalized[t[1]],normalized[t[2]]},mid).empty())return -1.0;
        return score;
    };
    for(unsigned pass=0;pass<128;++pass) {
        std::map<MeshEdge,std::vector<Id>> adjacency;
        for(Id i=0;i<donor.triangles.size();++i)for(Id k=0;k<3;++k)adjacency[std::minmax(donor.triangles[i][k],donor.triangles[i][(k+1)%3])].push_back(i);
        std::set<Id> changed;
        for(const auto& [edge,uses]:adjacency)if(uses.size()==2&&!changed.count(uses[0])&&!changed.count(uses[1])) {
            auto& first=donor.triangles[uses[0]];auto& second=donor.triangles[uses[1]];
            Id a=invalidId,b=invalidId;for(Id i:first)if(i!=edge.first&&i!=edge.second)a=i;for(Id i:second)if(i!=edge.first&&i!=edge.second)b=i;
            if(a==invalidId||b==invalidId||a==b||adjacency.count(std::minmax(a,b)))continue;
            if(cross(normalized[a],normalized[b],normalized[edge.first])*cross(normalized[a],normalized[b],normalized[edge.second])>=0)continue;
            std::array<Id,3> x={a,b,edge.first},y={b,a,edge.second};
            if(quality(x)<0)std::swap(x[1],x[2]);if(quality(y)<0)std::swap(y[1],y[2]);
            const double sx=macroQuality(x),sy=macroQuality(y),sf=macroQuality(first),ss=macroQuality(second);
            const unsigned before=(sf<=0)+(ss<=0),after=(sx<=0)+(sy<=0);
            if(after>before||(after==before&&std::min(sx,sy)<=std::min(sf,ss)+1.e-12))continue;
            first=x;second=y;changed.insert(uses[0]);changed.insert(uses[1]);
        }
        if(changed.empty())break;
    }
    };
    optimize();
    for(unsigned pass=0;pass<refinement;++pass) {
        if(donor.triangles.size()>25000)return fail("Macro refinement budget exceeded");
        std::map<MeshEdge,Id> splits;
        for(const auto& t:donor.triangles)for(Id k=0;k<3;++k) {
            const auto key=std::minmax(t[k],t[(k+1)%3]);
            if(boundaryMidpoints.count(key)||splits.count(key))continue;
            const auto a=donor.sourceUV[key.first],b=donor.sourceUV[key.second];
            splits[key]=donor.sourceUV.size();donor.sourceUV.emplace_back((a.X()+b.X())*.5,(a.Y()+b.Y())*.5);donor.sourceNodes.push_back(invalidId);
        }
        std::vector<std::array<Id,3>> refined;
        for(const auto& t:donor.triangles) {
            std::array<Id,3> m={invalidId,invalidId,invalidId};unsigned count=0;
            for(Id k=0;k<3;++k){auto it=splits.find(std::minmax(t[k],t[(k+1)%3]));if(it!=splits.end()){m[k]=it->second;++count;}}
            if(count==0){refined.push_back(t);continue;}
            if(count==3) {
                for(Id k=0;k<3;++k)refined.push_back({t[k],m[k],m[(k+2)%3]});
                refined.push_back({m[0],m[1],m[2]});
            }else if(count==1) {
                Id k=0;while(m[k]==invalidId)++k;
                refined.push_back({t[k],m[k],t[(k+2)%3]});refined.push_back({m[k],t[(k+1)%3],t[(k+2)%3]});
            }else {
                Id k=0;while(m[k]!=invalidId)++k;
                const Id a=t[k],b=t[(k+1)%3],c=t[(k+2)%3],bc=m[(k+1)%3],ca=m[(k+2)%3];
                refined.push_back({c,ca,bc});refined.push_back({a,b,bc});refined.push_back({a,bc,ca});
            }
        }
        donor.triangles=std::move(refined);
        optimize();
    }
    r.sourceUV=donor.sourceUV;r.sourceNodes=donor.sourceNodes;r.triangles=donor.triangles;r.donorReady=true;
    r.uv=donor.sourceUV;r.masterNodes=donor.sourceNodes;
    std::vector<std::vector<gp_Pnt2d>> centerKernels;
    std::vector<Id> centerBases;
    std::map<MeshEdge,Id> midpoints;
    const auto add=[&](const gp_Pnt2d& uv,Id node){const Id id=r.uv.size();r.uv.push_back(uv);r.masterNodes.push_back(node);return id;};
    for(const auto& t:donor.triangles) {
        std::array<Id,3> mids;
        for(Id k=0;k<3;++k) {
            const auto key=std::minmax(t[k],t[(k+1)%3]);
            auto it=midpoints.find(key);
            if(it==midpoints.end()) {
                const auto boundary=boundaryMidpoints.find(key);
                const auto& a=r.uv[t[k]];const auto& b=r.uv[t[(k+1)%3]];
                const Id id=boundary==boundaryMidpoints.end()?add(gp_Pnt2d((a.X()+b.X())*.5,(a.Y()+b.Y())*.5),invalidId):add(boundary->second.uv,boundary->second.node);
                it=midpoints.emplace(key,id).first;
            }
            mids[k]=it->second;
        }
        // The true six-sided macro boundary may bow outside its three-corner
        // donor triangle. Clip its kernel, not the obsolete straight triangle.
        auto kernel=macroKernel({r.uv[t[0]],r.uv[t[1]],r.uv[t[2]]},{r.uv[mids[0]],r.uv[mids[1]],r.uv[mids[2]]});
        if(kernel.empty()) {
            // A non-star-shaped hexagonal macro patch can still admit two
            // convex quads separated by a diagonal between opposite vertices.
            const std::array<Id,6> ring={t[0],mids[0],t[1],mids[1],t[2],mids[2]};
            bool divided=false;
            for(Id start=0;start<3&&!divided;++start) {
                const std::array<std::array<Id,4>,2> cells={{{ring[start],ring[(start+1)%6],ring[(start+2)%6],ring[(start+3)%6]},
                    {ring[(start+3)%6],ring[(start+4)%6],ring[(start+5)%6],ring[start]}}};
                bool valid=true;
                for(const auto& q:cells)for(Id k=0;k<4;++k)valid=valid&&cross(r.uv[q[k]],r.uv[q[(k+1)%4]],r.uv[q[(k+2)%4]])>0;
                if(valid){r.quads.insert(r.quads.end(),cells.begin(),cells.end());divided=true;}
            }
            if(!divided) {
                FaceBoundaryInput local;local.owner=chart.owner;local.face=chart.face;local.ready=true;
                ChartLoop loop;loop.outer=true;loop.wire=invalidId;
                std::map<std::pair<double,double>,Id> anchors;
                for(Id i:ring){loop.vertices.push_back({r.masterNodes[i],invalidId,0,r.uv[i]});anchors[{r.uv[i].X(),r.uv[i].Y()}]=i;}
                for(Id k=0;k<6;++k)loop.signedArea+=cross(r.uv[ring[0]],r.uv[ring[k]],r.uv[ring[(k+1)%6]])*.5;
                local.loops.push_back(std::move(loop));
                for(const auto mode:{std::pair<bool,bool>{true,false},{true,true},{false,false},{false,true}}) {
                    const auto filled=FillCadChartCandidate(local,mode.first,mode.second);
                    if(!filled.ready())continue;
                    std::vector<Id> remap;
                    for(Id i=0;i<filled.uv.size();++i){const auto& p=filled.uv[i];auto it=anchors.find({p.X(),p.Y()});remap.push_back(it==anchors.end()?add(p,invalidId):it->second);}
                    for(const auto& q:filled.quads)r.quads.push_back({remap[q[0]],remap[q[1]],remap[q[2]],remap[q[3]]});
                    divided=true;break;
                }
            }
            if(!divided)return fail("Macro patch has no convex center, diagonal or local fill");
            continue;
        }
        double centerU=0,centerV=0;for(const auto& p:kernel){centerU+=p.X();centerV+=p.Y();}
        const Id center=add(gp_Pnt2d(centerU/kernel.size(),centerV/kernel.size()),invalidId);
        centerKernels.push_back(std::move(kernel));
        centerBases.push_back(r.quads.size());
        for(Id k=0;k<3;++k)r.quads.push_back({t[k],mids[k],center,mids[(k+2)%3]});
    }
    r.patchCount=donor.triangles.size();
    try {
        for(Id i=0;i<r.uv.size();++i)r.xyz.push_back(r.masterNodes[i]==invalidId?chart.owner->evaluateSurface(chart.face,r.uv[i]):chart.owner->nodes()[r.masterNodes[i]].xyz);
    }catch(const Standard_Failure&){return fail("Macro surface evaluation failed");}
    for(const auto& p:r.xyz)if(!std::isfinite(p.X())||!std::isfinite(p.Y())||!std::isfinite(p.Z()))return fail("Non-finite macro surface point");
    for(Id patch=0;patch<centerKernels.size();++patch) {
        const Id base=centerBases[patch],center=r.quads[base][2];
        const auto admissible=[&]() {
            for(Id j=0;j<3;++j) {
                const auto& q=r.quads[base+j];double u=0,v=0;
                for(Id k=0;k<4;++k){if(!(cross(r.uv[q[k]],r.uv[q[(k+1)%4]],r.uv[q[(k+2)%4]])>0))return false;u+=r.uv[q[k]].X();v+=r.uv[q[k]].Y();}
                if(!ContainsUV(chart,gp_Pnt2d(u/4,v/4)))return false;
                bool valid=false;
                for(Id start=0;start<2;++start){const auto& p=r.xyz[q[start]];const gp_Vec a(p,r.xyz[q[(start+1)%4]]),b(p,r.xyz[q[(start+2)%4]]),c(p,r.xyz[q[(start+3)%4]]);valid=valid||a.Crossed(b).Dot(b.Crossed(c))>0;}
                if(!valid)return false;
            }
            return true;
        };
        if(admissible())continue;
        const auto original=r.uv[center];bool found=false;
        for(double weight:{.25,.5,.75,.9,.99}) {
            for(const auto& p:centerKernels[patch]) {
                r.uv[center]=gp_Pnt2d(original.X()+(p.X()-original.X())*weight,original.Y()+(p.Y()-original.Y())*weight);
                try{r.xyz[center]=chart.owner->evaluateSurface(chart.face,r.uv[center]);}catch(const Standard_Failure&){continue;}
                if(admissible()){found=true;break;}
            }
            if(found)break;
        }
        if(!found){r.uv[center]=original;r.xyz[center]=chart.owner->evaluateSurface(chart.face,original);}
    }
    double area=0,expected=0;for(const auto& l:chart.loops)expected+=l.signedArea;
    for(auto& q:r.quads) {
        double x=0,y=0;
        for(Id k=0;k<4;++k) {
            if(cross(r.uv[q[k]],r.uv[q[(k+1)%4]],r.uv[q[(k+2)%4]])<=0)return fail("Macro UV quad folded");
            x+=r.uv[q[k]].X();y+=r.uv[q[k]].Y();
        }
        if(!ContainsUV(chart,gp_Pnt2d(x/4,y/4)))return fail("Macro quad leaves chart");
        area+=(cross(r.uv[q[0]],r.uv[q[1]],r.uv[q[2]])+cross(r.uv[q[0]],r.uv[q[2]],r.uv[q[3]]))*.5;
        const auto validDiagonal=[&](){const gp_Vec a(r.xyz[q[0]],r.xyz[q[1]]),b(r.xyz[q[0]],r.xyz[q[2]]),c(r.xyz[q[0]],r.xyz[q[3]]);return a.Crossed(b).Dot(b.Crossed(c))>0;};
        if(!validDiagonal())std::rotate(q.begin(),q.begin()+1,q.end());
        if(!validDiagonal()) {
            std::ostringstream detail;detail<<"Macro spatial quad folded";
            for(Id id:q){const auto& uv=r.uv[id];const auto& p=r.xyz[id];detail<<" ["<<uv.X()<<","<<uv.Y()<<"; "<<p.X()<<","<<p.Y()<<","<<p.Z()<<"; master="<<r.masterNodes[id]<<"; residual="<<p.Distance(chart.owner->evaluateSurface(chart.face,uv))<<"]";}
            r.error=detail.str();return r;
        }
        // Keep the admitted diagonal while reversing the CAD face orientation.
        if(chart.reversed)std::swap(q[1],q[3]);
    }
    if(std::abs(area-expected)>expected*1.e-7)return fail("Macro area mismatch");
    std::map<std::pair<double,double>,Id> instances;
    std::map<MeshEdge,unsigned> expectedEdges,actualEdges;
    std::vector<Id> expectedNodes;
    for(const auto& loop:chart.loops) {
        const Id start=expectedNodes.size();
        for(const auto& v:loop.vertices){if(!instances.emplace(std::make_pair(v.uv.X(),v.uv.Y()),expectedNodes.size()).second)return fail("Ambiguous macro boundary instance");expectedNodes.push_back(v.node);}
        for(Id i=0;i<loop.vertices.size();++i){Id a=start+i,b=start+(i+1)%loop.vertices.size();if(chart.reversed)std::swap(a,b);++expectedEdges[{a,b}];}
    }
    std::map<MeshEdge,std::vector<MeshEdge>> incidence;
    for(const auto& q:r.quads)for(Id k=0;k<4;++k)incidence[std::minmax(q[k],q[(k+1)%4])].push_back({q[k],q[(k+1)%4]});
    for(const auto& [edge,uses]:incidence) {
        if(uses.size()==2&&uses[0].first==uses[1].second&&uses[0].second==uses[1].first)continue;
        if(uses.size()!=1)return fail("Non-manifold macro output");
        Id ids[2];unsigned k=0;
        for(Id i:{uses[0].first,uses[0].second}) {
            auto it=instances.find({r.uv[i].X(),r.uv[i].Y()});
            if(it==instances.end()||r.masterNodes[i]!=expectedNodes[it->second])return fail("Macro output changed boundary identity");
            ids[k++]=it->second;
        }
        ++actualEdges[{ids[0],ids[1]}];
    }
    if(expectedEdges!=actualEdges)return fail("Macro output changed oriented boundary segments");
    r.stage="ready";return r;
}

ChartFillResult FillCadChart(const FaceBoundaryInput& chart,const std::vector<Id>& cornerOccurrences) {
    std::string structuredFailure;
    if(chart.ready&&chart.owner&&chart.face<chart.owner->faces().size()&&chart.loops.size()==1&&chart.loops.front().wire<chart.owner->wires().size()) {
        const auto& wire=chart.owner->wires()[chart.loops.front().wire];
        if(wire.occurrences.size()==4) {
            const auto patch=BuildStructuredPatch(chart,wire.occurrences,false);
            structuredFailure=patch.error;
            if(patch.error.empty()) {
                ChartFillResult result;result.face=chart.face;result.stage="ready";result.strategy="cad-coons";
                result.donorReady=true;result.patchCount=1;result.uv=patch.uv;result.xyz=patch.xyz;
                result.masterNodes=patch.masterNodes;result.quads=patch.quads;result.sourceUV=patch.uv;result.sourceNodes=patch.masterNodes;
                for(const auto& q:patch.quads) {
                    std::array<Id,3> a={q[0],q[1],q[2]},b={q[0],q[2],q[3]};
                    if(chart.reversed){std::swap(a[1],a[2]);std::swap(b[1],b[2]);}
                    result.triangles.push_back(a);result.triangles.push_back(b);
                }
                result.attempts.push_back("cad-coons: ready");return result;
            }
        }
    }
    auto logicalPlan=PlanLogicalCadSides(chart);
    if(cornerOccurrences.size()==4&&chart.loops.size()==1) {
        std::vector<Id> indices;
        const auto& ring=chart.loops[0].vertices;
        for(Id i=0;i<ring.size();++i)if(ring[i].sample==0&&std::find(cornerOccurrences.begin(),cornerOccurrences.end(),ring[i].occurrence)!=cornerOccurrences.end())indices.push_back(i);
        if(indices.size()==4){logicalPlan.ready=true;std::copy(indices.begin(),indices.end(),logicalPlan.corners.begin());}
    }
    std::string logicalFailure;
    if(logicalPlan.ready)for(unsigned mode=0;mode<3;++mode) {
        auto patch=mode?BuildRuledPatch(chart,logicalPlan.corners):BuildStructuredPatchAtCorners(chart,logicalPlan.corners);
        if(mode==2&&!patch.uv.empty())patch=ValidateStructuredPatch(chart,std::move(patch),true);
        logicalFailure=std::string(mode==2?"tolerance-ruled: ":mode?"ruled-strip: ":"planned-coons: ")+patch.error;
        if(patch.error.empty()) {
            ChartFillResult result;result.face=chart.face;result.stage="ready";result.strategy=mode==2?"tolerance-ruled":mode?"ruled-strip":"planned-coons";
            result.maximumSurfaceDeviation=patch.maximumSurfaceDeviation;
            result.donorReady=true;result.patchCount=1;result.uv=patch.uv;result.xyz=patch.xyz;result.masterNodes=patch.masterNodes;result.quads=patch.quads;
            result.sourceUV=patch.uv;result.sourceNodes=patch.masterNodes;
            for(const auto& q:patch.quads){std::array<Id,3> a={q[0],q[1],q[2]},b={q[0],q[2],q[3]};if(chart.reversed){std::swap(a[1],a[2]);std::swap(b[1],b[2]);}result.triangles.push_back(a);result.triangles.push_back(b);}
            result.attempts.push_back(result.strategy+": ready");return result;
        }
    }
    auto best=FillCadChartCandidate(chart,true,false);
    if(!logicalFailure.empty())best.attempts.push_back(logicalFailure);
    if(!structuredFailure.empty())best.attempts.push_back("cad-coons: rejected: "+structuredFailure);
    best.attempts.push_back(best.strategy+": "+best.stage+": "+best.error);
    if(best.ready()||!best.donorReady)return best;
    for(const auto strategy:{std::pair<bool,bool>{true,true},{false,false},{false,true}}) {
        // A cut chart already tries both modes for each simple child patch.
        if(chart.loops.size()>1&&strategy.second)continue;
        auto trial=FillCadChartCandidate(chart,strategy.first,strategy.second);
        best.attempts.push_back(trial.strategy+": "+trial.stage+": "+trial.error);
        if(trial.ready()){trial.attempts=std::move(best.attempts);return trial;}
    }
    // A logical patch side may span several CAD occurrences. Choose corners
    // only from existing chart instances; opposite sides must have equal counts.
    // This is a bounded fallback, after the existing successful paths.
    if(chart.loops.size()==1&&chart.loops.front().outer) {
        const auto& ring=chart.loops.front().vertices;const Id n=ring.size(),half=n/2;
        if(n>=8&&n<=1024&&n%2==0) {
            std::vector<double> turn(n);
            for(Id i=0;i<n;++i) {
                const auto& a=ring[(i+n-1)%n].uv;const auto& b=ring[i].uv;const auto& c=ring[(i+1)%n].uv;
                turn[i]=std::atan2(cross(a,b,c),(b.X()-a.X())*(c.X()-b.X())+(b.Y()-a.Y())*(c.Y()-b.Y()));
            }
            struct Plan {double score;std::array<Id,4> corners;};
            std::vector<Plan> plans;
            for(Id a=0;a<half;++a)for(Id b=a+2;b<half&&b-a<=half-2;++b) {
                const std::array<Id,4> corners={a,b,a+half,b+half};
                double score=0;for(Id i:corners)score+=turn[i];
                plans.push_back({score,corners});
            }
            std::sort(plans.begin(),plans.end(),[](const Plan& a,const Plan& b){return a.score!=b.score?a.score>b.score:a.corners<b.corners;});
            const Id budget=std::min<Id>(256,plans.size());
            std::map<std::string,Id> rejections;
            for(Id attempt=0;attempt<budget;++attempt) {
                const auto patch=BuildStructuredPatchAtCorners(chart,plans[attempt].corners);
                if(!patch.error.empty()){++rejections[patch.error];continue;}
                ChartFillResult result;result.face=chart.face;result.stage="ready";result.strategy="logical-coons";
                result.donorReady=true;result.patchCount=1;result.uv=patch.uv;result.xyz=patch.xyz;
                result.masterNodes=patch.masterNodes;result.quads=patch.quads;
                result.sourceUV=best.sourceUV;result.sourceNodes=best.sourceNodes;result.triangles=best.triangles;
                result.attempts=std::move(best.attempts);
                result.logicalCorners.assign(plans[attempt].corners.begin(),plans[attempt].corners.end());
                result.attempts.push_back("logical-coons: ready at candidate "+std::to_string(attempt));
                return result;
            }
            best.attempts.push_back("logical-coons: rejected all "+std::to_string(budget)+" candidates");
            for(const auto& [reason,count]:rejections)best.attempts.push_back("logical-coons: "+std::to_string(count)+": "+reason);
        }
    }
    for(bool boundaryAware:{false,true})for(unsigned phase=0;phase<2;++phase)for(unsigned refinement=0;refinement<=3;++refinement) {
        auto macro=FillTriangularPatches(chart,refinement,phase,boundaryAware);
        best.attempts.push_back(macro.strategy+(boundaryAware?" boundary-aware":" angle-quality")+" phase "+std::to_string(phase)+" refinement "+std::to_string(refinement)+": "+macro.error);
        if(macro.ready()){macro.attempts=std::move(best.attempts);return macro;}
    }
    if(logicalPlan.ready)for(unsigned mode=1;mode<4;++mode) {
        auto patch=mode>=2?BuildInverseHarmonicPatch(chart,logicalPlan.corners):BuildStructuredPatchAtCorners(chart,logicalPlan.corners,true);
        if(mode==3)patch=UntangleStructuredPatch(chart,std::move(patch));
        const std::string strategy=mode==3?"untangled-structured":mode==2?"inverse-harmonic":"planned-harmonic";
        best.attempts.push_back(strategy+": "+patch.error);
        if(!patch.error.empty())continue;
        ChartFillResult result;result.face=chart.face;result.stage="ready";result.strategy=strategy;
        result.donorReady=true;result.patchCount=1;result.uv=patch.uv;result.xyz=patch.xyz;result.masterNodes=patch.masterNodes;result.quads=patch.quads;
        result.sourceUV=patch.uv;result.sourceNodes=patch.masterNodes;
        for(const auto& q:patch.quads){std::array<Id,3> a={q[0],q[1],q[2]},b={q[0],q[2],q[3]};if(chart.reversed){std::swap(a[1],a[2]);std::swap(b[1],b[2]);}result.triangles.push_back(a);result.triangles.push_back(b);}
        result.attempts=std::move(best.attempts);return result;
    }
    return best;
}
}
