#pragma once
#include "DraftingDimensions.h"
#include <BRepAdaptor_Curve.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <cmath>

namespace drafting {
inline void CaptureEdge(Reference& ref,const TopTools_IndexedMapOfShape& edges,int index) {
    BRepAdaptor_Curve curve(TopoDS::Edge(edges(index)));
    ref.edge=index;ref.edge_count=edges.Extent();ref.curve_type=int(curve.GetType());
    ref.edge_points.clear();
    for(int i=0;i<9;++i) {
        const auto p=curve.Value(curve.FirstParameter()+(curve.LastParameter()-curve.FirstParameter())*i/8.);
        ref.edge_points.push_back({p.X(),p.Y(),p.Z()});
    }
}
// Match the complete curve, not just the picked endpoint (which may be shared).
// A changed edge count never authorizes using the previous numeric edge index.
inline bool ResolveEdge(Reference& ref,const TopTools_IndexedMapOfShape& edges) {
    const auto compatible=[&](int index) {
        return index>0 && index<=edges.Extent()
            && int(BRepAdaptor_Curve(TopoDS::Edge(edges(index))).GetType())==ref.curve_type;
    };
    const auto match=[&](int index,bool& reverse) {
        if(ref.edge_points.size()!=9 || !compatible(index))return false;
        Reference candidate;CaptureEdge(candidate,edges,index);
        double direct=0,reversed=0;
        for(int i=0;i<9;++i)for(int axis=0;axis<3;++axis) {
            direct=std::max(direct,std::abs(ref.edge_points[i][axis]-candidate.edge_points[i][axis]));
            reversed=std::max(reversed,std::abs(ref.edge_points[i][axis]-candidate.edge_points[8-i][axis]));
        }
        reverse=reversed<direct;return std::min(direct,reversed)<1.e-5;
    };
    bool reverse=false;int found=0;
    if(ref.edge_count==edges.Extent() && match(ref.edge,reverse))found=ref.edge;
    else if(!ref.edge_points.isEmpty()) {
        for(int i=1;i<=edges.Extent();++i) {
            bool flipped=false;if(!match(i,flipped))continue;
            if(found)return false; // Coincident alternatives are ambiguous.
            found=i;reverse=flipped;
        }
    }
    // Keep the existing parametric-update behaviour when topology is unchanged.
    if(!found && ref.edge_count==edges.Extent() && compatible(ref.edge)){found=ref.edge;reverse=false;}
    if(!found)return false;
    if(reverse)ref.parameter=1-ref.parameter;
    CaptureEdge(ref,edges,found);return true;
}
}
