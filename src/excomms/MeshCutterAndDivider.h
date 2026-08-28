#pragma once

#include "DetectFiguresNearLineSet.h"

namespace comms {

	
// The active class for 'cMeshContainer'.
// @todo fine  Template this class more to 'cList' and 'UnlimitedBitset'.
//       See Bevel2::opearator()().
// @see MeshCutterAndDivider*, MeshDivider, DetectFiguresNearLineSet
template< typename V >
class MeshCutterAndDivider {
public:
	typedef cList< V >    vertices_t;
	typedef cList< int >  divided_t;

	// Mode for more faster process.
	enum MaskMode {
		HarvestOnlyA = 1 << 0,
		HarvestOnlyB = 1 << 1,
		// @todo HarvestOnlyMiddle = 1 << 2,  # Today: always harvest.
		HarvestAll = 0xFF
	};

public:
	// Cut, divide and harvest the 'vertex' to 'a' or 'b' depending on 'point'.
	virtual void operator()( cMeshContainer*, enum MaskMode ) = 0;

	const vertices_t& GetA() const { return va; }
	const vertices_t& GetB() const { return vb; }
	const vertices_t& GetMiddle() const { return vmiddle; }

	// #! Actual only for the original mesh.
	const divided_t& GetDivided() const { return divided; }

protected:
	// lists of vertices which cutted and divide the mesh
	vertices_t  va, vb, vmiddle;
	// begin raw blocks of figures which divided
	// @see GetDivided()
	cList< int >  divided;
};


typedef MeshCutterAndDivider< int >    MeshCutterAndDividerIndices;
typedef MeshCutterAndDivider< cVec3 >  MeshCutterAndDividerCoords;




template< typename V >
class LinesDistanceCutterAndDivider: public MeshCutterAndDivider< V > {
public:
	typedef LinesDistanceCutterAndDivider< V >  Parent;

public:
	const comms::cLineSetF3  lines;
	const float              radius;

public:
	LinesDistanceCutterAndDivider( const comms::cLineSetF3& lines, float radius );
	virtual ~LinesDistanceCutterAndDivider();
	virtual void operator()( cMeshContainer*, enum MeshCutterAndDivider< V >::MaskMode );

protected:
	enum OnSide {
		OnSideUndefined = 0,
		OnSideA,
		OnSideB,
		OnSideDivider
	};

	float CalcNepper( const cVec3& point1, const cVec3& point2 ) const;

	// @see operator()
	struct dividedFigure_t {
		int  beginRawFigure;
		// vertices and neppers
		// @see cMeshContainer::Divide()
		int  ai, bi;
		float  m;
		int  ci, di;
		float  n;
	};
	typedef cList< dividedFigure_t >  dividedFigures_t;

	// for calc in threads
	typedef struct mutexes_t {
		std::mutex  va, vb;
	} mutexes_t;

	// @see operator()
	// #! 'dividedFigures_t' work without mutex: must use separate lists for
	//    every thread.
	template< size_t current, size_t all >
	struct Parallel {
		Parallel(
			dividedFigures_t*,
			typename MeshCutterAndDivider< V >::vertices_t*  va,
			typename MeshCutterAndDivider< V >::vertices_t*  vb,
			mutexes_t*,
			const cMeshContainer*,
			const DetectFiguresNearLineSet::beginRawFigures_t*,
			const cLineSetF3*  lines,
			float  radius
		);
		void operator()() const;
		OnSide DetectOnSide( int vertex ) const;

