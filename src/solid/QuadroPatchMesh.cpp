#include "QuadroPatchMesh.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <Standard_Failure.hxx>

namespace quadro {
StructuredBodyPlan PlanStructuredBody(const BoundarySnapshot& s,double density) {
    StructuredBodyPlan p;
    if(!s.topologyValid()||s.faces().empty()||!std::isfinite(density)||density<=0) return p;
    bool hasEight=false;
    for(const auto& e:s.edges()) if(e.degenerate||e.occurrences.size()!=2||
        s.occurrences()[e.occurrences[0]].face==s.occurrences()[e.occurrences[1]].face) return p;
    for(const auto& f:s.faces()) {
        if(f.wires.size()!=1) return p;
        const auto& ids=s.wires()[f.wires.front()].occurrences;
        if(ids.size()!=4&&ids.size()!=8) return p;
        for(Id id:ids) if(s.occurrences()[id].seam) return p;
        std::vector<Id> corners;
        if(ids.size()==4) {
            corners=ids;
            for(Id i=0;i<2;++i)p.sampling.equalSegmentGroups.push_back({s.occurrences()[ids[i]].edge,s.occurrences()[ids[i+2]].edge});
        } else {
            double sums[2]={0,0};
            for(Id i=0;i<8;++i) {
                const auto& e=s.edges()[s.occurrences()[ids[i]].edge];
                sums[i%2]+=s.vertices()[e.firstVertex].xyz.Distance(s.vertices()[e.lastVertex].xyz);
            }
            const Id shortParity=sums[0]<sums[1]?0:1, longParity=1-shortParity;
            // Supported family: four long sides separated by four rounded
            // corner edges. Generic octagons need a separate patch planner.
            if(sums[longParity]<1.5*sums[shortParity]) return p;
            std::vector<Id> shortEdges;
            for(Id i=shortParity;i<8;i+=2){corners.push_back(ids[i]);shortEdges.push_back(s.occurrences()[ids[i]].edge);}
            p.sampling.equalSegmentGroups.push_back(shortEdges);
            for(Id i=0;i<2;++i)p.sampling.equalSegmentGroups.push_back({
                s.occurrences()[ids[longParity+2*i]].edge,s.occurrences()[ids[longParity+2*i+4]].edge});
            hasEight=true;
        }
        p.cornerOccurrences.push_back(std::move(corners));p.splitCornerEdges.push_back(ids.size()==8);
    }
    // Roll out the new consumer to closed rounded patch bodies first. Existing
    // simple solids, holes, seams and general quilts keep their current path.
    if(!hasEight) return p;
    gp_Pnt lo=s.vertices().front().xyz,hi=lo;
    for(const auto& v:s.vertices())for(int k=1;k<=3;++k){lo.SetCoord(k,std::min(lo.Coord(k),v.xyz.Coord(k)));hi.SetCoord(k,std::max(hi.Coord(k),v.xyz.Coord(k)));}
    const double diagonal=lo.Distance(hi);
    if(!(diagonal>0)) return p;
    density=std::clamp(density,0.01,1.0);
    p.sampling.minimumSegments=4;p.sampling.maximumSegments=256;
    p.sampling.maxSegmentLength=diagonal/(12+24*density);
    p.sampling.chordTolerance=diagonal*0.002/(0.25+density);
    p.eligible=true;return p;
}

namespace {
double cross(const gp_Pnt2d& a,const gp_Pnt2d& b,const gp_Pnt2d& c) {
    return (b.X()-a.X())*(c.Y()-a.Y())-(b.Y()-a.Y())*(c.X()-a.X());
}
}

StructuredPatch BuildStructuredPatch(const FaceBoundaryInput& input,
    const std::vector<Id>& cornerOccurrences,bool split) {
    StructuredPatch p;p.face=input.face;
    const auto fail=[&](const char* message){p.error=message;return p;};
    if(!input.ready||input.loops.size()!=1||!input.loops.front().outer) return fail("A validated single outer chart is required");
    const auto& s=*input.owner;const auto& ring=input.loops.front().vertices;
    std::vector<Id> cuts;
    for(Id i=0;i<ring.size();++i) if(std::find(cornerOccurrences.begin(),cornerOccurrences.end(),ring[i].occurrence)!=cornerOccurrences.end()) {
        const auto count=s.views()[ring[i].occurrence].samples.size()-1;
        if(split && count%2) return fail("Rounded corner edge requires an even segment count");
        if(ring[i].sample==(split?count/2:0))cuts.push_back(i);
    }
    if(cuts.size()!=4) return fail("Four logical corners were not found in the CAD boundary");
    return BuildStructuredPatchAtCorners(input,{cuts[0],cuts[1],cuts[2],cuts[3]});
}

StructuredPatch BuildStructuredPatchAtCorners(const FaceBoundaryInput& input,
    const std::array<Id,4>& cuts,bool harmonic) {
    StructuredPatch p;p.face=input.face;
    const auto fail=[&](const char* message){p.error=message;return p;};
    if(!input.ready||!input.owner||input.loops.size()!=1||!input.loops.front().outer)
        return fail("A validated single outer chart is required");
    const auto& ring=input.loops.front().vertices;
    if(cuts[3]>=ring.size()||!(cuts[0]<cuts[1]&&cuts[1]<cuts[2]&&cuts[2]<cuts[3]))
        return fail("Logical corners must be distinct and ordered in the source loop");
    std::array<std::vector<ChartVertex>,4> side;
    for(Id k=0;k<4;++k) {
        Id i=cuts[k];side[k].push_back(ring[i]);
        do {i=(i+1)%ring.size();side[k].push_back(ring[i]);}while(i!=cuts[(k+1)%4]);
    }
    if(side[0].size()!=side[2].size()||side[1].size()!=side[3].size())return fail("Opposite CAD sides have incompatible segment counts");
    const Id nu=side[0].size()-1,nv=side[1].size()-1;
    if(nu<2||nv<2||nu>512||nv>512)return fail("Structured patch size outside budget");
    p.columns=nu+1;p.rows=nv+1;
    p.uv.resize((nu+1)*(nv+1));p.xyz.resize(p.uv.size());p.masterNodes.assign(p.uv.size(),invalidId);
    const auto index=[&](Id i,Id j){return j*(nu+1)+i;};
    const auto setBoundary=[&](Id i,Id j,const ChartVertex& v){p.uv[index(i,j)]=v.uv;p.masterNodes[index(i,j)]=v.node;};
    for(Id i=0;i<=nu;++i){setBoundary(i,0,side[0][i]);setBoundary(i,nv,side[2][nu-i]);}
    for(Id j=0;j<=nv;++j){setBoundary(nu,j,side[1][j]);setBoundary(0,j,side[3][nv-j]);}
    const auto c00=p.uv[index(0,0)],c10=p.uv[index(nu,0)],c11=p.uv[index(nu,nv)],c01=p.uv[index(0,nv)];
    for(Id j=1;j<nv;++j)for(Id i=1;i<nu;++i) {
        const double u=double(i)/nu,v=double(j)/nv;
        const auto b=p.uv[index(i,0)],t=p.uv[index(i,nv)],l=p.uv[index(0,j)],r=p.uv[index(nu,j)];
        const auto coons=[&](int k){return (1-v)*b.Coord(k)+v*t.Coord(k)+(1-u)*l.Coord(k)+u*r.Coord(k)-
            ((1-u)*(1-v)*c00.Coord(k)+u*(1-v)*c10.Coord(k)+u*v*c11.Coord(k)+(1-u)*v*c01.Coord(k));};
        p.uv[index(i,j)]=gp_Pnt2d(coons(1),coons(2));
    }
    if(harmonic) {
        if(p.uv.size()>16384)return fail("Harmonic patch size outside budget");
        double scale=0;for(const auto& v:p.uv)scale=std::max(scale,v.Distance(c00));
        bool converged=false;
        for(unsigned iteration=0;iteration<2000;++iteration) {
            double residual=0;
            for(Id j=1;j<nv;++j)for(Id i=1;i<nu;++i) {
                const auto old=p.uv[index(i,j)];double x=0,y=0;
                for(Id id:{index(i-1,j),index(i+1,j),index(i,j-1),index(i,j+1)}){x+=p.uv[id].X();y+=p.uv[id].Y();}
                const gp_Pnt2d next(old.X()+1.5*(x*.25-old.X()),old.Y()+1.5*(y*.25-old.Y()));
                residual=std::max(residual,old.Distance(next));p.uv[index(i,j)]=next;
            }
            if(residual<=scale*1.e-10){converged=true;break;}
        }
        if(!converged)return fail("Harmonic patch iteration budget exceeded");
    }
    return ValidateStructuredPatch(input,std::move(p));
}

StructuredPatch ValidateStructuredPatch(const FaceBoundaryInput& input,StructuredPatch p,bool extendBoundaryDisplacement) {
    const auto fail=[&](const char* message){p.error=message;return p;};
    if(!input.ready||!input.owner||input.loops.size()!=1||p.columns<2||p.rows<2||p.uv.size()!=p.columns*p.rows||p.masterNodes.size()!=p.uv.size())return fail("Invalid structured patch storage");
    const auto& s=*input.owner;const Id nu=p.columns-1,nv=p.rows-1;
    const auto index=[&](Id i,Id j){return j*(nu+1)+i;};
    p.xyz.resize(p.uv.size());p.quads.clear();p.error.clear();p.maximumSurfaceDeviation=0;
    try {
        for(Id i=0;i<p.uv.size();++i) p.xyz[i]=p.masterNodes[i]==invalidId?s.evaluateSurface(input.face,p.uv[i]):s.nodes()[p.masterNodes[i]].xyz;
    }catch(const Standard_Failure&){return fail("Surface evaluation failed");}
    if(extendBoundaryDisplacement) {
        if(!s.cadToleranceReconciliationEnabled()||!(s.meshTolerance()>0))return fail("Boundary displacement extension is not enabled");
        std::vector<gp_Vec> delta(p.uv.size());
        try {for(Id j=0;j<=nv;++j)for(Id i=0;i<=nu;++i)if(i==0||j==0||i==nu||j==nv){const Id id=index(i,j);delta[id]=gp_Vec(s.evaluateSurface(input.face,p.uv[id]),p.xyz[id]);}}
        catch(const Standard_Failure&){return fail("Boundary residual evaluation failed");}
        const auto a=delta[index(0,0)],b=delta[index(nu,0)],c=delta[index(nu,nv)],d=delta[index(0,nv)];
        for(Id j=1;j<nv;++j)for(Id i=1;i<nu;++i) {
            const double u=double(i)/nu,v=double(j)/nv;
            const gp_Vec shift=delta[index(i,0)]*(1-v)+delta[index(i,nv)]*v+delta[index(0,j)]*(1-u)+delta[index(nu,j)]*u-
                (a*((1-u)*(1-v))+b*(u*(1-v))+c*(u*v)+d*((1-u)*v));
            if(shift.Magnitude()>s.meshTolerance())return fail("Boundary displacement exceeds mesh approximation budget");
            p.xyz[index(i,j)].Translate(shift);p.maximumSurfaceDeviation=std::max(p.maximumSurfaceDeviation,shift.Magnitude());
        }
    }
    double area=0;
    const double areaBudget=std::abs(input.loops.front().signedArea)*1.e-12;
    for(Id j=0;j<nv;++j)for(Id i=0;i<nu;++i) {
        std::array<Id,4> q={index(i,j),index(i+1,j),index(i+1,j+1),index(i,j+1)};
        double x=0,y=0;
        for(Id k=0;k<4;++k) {
            if(cross(p.uv[q[k]],p.uv[q[(k+1)%4]],p.uv[q[(k+2)%4]])<=areaBudget)
                return fail("Structured UV cell is folded or degenerate");
            x+=p.uv[q[k]].X();y+=p.uv[q[k]].Y();
            if(!std::isfinite(p.xyz[q[k]].X())||!std::isfinite(p.xyz[q[k]].Y())||!std::isfinite(p.xyz[q[k]].Z()))return fail("Non-finite patch vertex");
        }
        if(!ContainsUV(input,gp_Pnt2d(x/4,y/4)))return fail("Structured cell leaves the CAD chart");
        const auto validDiagonal=[&](){const gp_Vec a(p.xyz[q[0]],p.xyz[q[1]]),b(p.xyz[q[0]],p.xyz[q[2]]),c(p.xyz[q[0]],p.xyz[q[3]]);const auto n1=a.Crossed(b),n2=b.Crossed(c);return n1.SquareMagnitude()>1.e-24&&n2.SquareMagnitude()>1.e-24&&n1.Dot(n2)>0;};
        if(!validDiagonal())std::rotate(q.begin(),q.begin()+1,q.end());
        if(!validDiagonal())
            return fail("Structured spatial cell is folded or degenerate");
        if(extendBoundaryDisplacement) {
            std::vector<std::pair<gp_Pnt,gp_Pnt2d>> probes;
            for(Id k=0;k<4;++k){const auto& a=p.xyz[q[k]];const auto& b=p.xyz[q[(k+1)%4]];const auto& u=p.uv[q[k]];const auto& v=p.uv[q[(k+1)%4]];probes.push_back({gp_Pnt((a.X()+b.X())*.5,(a.Y()+b.Y())*.5,(a.Z()+b.Z())*.5),gp_Pnt2d((u.X()+v.X())*.5,(u.Y()+v.Y())*.5)});}
            for(Id k=1;k<3;++k){const auto& a=p.xyz[q[0]];const auto& b=p.xyz[q[k]];const auto& c=p.xyz[q[k+1]];const auto& u=p.uv[q[0]];const auto& v=p.uv[q[k]];const auto& w=p.uv[q[k+1]];probes.push_back({gp_Pnt((a.X()+b.X()+c.X())/3,(a.Y()+b.Y()+c.Y())/3,(a.Z()+b.Z()+c.Z())/3),gp_Pnt2d((u.X()+v.X()+w.X())/3,(u.Y()+v.Y()+w.Y())/3)});}
            try {for(const auto& [point,uv]:probes){double deviation=point.Distance(s.evaluateSurface(input.face,uv));if(deviation>s.meshTolerance())deviation=s.projectedSurfaceDistance(input.face,point);if(deviation<0||!std::isfinite(deviation)||deviation>s.meshTolerance())return fail("Corrected patch exceeds sampled surface error budget");p.maximumSurfaceDeviation=std::max(p.maximumSurfaceDeviation,deviation);}}
            catch(const Standard_Failure&){return fail("Corrected patch surface evaluation failed");}
        }
        area+=(cross(p.uv[q[0]],p.uv[q[1]],p.uv[q[2]])+cross(p.uv[q[0]],p.uv[q[2]],p.uv[q[3]]))*.5;
        if(input.reversed)std::swap(q[1],q[3]);
        p.quads.push_back(q);
    }
    if(std::abs(area-std::abs(input.loops.front().signedArea))>std::max(area*1.e-8,1.e-12))return fail("Structured cells do not cover the complete chart");
    return p;
}

LogicalSidePlan PlanLogicalCadSides(const FaceBoundaryInput& input) {
    LogicalSidePlan plan;
    if(!input.ready||!input.owner||input.loops.size()!=1||!input.loops[0].outer)return plan;
    const auto& ring=input.loops[0].vertices;
    std::vector<std::pair<double,Id>> turns;
    for(Id i=0;i<ring.size();++i)if(ring[i].sample==0&&ring[i].occurrence!=invalidId) {
        const auto& a=ring[(i+ring.size()-1)%ring.size()].uv;const auto& b=ring[i].uv;const auto& c=ring[(i+1)%ring.size()].uv;
        const double angle=std::atan2(cross(a,b,c),(b.X()-a.X())*(c.X()-b.X())+(b.Y()-a.Y())*(c.Y()-b.Y()));
        if(angle>0)turns.push_back({angle,i});
    }
    if(turns.size()<4)return plan;
    std::sort(turns.begin(),turns.end(),[](const auto& a,const auto& b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
    for(Id k=0;k<4;++k)plan.corners[k]=turns[k].second;
    std::sort(plan.corners.begin(),plan.corners.end());
    for(Id k=0;k<4;++k) {
        std::set<Id> occurrences;
        for(Id i=(plan.corners[k]+1)%ring.size();i!=plan.corners[(k+1)%4];i=(i+1)%ring.size())
            if(ring[i].sample>0&&ring[i].occurrence!=invalidId)occurrences.insert(ring[i].occurrence);
        for(Id oi:occurrences)plan.edges[k].push_back(input.owner->occurrences()[oi].edge);
        if(plan.edges[k].empty())return LogicalSidePlan{};
    }
    plan.ready=true;return plan;
}

LogicalBoundaryPlan PlanLogicalBoundary(const std::shared_ptr<const BoundarySnapshot>& boundary,const SamplingOptions& options) {
    LogicalBoundaryPlan result;result.sampling=options;
    if(!boundary||!boundary->discretizationReady())return result;
    result.cornerOccurrences.resize(boundary->faces().size());
    result.sampling.minimumSegmentsByEdge.resize(boundary->edges().size());
    for(const auto& m:boundary->masters())result.sampling.minimumSegmentsByEdge[m.edge]=m.nodes.size()-1;
    for(const auto& face:boundary->faces()) {
        const auto chart=BuildFaceBoundaryInput(boundary,face.id);const auto plan=PlanLogicalCadSides(chart);
        if(!plan.ready)continue;
        for(Id i:plan.corners)result.cornerOccurrences[face.id].push_back(chart.loops[0].vertices[i].occurrence);
        const Id sumIndex=result.sampling.equalSegmentSums.size();std::array<bool,2> partitioned={false,false};
        for(Id k=0;k<2;++k)result.sampling.equalSegmentSums.push_back({plan.edges[k],plan.edges[k+2]});
        const auto& ring=chart.loops[0].vertices;
        const auto stations=[&](Id start,Id finish,int direction) {
            std::vector<Id> indices{start};
            for(Id i=direction>0?(start+1)%ring.size():(start+ring.size()-1)%ring.size();i!=finish;i=direction>0?(i+1)%ring.size():(i+ring.size()-1)%ring.size())if(ring[i].sample==0)indices.push_back(i);
            indices.push_back(finish);return indices;
        };
        const auto intervalEdges=[&](Id start,Id finish,int direction) {
            std::set<Id> occurrences;
            for(Id i=direction>0?(start+1)%ring.size():(start+ring.size()-1)%ring.size();i!=finish;i=direction>0?(i+1)%ring.size():(i+ring.size()-1)%ring.size())if(ring[i].sample>0)occurrences.insert(ring[i].occurrence);
            std::vector<Id> edges;for(Id oi:occurrences)edges.push_back(boundary->occurrences()[oi].edge);return edges;
        };
        for(Id k=0;k<2;++k) {
            if(plan.edges[k].size()==plan.edges[k+2].size()||std::min(plan.edges[k].size(),plan.edges[k+2].size())<2)continue;
            const auto a=stations(plan.corners[k],plan.corners[k+1],1),b=stations(plan.corners[(k+3)%4],plan.corners[k+2],-1);
            const auto position=[&](Id i)->const gp_Pnt& {return boundary->nodes()[ring[i].node].xyz;};
            const double width=std::max(position(a.front()).Distance(position(b.front())),position(a.back()).Distance(position(b.back())));
            double length=0;for(Id i=1;i<a.size();++i)length+=position(a[i-1]).Distance(position(a[i]));
            if(!(width>0)||length<20*width)continue;
            for(const auto& side:plan.edges)for(Id e:side)result.sampling.arcLengthEdges.push_back(e);
            std::vector<std::pair<Id,Id>> matches{{0,0}};
            for(Id i=1;i+1<a.size();++i) {
                Id nearest=invalidId;double distance=2*width;
                for(Id j=1;j+1<b.size();++j){const double d=position(a[i]).Distance(position(b[j]));if(d<distance){distance=d;nearest=j;}}
                if(nearest==invalidId||nearest<=matches.back().second)continue;
                Id reverse=i;double closest=distance;
                for(Id j=1;j+1<a.size();++j){const double d=position(a[j]).Distance(position(b[nearest]));if(d<closest){closest=d;reverse=j;}}
                if(reverse==i)matches.push_back({i,nearest});
            }
            matches.push_back({a.size()-1,b.size()-1});
            if(matches.size()==2)continue;
            partitioned[k]=true;
            for(Id i=1;i<matches.size();++i) {
                auto left=intervalEdges(a[matches[i-1].first],a[matches[i].first],1),right=intervalEdges(b[matches[i-1].second],b[matches[i].second],-1);
                const auto anchor=[&](const std::vector<Id>& target,Id start,const std::vector<Id>& source,Id begin,Id end,int direction) {
                    if(target.size()!=1||end<=begin+1)return;
                    const Id edge=target[0];const auto& cad=boundary->edges()[edge];
                    if(cad.closed||cad.degenerate)return;
                    const bool reversed=ring[start].node==cad.lastVertex;
                    if(!reversed&&ring[start].node!=cad.firstVertex)return;
                    for(Id j=begin+1;j<end;++j) {
                        double distance;const double parameter=boundary->projectedEdgeParameter(edge,position(source[j]),&distance);
                        if(!std::isfinite(parameter)||distance>2*width||parameter<=cad.firstParameter||parameter>=cad.lastParameter)continue;
                        auto prefix=intervalEdges(source[begin],source[j],direction);
                        if(!prefix.empty())result.sampling.stations.push_back({edge,parameter,std::move(prefix),reversed});
                    }
                };
                anchor(left,a[matches[i-1].first],b,matches[i-1].second,matches[i].second,-1);
                anchor(right,b[matches[i-1].second],a,matches[i-1].first,matches[i].first,1);
                if(!left.empty()&&!right.empty())result.sampling.equalSegmentSums.push_back({std::move(left),std::move(right)});
            }
        }
        for(int k=1;k>=0;--k)if(partitioned[k])result.sampling.equalSegmentSums.erase(result.sampling.equalSegmentSums.begin()+sumIndex+k);
    }
    std::sort(result.sampling.arcLengthEdges.begin(),result.sampling.arcLengthEdges.end());
    result.sampling.arcLengthEdges.erase(std::unique(result.sampling.arcLengthEdges.begin(),result.sampling.arcLengthEdges.end()),result.sampling.arcLengthEdges.end());
    return result;
}

bool ValidateClosedPatchBody(const BoundarySnapshot& boundary,const std::vector<StructuredPatch>& patches,std::string& error) {
    std::map<std::pair<Id,Id>,std::pair<int,int>> edges;
    Id next=boundary.nodes().size();std::size_t cells=0;
    for(const auto& patch:patches) {
        if(!patch.error.empty()||patch.quads.empty()){error="Incomplete patch body";return false;}
        std::vector<Id> ids=patch.masterNodes;
        for(auto& id:ids)if(id==invalidId)id=next++;
        cells+=patch.quads.size();if(cells>1000000){error="Body cell budget exceeded";return false;}
        for(const auto& q:patch.quads)for(Id k=0;k<4;++k) {
            const Id a=ids[q[k]],b=ids[q[(k+1)%4]];
            if(a==b){error="Collapsed body edge";return false;}
            auto& e=edges[std::minmax(a,b)];++e.first;e.second+=(a<b?1:-1);
        }
    }
    for(const auto& e:edges)if(e.second.first!=2||e.second.second!=0){error="Body has an open, non-manifold or inconsistently oriented edge";return false;}
    return true;
}
}
