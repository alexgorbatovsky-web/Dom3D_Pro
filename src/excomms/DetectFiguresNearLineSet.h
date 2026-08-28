#pragma once

namespace comms {
#include "../3D-Coat/cLineSet/cLineSet.h"
}


namespace comms {

	
// The active class for 'cMeshContainer'.
// @see MeshDivider
class DetectFiguresNearLineSet {
public:
	// list of begin-raw-figure from a mesh
	typedef cList< int >  beginRawFigures_t;

public:
	const cLineSetF3  lines;
	const float  radius;

public:
	DetectFiguresNearLineSet( const cLineSetF3&, float radius );

	// Harvest nearest vertices from the mesh.
	// @return Lists the figures (position in the raw) which have been
	//         lying near.
	beginRawFigures_t operator()( cMeshContainer* ) const;

protected:
	bool IsNear( const cVec3& point ) const;

private:
	const float  radiusSq;
};




inline
DetectFiguresNearLineSet::DetectFiguresNearLineSet(
	const cLineSetF3&  lines,
	float  radius
) :
	lines( lines ),
	radius( radius ),
	radiusSq( radius * radius )
{
	assert( !lines.IsEmpty() );
	assert( radius >= cMath::Epsilon );
	// @todo optimize  Build tree for faster detect.
}




inline DetectFiguresNearLineSet::beginRawFigures_t
DetectFiguresNearLineSet::operator()( cMeshContainer* mesh ) const {
	assert( mesh );

	beginRawFigures_t  r;

	const auto&  raw = mesh->GetRaw();
	const auto&  positions = mesh->GetPositions();

	// process by raw of mesh
	// @todo optimize  Split by threads.
	//       See 'LinesDistanceCutterAndDivider< V >::Parallel'.
	for (int i = 0; i < raw.Count(); ++i) {
		const int  n = raw[ i ][ 0 ];
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + j + 1;
			const int  ai = raw[ aiRaw ][ 0 ];
			const cVec3&  a = positions[ ai ];
			if ( IsNear( a ) ) {
				r.Add( i );
				// # One point is enough.
				break;
			}
		} // for (int j = 0; j < n; ++j)
		i += n;
	} // for (int i = 0; i < raw.Count(); ++i)

	return r;
}





inline bool
DetectFiguresNearLineSet::IsNear( const cVec3& point ) const {

	const cLineF3::sharpNode_t  p =
		static_cast< cLineF3::sharpNode_t >( point );
#if 0
	// I
	for (int k = 0; k < lines.Count(); ++k) {
		const cLineF3&  line = lines[ k ];
		for (int i = 0; i < line.CountSegment(); ++i) {
			const float  dSq =
				p.DistanceToLineSegSq( line[ i ],  line[ i + 1 ] );
			if (dSq < radiusSq) {
				return true;
			}
		} // for (int i = 0; i < line.CountSegment(); ++i)
	} // for (int k = 0; k < lines.Count(); ++k)

	return false;

#else
	// II
	return lines.HasNear( p, radius );
#endif
}


} // comms
