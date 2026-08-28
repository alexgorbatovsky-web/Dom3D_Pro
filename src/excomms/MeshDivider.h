#pragma once

namespace comms {

	
// The active functor for cMeshContainer::DivideMesh().
// @todo fine  Template this class more to 'cList' and 'UnlimitedBitset'.
//       See Bevel2::opearator()().
// @see MeshCutterAndDivider
template< typename V >
class MeshDivider {
public:
	typedef cList< V >  vertices_t;

public:
	// Divide and harvest the 'vertex' to 'a' or 'b' depending on 'point'.
	virtual void operator()( const cVec3& point, const V& vertex ) = 0;

	const vertices_t& GetA() const { return a; }
	const vertices_t& GetB() const { return b; }

protected:
	// lists of vertices which divide the mesh on the two parts
	vertices_t  a, b;
};


typedef MeshDivider< int >    MeshDividerIndices;
typedef MeshDivider< cVec3 >  MeshDividerCoords;




template< typename V >
class LineDistanceDivider: public MeshDivider< V > {
public:
	const comms::cLineF3  line;
	const float           radius;

public:
	LineDistanceDivider( const comms::cLineF3& line, float radius ) :
		line( line ),
		radius( radius )
	{
		assert( line.IsValid() );
		assert( radius > 0 );
	}

	virtual ~LineDistanceDivider() {
	}

	virtual void operator()( const cVec3& point, const V& vertex ) {
		const cLineF3::distances_t&  distances = line.Distances( point );
		for (int i = 0; i < distances.Count(); ++i) {
			const float  delta = distances[ i ] - radius;
			// divide this!
			(delta < 0) ? MeshDivider< V >::a.Add( vertex ) : MeshDivider< V >::b.Add( vertex );
		}
	}
};


typedef LineDistanceDivider< int >    LineDistanceDividerIndices;
typedef LineDistanceDivider< cVec3 >  LineDistanceDividerCoords;




template< typename V >
class SphereDivider: public MeshDivider< V > {
public:
	const cVec3  center;
	const float  radiusSq;

public:
	SphereDivider( const cVec3& center, float radius ) :
		center( center ),
		radiusSq( radius * radius )
	{
		assert( radius > 0 );
	}

	virtual ~SphereDivider() {
	}

	virtual void operator()( const cVec3& point, const V& vertex ) {
		const float  distanceSq = (point - center).LengthSq();
		// divide this!
		(distanceSq < radiusSq) ? MeshDivider< V >::a.Add( vertex ) : MeshDivider< V >::b.Add( vertex );
	}
};


typedef SphereDivider< int >    SphereDividerIndices;
typedef SphereDivider< cVec3 >  SphereDividerCoords;


} // comms