		const cMeshContainer* const  mesh;
		const DetectFiguresNearLineSet::beginRawFigures_t* const  preparedFigures;
		const cLineSetF3* const  lines;
		const float  radius;
		const float  radiusSq;
		mutable dividedFigures_t*  dfl;
		mutable typename MeshCutterAndDivider< V >::vertices_t*  va;
		mutable typename MeshCutterAndDivider< V >::vertices_t*  vb;
		mutable mutexes_t*  mutexes;
		mutable uni_hash< bool, int >*  skipped;
	};

private:
	const float  radiusSq;
};


typedef LinesDistanceCutterAndDivider< int >    LinesDistanceCutterAndDividerIndices;
typedef LinesDistanceCutterAndDivider< cVec3 >  LinesDistanceCutterAndDividerCoords;




template< typename V >
inline
LinesDistanceCutterAndDivider< V >::LinesDistanceCutterAndDivider(
	const comms::cLineSetF3&  lines,
	float  radius
) :
	lines( lines ),
	radius( radius ),
	radiusSq( radius * radius )
{
#ifdef _DEBUG
	for (int k = 0; k < lines.Count(); ++k) {
		assert( lines[ k ].IsValid() );
	}
	assert( radius > cMath::Epsilon );
#endif
}


	
	
template< typename V >
inline
LinesDistanceCutterAndDivider< V >::~LinesDistanceCutterAndDivider() {
}




template< typename V >
inline void
LinesDistanceCutterAndDivider< V >::operator()(
	cMeshContainer*  mesh,
	enum MeshCutterAndDivider< V >::MaskMode  mode
) {
	assert( mesh );

	// prepare a figures which must be divide
	DetectFiguresNearLineSet  detectNear( lines, radius * 1.1f );
	const DetectFiguresNearLineSet::beginRawFigures_t  preparedFigures =
		detectNear( mesh );

	// @test
	for (int k = 0; k < preparedFigures.Count(); ++k) {
		const int  pf = preparedFigures[ k ];
		DbgLayer( "LinesDistanceCutterAndDivider() preparedFigures" );
		AddDbgFigure( *mesh, pf, 0xFF117711 );
	}


	// process by the harvested figures
	dividedFigures_t  dfl;
#if 0
	// one thread
	{
		mutexes_t  mutexes;
		const Parallel< 1, 1 >  a(
			&dfl,
			(mode & HarvestOnlyA) ? &va : NULL,
			(mode & HarvestOnlyB) ? &vb : NULL,
			&mutexes, mesh, &preparedFigures, &lines, radius
		);
		a();
	}
#else
	// some threads
	// @todo error  Don't work without mutexes.
	{
		dividedFigures_t  rt[ 4 ];
		mutexes_t  mutexes;
		const Parallel< 1, 4 >  a(
			&rt[ 1 - 1 ],
			(mode & MeshCutterAndDivider< V >::HarvestOnlyA) ? &this->va : NULL,
			(mode & MeshCutterAndDivider< V >::HarvestOnlyB) ? &this->vb : NULL,
			&mutexes, mesh, &preparedFigures, &lines, radius
		);
		const Parallel< 2, 4 >  b(
			&rt[ 2 - 1 ],
			(mode & MeshCutterAndDivider< V >::HarvestOnlyA) ? &this->va : NULL,
			(mode & MeshCutterAndDivider< V >::HarvestOnlyB) ? &this->vb : NULL,
			&mutexes, mesh, &preparedFigures, &lines, radius
		);
		const Parallel< 3, 4 >  c(
			&rt[ 3 - 1 ],
			(mode & MeshCutterAndDivider< V >::HarvestOnlyA) ? &this->va : NULL,
			(mode & MeshCutterAndDivider< V >::HarvestOnlyB) ? &this->vb : NULL,
			&mutexes, mesh, &preparedFigures, &lines, radius
		);
		const Parallel< 4, 4 >  d(
			&rt[ 4 - 1 ],
			(mode & MeshCutterAndDivider< V >::HarvestOnlyA) ? &this->va : NULL,
			(mode & MeshCutterAndDivider< V >::HarvestOnlyB) ? &this->vb : NULL,
			&mutexes, mesh, &preparedFigures, &lines, radius
		);
        // Error: Use of undeclared identifier 'tbb::parallel_invoke'
		//tbb::parallel_invoke( a, b, c, d );
		dfl.AddRange( rt[ 0 ] );
		dfl.AddRange( rt[ 1 ] );
		dfl.AddRange( rt[ 2 ] );
		dfl.AddRange( rt[ 3 ] );
	}
#endif

	/* @test
	for (int k = 0; k < dfl.Count(); ++k) {
		const dividedFigure_t&  df = dfl[ k ];
		DbgLayer( "LinesDistanceCutterAndDivider() dfl" );
		AddDbgFigure( *mesh, df.beginRawFigure, 0xFF771111 );
	}
	*/


	// process harvested figures and divide them
	cList< int >  changed;
	for (int k = 0; k < dfl.Count(); ++k) {
		const dividedFigure_t&  df = dfl[ k ];

		// # All new figures has added to end of raw.
		// @see cMeshContainer::Divide()
		::std::pair< cVec3, cVec3 >  coords;
		try {
			coords = mesh->Divide< false >(
				df.ai, df.bi, df.m,
				df.ci, df.di, df.n
			);
		} catch ( const Exception& ) {
			// @test
			DbgLayer( "LinesDistanceCutterAndDivider() error m--n" );
			static const DWORD  color = 0xFFAA2222;
			AddDbgLine( coords.first, coords.second, color, color );
			AddDbgPoint( coords.first, color );
			AddDbgPoint( coords.second, color );

			// # Just skip errors by call Divide().
			continue;
		}

		// @test
		{
			DbgLayer( "LinesDistanceCutterAndDivider() good m--n" );
			static const DWORD  color = 0xFFAAAA22;
			AddDbgLine( coords.first, coords.second, color, color );
			AddDbgPoint( coords.first, color );
			AddDbgPoint( coords.second, color );
		}

		const int  vertex1 = mesh->GetVertex( coords.first );
		assert( vertex1 != -1 );
		MeshCutterAndDivider< V >::vmiddle.Add( vertex1 );
		const int  vertex2 = mesh->GetVertex( coords.second );
		assert( vertex2 != -1 );
		MeshCutterAndDivider< V >::vmiddle.Add( vertex2 );

		/* - Don't have a sense, because the mesh has changing after Divide().
		divided.Add( df.beginRawFigure );
		*/
	} // for (int k = 0; k < dfl.Count(); ++k)

	mesh->EraseClearConfluentByIndexOnly();

	//const int  beginRawBlockEtalon = 0;
	//mesh->CorrectFaces( beginRawBlockEtalon );
}




template< typename V >
inline float
LinesDistanceCutterAndDivider< V >::CalcNepper(
	const cVec3&  point1,
	const cVec3&  point2
) const {
	assert( false
		&& "@todo" );
	return FLT_MAX;

#if 0
	// @todo
	const float  distance = point1.Distance( point2 );
	if ( cMath::IsZero( distance, 0.1f ) ) {
		// @todo stable  Replace on assert()?
		return 0.0f;
	}

	// search a nearest line...
	const cLineF3*  nearestLine = NULL;
	// ...and get distances for points from this line
	float  distance1, distance2, dmin;
	distance1 = distance2 = dmin = FLT_MAX;
	for (int k = 0; k < lines.Count(); ++k) {
		const cLineF3&  line = lines[ k ];
		const float  d1 = line.DistanceMin( point1 );
		const float  d2 = line.DistanceMin( point2 );
		const float  davg = (d1 + d2) * 0.5f;
		if (davg < dmin) {
			dmin = davg;
			nearestLine = &lines[ k ];
			distance1 = d1;
			distance2 = d2;
		}
	} // for (int k = 0; k < lines.Count(); ++k)
	assert( nearestLine );

	// calc a nepper for line and points
	if ( cMath::IsZero( distance1, 0.1f ) ) {
		return 0.0f;
	}
	if ( cMath::IsZero( distance2, 0.1f ) ) {
		return 1.0f;
	}

	return (distance1 > distance) ? 1.0f : (distance1 / distance);
#endif
}




template< typename V >
template< size_t current, size_t all >
inline
LinesDistanceCutterAndDivider< V >::Parallel< current, all >::Parallel(
	dividedFigures_t*  dfl,
	typename MeshCutterAndDivider< V >::vertices_t*  va,
	typename MeshCutterAndDivider< V >::vertices_t*  vb,
	mutexes_t*  mutexes,
	const cMeshContainer*  mesh,
	const DetectFiguresNearLineSet::beginRawFigures_t*  preparedFigures,
	const cLineSetF3*  lines,
	float  radius
) :
	dfl( dfl ),
	va( va ),
	vb( vb ),
	mutexes( mutexes ),
	mesh( mesh ),
	preparedFigures( preparedFigures ),
	lines( lines ),
	radius( radius ),
	radiusSq( radius * radius )
{
	assert( dfl );
	//assert( va );  - can be undefined
	//assert( vb );  - can be undefined
	assert( mutexes || (!va && !vb) );
	assert( mesh );
	assert( preparedFigures );
	assert( radius > cMath::Epsilon );
	assert( lines && !lines->IsEmpty() );
}




template< typename V >
template< size_t current, size_t all >
inline void
LinesDistanceCutterAndDivider< V >::Parallel< current, all >::operator()() const {

	assert( current >= 1 );
	assert( all >= 1 );
	assert( current <= all );

	const int  n = (int)( preparedFigures->Count() / all );
	const bool tiny = (n == 0);
	if (tiny && (current > 1)) {
		// # Run only one thread.
		return;
	}
	const int  from = tiny ? 0 : (int)((current - 1) * n);
	const int  to = (tiny || (current == all)) ?
		preparedFigures->Count() : (int)(from + n);

	const auto&  raw = mesh->GetRaw();
	const auto&  positions = mesh->GetPositions();

	assert( to <= preparedFigures->Count() );
	for (int k = from; k < to; ++k) {
		const int  i = ( *preparedFigures )[ k ];
		const int  n = raw[ i ][ 0 ];
		// skip lines, points and empty blocks
		if (n < 3) {
			continue;
		}

		bool  untouched = true;
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + 1 + j;
			const int  biRaw = i + 1 + (j + 1) % n;
			const int  ai = raw[ aiRaw ][ 0 ];
			const int  bi = raw[ biRaw ][ 0 ];

			const enum OnSide  as = DetectOnSide( ai );
			const enum OnSide  bs = DetectOnSide( bi );
			if (as != bs) {
				// # '.' is vertices of figure.
				// # '*' is divider.
				// # '@' is intersections.
				// .-----------.
				//  \          |
				//   \         |
				//    @********@
				//     \       |
				//      \      |
				//       .-----.
				// search 2 edges of figure which need cut
				dividedFigure_t  df = {};
				df.ci = -1;
				df.beginRawFigure = i;
				// @todo Divide by distance of the divider.
				df.ai = ai;
				df.bi = bi;
				df.m = 0.5f;  //@todo CalcNepper( a, b );
				// search other edge (next point for divider)
#if 1
				// strong detect
				j++;
				for ( ; j < n; ++j) {
					const int  aiRaw = i + j + 1;
					const int  biRaw = i + 1 + (j + 1) % n;
					const int  ai = raw[ aiRaw ][ 0 ];
					const int  bi = raw[ biRaw ][ 0 ];
					// @todo optimize Detect 'bs' equals last 'as'.
					const enum OnSide  as = DetectOnSide( ai );
					const enum OnSide  bs = DetectOnSide( bi );
					if (as != bs) {
						df.ci = ai;
						df.di = bi;
						df.n = 0.5f;  //@todo CalcNepper( c, d );
						// # Need only 4 vertices for divider.
						break;
					}
				} // for ( ; j < n; ++j)

#else
				// fast detect
				const int  delta = (n > 3) ? 2 : 1;
				const int  ciRaw = i + 1 + (j + delta);
				const int  diRaw = i + 1 + (j + 1 + delta) % n;
				df.ci = raw[ ciRaw ][ 0 ];
				df.di = raw[ diRaw ][ 0 ];
				df.n = 0.5f;  //@todo CalcNepper( c, d );
#endif

				const bool  found = (df.ci != -1);
				if ( found ) {
					dfl->Add( df );
					untouched = false;
				}
			} // if (as != bs)
		} // for (int j = 0; j < n; ++j)
	} // for (int k = 0; k < preparedFigures.Count(); ++k)
}





