#pragma once

#include "cMeshContainer.h"


namespace comms {


// The active class for 'cMeshContainer'.
class SubdivideMeshFigure {
public:
	virtual cMeshContainer::figures_t operator()( const cMeshContainer::figure_t& ) const = 0;
};




template< size_t depth >
class SubdivideMeshFigureByTriangles: public SubdivideMeshFigure {
public:
	virtual cMeshContainer::figures_t operator()( const cMeshContainer::figure_t& ) const;

private:
	static void divide(
		cMeshContainer::figures_t*,
		const cMeshContainer::figure_t&,
		size_t  currentDepth = 0
	);
};




template< size_t depth >
inline cMeshContainer::figures_t
SubdivideMeshFigureByTriangles< depth >::operator()(
	const cMeshContainer::figure_t&  fi
) const {
	if ( fi.IsEmpty() ) {
		throw Exception( "Figure is empty." );
	}

	cMeshContainer::figures_t  r;
	divide( &r, fi );

	return r;
}




template< size_t depth >
inline void
SubdivideMeshFigureByTriangles< depth >::divide(
	cMeshContainer::figures_t*  r,
	const cMeshContainer::figure_t&  fi,
	size_t  currentDepth
) {
	// find a center point
	const int  n = fi.Count();
	cVec3  center = fi[ 0 ];
	for (int i = 1; i < n; ++i) {
		center += fi[ i ];
	}
	center /= (float)n;

	for (int i = 0; i < n; ++i) {
		const int  aRaw = i;
		const int  bRaw = (i + 1) % n;
		const cVec3&  a = fi[ aRaw ];
		const cVec3&  b = fi[ bRaw ];
		cMeshContainer::figure_t  nfi;
		nfi.Add( center );
		nfi.Add( a );
		nfi.Add( b );
		if ( currentDepth == depth ) {
			r->Add( nfi );
		} else {
			divide( r, nfi, currentDepth + 1 );
		}
	} // for (int i = 0; i < n; ++i)
}




template< size_t depth >
class SubdivideMeshFigureByQuads: public SubdivideMeshFigure {
public:
	virtual cMeshContainer::figures_t operator()( const cMeshContainer::figure_t& ) const;

private:
	static void divide(
		cMeshContainer::figures_t*,
		const cMeshContainer::figure_t&,
		size_t  currentDepth = 0
	);
};




template< size_t depth >
inline cMeshContainer::figures_t
SubdivideMeshFigureByQuads< depth >::operator()(
	const cMeshContainer::figure_t&  fi
) const {
	if ( fi.IsEmpty() ) {
		throw Exception( "Figure is empty." );
	}

	cMeshContainer::figures_t  r;
	divide( &r, fi );

	return r;
}




template< size_t depth >
inline void
SubdivideMeshFigureByQuads< depth >::divide(
	cMeshContainer::figures_t*  r,
	const cMeshContainer::figure_t&  fi,
	size_t  currentDepth
) {
	// find a center point
	const int  n = fi.Count();
	cVec3  center = fi[ 0 ];
	for (int i = 1; i < n; ++i) {
		center += fi[ i ];
	}
	center /= (float)n;

	for (int i = 0; i < n; ++i) {
		const int  aRaw = i;
		const int  bRaw = (i + 1) % n;
		const int  cRaw = (i + 2) % n;
		const cVec3&  a = fi[ aRaw ];
		const cVec3&  b = fi[ bRaw ];
		const cVec3&  c = fi[ cRaw ];
		const cVec3  pab = (a + b) / 2;
		const cVec3  pbc = (b + c) / 2;
		cMeshContainer::figure_t  nfi;
		nfi.Add( center );
		nfi.Add( pab );
		nfi.Add( b );
		nfi.Add( pbc );
		if ( currentDepth == depth ) {
			r->Add( nfi );
		} else {
			divide( r, nfi, currentDepth + 1 );
		}
	} // for (int i = 0; i < n; ++i)
}


} // comms
