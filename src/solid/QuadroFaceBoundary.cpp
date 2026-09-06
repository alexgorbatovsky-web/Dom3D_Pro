#include "QuadroFaceBoundary.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <Standard_Failure.hxx>

namespace quadro {
namespace {
double cross(const gp_Pnt2d& a,const gp_Pnt2d& b,const gp_Pnt2d& c) {
    return (b.X()-a.X())*(c.Y()-a.Y())-(b.Y()-a.Y())*(c.X()-a.X());
}
bool onSegment(const gp_Pnt2d& p,const gp_Pnt2d& a,const gp_Pnt2d& b,double eps) {
    return std::abs(cross(a,b,p))<=eps*std::max(a.Distance(b),eps) &&
        p.X()>=std::min(a.X(),b.X())-eps && p.X()<=std::max(a.X(),b.X())+eps &&
        p.Y()>=std::min(a.Y(),b.Y())-eps && p.Y()<=std::max(a.Y(),b.Y())+eps;
}
bool intersects(const gp_Pnt2d& a,const gp_Pnt2d& b,const gp_Pnt2d& c,const gp_Pnt2d& d,double eps) {
    if(std::max(a.X(),b.X())+eps<std::min(c.X(),d.X()) || std::max(c.X(),d.X())+eps<std::min(a.X(),b.X()) ||
       std::max(a.Y(),b.Y())+eps<std::min(c.Y(),d.Y()) || std::max(c.Y(),d.Y())+eps<std::min(a.Y(),b.Y())) return false;
    if (onSegment(a,c,d,eps)||onSegment(b,c,d,eps)||onSegment(c,a,b,eps)||onSegment(d,a,b,eps)) return true;
    const double abEps=eps*std::max(a.Distance(b),eps),cdEps=eps*std::max(c.Distance(d),eps);
    const double x=cross(a,b,c),y=cross(a,b,d),u=cross(c,d,a),v=cross(c,d,b);
    // Do not turn tiny roundoff on nearly collinear pcurves into a crossing.
    return ((x>abEps&&y<-abEps)||(y>abEps&&x<-abEps)) &&
        ((u>cdEps&&v<-cdEps)||(v>cdEps&&u<-cdEps));
}
// 0 outside, 1 inside, 2 on the polygon boundary.
int locate(const ChartLoop& loop,const gp_Pnt2d& p,double eps) {
    bool inside=false;
    for(Id i=0;i<loop.vertices.size();++i) {
        const auto& a=loop.vertices[i].uv;const auto& b=loop.vertices[(i+1)%loop.vertices.size()].uv;
        if(onSegment(p,a,b,eps)) return 2;
        if((a.Y()>p.Y())!=(b.Y()>p.Y()) && p.X()<(b.X()-a.X())*(p.Y()-a.Y())/(b.Y()-a.Y())+a.X()) inside=!inside;
    }
    return inside?1:0;
}
gp_Pnt2d center(const ChartLoop& loop) {
    double x=0,y=0;for(const auto& v:loop.vertices){x+=v.uv.X();y+=v.uv.Y();}
    return gp_Pnt2d(x/loop.vertices.size(),y/loop.vertices.size());
}
double scale(const ChartLoop& loop) {
    double xmin=std::numeric_limits<double>::max(),xmax=-xmin,ymin=xmin,ymax=-xmin;
    for(const auto& v:loop.vertices){xmin=std::min(xmin,v.uv.X());xmax=std::max(xmax,v.uv.X());ymin=std::min(ymin,v.uv.Y());ymax=std::max(ymax,v.uv.Y());}
    return std::max({xmax-xmin,ymax-ymin,1.e-6});
}
bool crossLoops(const ChartLoop& a,const ChartLoop& b,double eps) {
    for(Id i=0;i<a.vertices.size();++i) for(Id j=0;j<b.vertices.size();++j)
        if(intersects(a.vertices[i].uv,a.vertices[(i+1)%a.vertices.size()].uv,
            b.vertices[j].uv,b.vertices[(j+1)%b.vertices.size()].uv,eps)) return true;
    return false;
}
}

FaceBoundaryInput BuildFaceBoundaryInput(std::shared_ptr<const BoundarySnapshot> snapshot,Id faceId) {
    FaceBoundaryInput input;input.owner=std::move(snapshot);input.face=faceId;
    const auto fail=[&](const char* code,const char* message,Id wire=invalidId,Id occurrence=invalidId){
        input.issues.push_back({code,message,faceId,wire,occurrence});
    };
    if(!input.owner || !input.owner->topologyValid() || faceId>=input.owner->faces().size()) {
        fail("InvalidTopology","Cannot adapt an invalid or absent CAD face");return input;
    }
    const auto& s=*input.owner;const auto& face=s.faces()[faceId];input.reversed=face.reversed;
    if(s.views().size()!=s.occurrences().size() || s.masters().size()!=s.edges().size()) {
        fail("MissingDiscretization","Complete occurrence samples are required");return input;
    }
    // A failure on another face does not erase inspectable, valid charts here.
    for(const auto& issue:s.issues()) {
        bool relevant=issue.face==faceId;
        if(issue.face==invalidId) {
            relevant=issue.edge==invalidId;
            if(issue.edge!=invalidId) for(Id oi:s.edges()[issue.edge].occurrences)
                if(s.occurrences()[oi].face==faceId) relevant=true;
        }
        if(relevant) input.issues.push_back(issue);
    }
    if(!input.issues.empty()) return input;
    std::size_t totalSamples=0;
    for(Id wi:face.wires) for(Id oi:s.wires()[wi].occurrences) totalSamples+=s.views()[oi].samples.size();
    if(totalSamples>4000) {fail("ChartValidationBudget","Single-chart polygon validation sample budget exceeded");return input;}
    const auto cornerFits=[&](const gp_Pnt2d& a,const gp_Pnt2d& b,Id node,double budget) {
        try {
            for(double t:{0.25,0.5,0.75}) {
                const auto uv=gp_Pnt2d(a.X()+(b.X()-a.X())*t,a.Y()+(b.Y()-a.Y())*t);
                const double r=s.evaluateSurface(faceId,uv).Distance(s.nodes()[node].xyz);
                if(!std::isfinite(r)||r>budget) return false;
            }
            return true;
        } catch(const Standard_Failure&) {return false;}
    };
    const auto reconcileCorner=[&](ChartVertex& corner,const gp_Pnt2d& incoming,double budget) {
        // Symmetric representative of two pcurve endpoints for ONE CAD vertex.
        // Source occurrences stay untouched; only this chart instance changes.
        const auto midpoint=gp_Pnt2d((incoming.X()+corner.uv.X())*.5,(incoming.Y()+corner.uv.Y())*.5);
        try {
            if(s.evaluateSurface(faceId,midpoint).Distance(s.nodes()[corner.node].xyz)<=budget) {
                corner.cornerReconciled=midpoint.Distance(corner.uv)>0;corner.uv=midpoint;
            }
        }catch(const Standard_Failure&) {fail("CadCornerEvaluationFailure","Cannot evaluate reconciled UV corner",invalidId,corner.occurrence);}
    };
    for(Id wi:face.wires) {
        const auto& wire=s.wires()[wi];ChartLoop loop;loop.wire=wi;loop.outer=wire.outer;
        struct CornerCandidate {Id index;gp_Pnt2d incoming,outgoing;double budget;};
        std::vector<CornerCandidate> cornerCandidates;
        gp_Pnt2d previous;double previousBudget=0,firstBudget=0;bool first=true;
        for(Id oi:wire.occurrences) {
            const auto& occurrence=s.occurrences()[oi];const auto& samples=s.views()[oi].samples;
            if(s.edges()[occurrence.edge].degenerate) fail("PoleNeedsChartCut","Collapsed spatial edge requires a pole-specific patch",wi,oi);
            if(samples.size()<2) {fail("IncompleteOccurrence","Occurrence has no usable samples",wi,oi);continue;}
            double du=0,dv=0;
            if(!first) {
                if(face.uPeriod) du=face.uPeriod*std::round((previous.X()-samples.front().uv.X())/face.uPeriod);
                if(face.vPeriod) dv=face.vPeriod*std::round((previous.Y()-samples.front().uv.Y())/face.vPeriod);
                if(!cornerFits(previous,gp_Pnt2d(samples.front().uv.X()+du,samples.front().uv.Y()+dv),
                    samples.front().node,std::max(previousBudget,samples.front().budget)))
                    fail("IncompatibleUVCorner","UV endpoint reconciliation exceeds the local CAD vertex budget",wi,oi);
            }
            else firstBudget=samples.front().budget;
            for(Id i=0;i+1<samples.size();++i) {
                const auto& p=samples[i];
                loop.vertices.push_back({p.node,oi,i,gp_Pnt2d(p.uv.X()+du,p.uv.Y()+dv)});
                if(i==0&&!first&&s.cadToleranceReconciliationEnabled()) {
                    cornerCandidates.push_back({loop.vertices.size()-1,previous,loop.vertices.back().uv,std::min(previousBudget,p.budget)});
                    reconcileCorner(loop.vertices.back(),previous,std::min(previousBudget,p.budget));
                }
            }
            previous=gp_Pnt2d(samples.back().uv.X()+du,samples.back().uv.Y()+dv);previousBudget=samples.back().budget;first=false;
        }
        if(loop.vertices.size()<3) {fail("DegenerateUVLoop","UV loop has fewer than three instances",wi);input.loops.push_back(std::move(loop));continue;}
        const auto start=loop.vertices.front().uv;
        if((face.uPeriod && std::abs(previous.X()-start.X())>face.uPeriod*.5) ||
           (face.vPeriod && std::abs(previous.Y()-start.Y())>face.vPeriod*.5))
            fail("PeriodicWireNeedsChartCut","Non-zero winding requires explicit chart cuts",wi);
        else if(!cornerFits(previous,start,loop.vertices.front().node,std::max(previousBudget,firstBudget)))
            fail("IncompatibleUVCorner","Closing UV corner exceeds the local CAD vertex budget",wi);
        else if(s.cadToleranceReconciliationEnabled()) {
            cornerCandidates.push_back({0,previous,start,std::min(previousBudget,firstBudget)});
            reconcileCorner(loop.vertices.front(),previous,std::min(previousBudget,firstBudget));
        }
        // Each CAD corner has one chart instance (the following occurrence's
        // start). Raw pcurve endpoints remain available in the owner snapshot.
        // This is identity-based assembly, not nearest-XYZ joining.
        const double eps=scale(loop)*1.e-12;
        // A midpoint can still cut across a thin tolerance-sized wedge after
        // refinement. Try only the original incident pcurve endpoints, keeping
        // the same CAD node and all edge samples. Accept a candidate only if it
        // stays in BOTH budgets and strictly reduces polygon intersections.
        if(!cornerCandidates.empty()) {
            std::size_t pairChecks=0;bool exhausted=false;
            const auto countCrossings=[&](std::size_t limit) {
                std::size_t count=0;
                for(Id i=0;i<loop.vertices.size();++i)for(Id j=i+2;j<loop.vertices.size();++j) {
                    if(i==0&&j+1==loop.vertices.size())continue;
                    if(++pairChecks>8000000){exhausted=true;return limit;}
                    if(intersects(loop.vertices[i].uv,loop.vertices[(i+1)%loop.vertices.size()].uv,
                        loop.vertices[j].uv,loop.vertices[(j+1)%loop.vertices.size()].uv,eps)&&++count>=limit)return count;
                }
                return count;
            };
            auto crossings=countCrossings(std::numeric_limits<std::size_t>::max());
            unsigned attempts=0;bool improved=true;
            while(crossings&&!exhausted&&improved&&attempts<128) {
                improved=false;
                for(const auto& candidate:cornerCandidates) {
                    auto& vertex=loop.vertices[candidate.index];
                    for(const auto& uv:{candidate.incoming,candidate.outgoing}) {
                        if(++attempts>128||exhausted)break;
                        try {if(s.evaluateSurface(faceId,uv).Distance(s.nodes()[vertex.node].xyz)>candidate.budget)continue;}
                        catch(const Standard_Failure&){continue;}
                        const auto saved=vertex.uv;vertex.uv=uv;
                        const auto count=countCrossings(crossings);
                        if(!exhausted&&count<crossings){crossings=count;vertex.cornerReconciled=uv.Distance(candidate.outgoing)>0;improved=true;break;}
                        vertex.uv=saved;
                    }
                    if(improved||exhausted||attempts>=128)break;
                }
            }
            if(exhausted)fail("ChartValidationBudget","Corner reconciliation validation budget exceeded",wi);
        }
        const auto origin=loop.vertices.front().uv;
        for(Id i=0;i<loop.vertices.size();++i) {
            const auto& a=loop.vertices[i].uv;const auto& b=loop.vertices[(i+1)%loop.vertices.size()].uv;
            loop.signedArea+=cross(origin,a,b)*.5;
            if(a.Distance(b)<=eps) fail("ZeroUVSegment","Repeated UV vertex in chart polygon",wi);
        }
        if(std::abs(loop.signedArea)<=eps*scale(loop)) fail("DegenerateUVLoop","UV area is zero",wi);
        bool crossing=false;
        for(Id i=0;i<loop.vertices.size()&&!crossing;++i) for(Id j=i+1;j<loop.vertices.size();++j) {
            if(j==i+1 || (i==0 && j+1==loop.vertices.size())) continue;
            if(intersects(loop.vertices[i].uv,loop.vertices[(i+1)%loop.vertices.size()].uv,
                          loop.vertices[j].uv,loop.vertices[(j+1)%loop.vertices.size()].uv,eps)) {
                input.refinementOccurrences.push_back(loop.vertices[i].occurrence);
                input.refinementOccurrences.push_back(loop.vertices[j].occurrence);
                crossing=true;break;
            }
        }
        if(crossing) fail("SelfIntersectingUVLoop","CAD-derived polygon intersects itself in this chart",wi);
        input.loops.push_back(std::move(loop));
    }
    if(!input.issues.empty()) return input;
    if(std::count_if(input.loops.begin(),input.loops.end(),[](const auto& l){return l.outer;})!=1) {
        fail("InvalidOuterWire","A single-chart adapter requires exactly one CAD outer wire");return input;
    }
    const auto outer=std::find_if(input.loops.begin(),input.loops.end(),[](const auto& l){return l.outer;});
    const double eps=scale(*outer)*1.e-12;const auto outerCenter=center(*outer);
    for(auto& hole:input.loops) if(!hole.outer) {
        const auto holeCenter=center(hole);
        const double baseU=face.uPeriod?std::round((outerCenter.X()-holeCenter.X())/face.uPeriod)*face.uPeriod:0;
        const double baseV=face.vPeriod?std::round((outerCenter.Y()-holeCenter.Y())/face.vPeriod)*face.vPeriod:0;
        std::vector<ChartLoop> candidates;
        for(int ku=face.uPeriod?-1:0;ku<=(face.uPeriod?1:0);++ku)
            for(int kv=face.vPeriod?-1:0;kv<=(face.vPeriod?1:0);++kv) {
                ChartLoop candidate=hole;
                for(auto& v:candidate.vertices)v.uv.SetCoord(v.uv.X()+baseU+ku*face.uPeriod,v.uv.Y()+baseV+kv*face.vPeriod);
                if(std::all_of(candidate.vertices.begin(),candidate.vertices.end(),[&](const auto& v){return locate(*outer,v.uv,eps)==1;}) &&
                    !crossLoops(*outer,candidate,eps)) candidates.push_back(std::move(candidate));
            }
        if(candidates.size()!=1) {fail("AmbiguousHoleChart","CAD hole has no unique contained periodic image",hole.wire);continue;}
        hole=std::move(candidates.front());
    }
    if(!input.issues.empty()) return input;
    for(Id a=0;a<input.loops.size();++a) if(!input.loops[a].outer)
        for(Id b=a+1;b<input.loops.size();++b) if(!input.loops[b].outer) {
            const auto& x=input.loops[a];const auto& y=input.loops[b];
            if(crossLoops(x,y,eps)||locate(x,y.vertices.front().uv,eps)||locate(y,x.vertices.front().uv,eps))
                fail("OverlappingHoles","Hole polygons intersect or nest in a single chart",x.wire);
        }
    // Consumer convention: outer counterclockwise, holes clockwise. Provenance
    // remains attached to each vertex, including its original occurrence index.
    for(auto& loop:input.loops) if((loop.signedArea>0)!=loop.outer) {
        std::reverse(loop.vertices.begin(),loop.vertices.end());loop.signedArea=-loop.signedArea;
    }
    input.ready=input.issues.empty();return input;
}
bool ContainsUV(const FaceBoundaryInput& input,const gp_Pnt2d& p) {
    if(!input.ready) return false;
    bool inside=false;
    for(const auto& loop:input.loops) {
        const int state=locate(loop,p,scale(loop)*1.e-12);
        if(loop.outer) inside=state!=0;
        else if(state!=0) return false;
    }
    return inside;
}
}