template< typename V >
template< size_t current, size_t all >
inline typename LinesDistanceCutterAndDivider< V >::OnSide
LinesDistanceCutterAndDivider< V >::Parallel< current, all >::DetectOnSide(
	int vertex
) const {
	const cVec3&  point = mesh->GetPosition( vertex );
	const cLineF3::sharpNode_t  p =
		static_cast< cLineF3::sharpNode_t >( point );
#if 0
	// I
	for (int k = 0; k < lines->Count(); ++k) {
		const cLineF3&  line = ( *lines )[ k ];
		for (int i = 0; i < line.CountSegment(); ++i) {
			const float  dSq =
				p.DistanceToLineSegSq( line[ i ], line[ i + 1 ] );
			/* - @todo extend  Harvet 'vmiddle'.
			if ( cMath::Equals( dSq, elta ) ) {
				// @todo vmiddle.Add( vertex );
				//return OnSideDivider;
				va->Add( vertex );
				return OnSideA;
			}
			*/
			if (dSq < radiusSq) {
				if ( va ) {
					va->Add( vertex );
				}
				return OnSideA;
			}
		} // for (int i = 0; i < line.CountSegment(); ++i)
	} // for (int k = 0; k < lines.Count(); ++k)

	if ( vb ) {
		vb->Add( vertex );
	}
	return OnSideB;

#else
	// II
	const bool  r = lines->HasNear( p, radius );
	if ( r ) {
		if ( va ) {
			std_scoped_lock  lock( mutexes->va );
			va->Add( vertex );
		}
		return OnSideA;
	}

	if ( vb ) {
		std_scoped_lock  lock( mutexes->vb );
		vb->Add( vertex );
	}
	return OnSideB;
#endif
}


} // comms
