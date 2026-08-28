#pragma once

#include "SubdivideMeshFigure.h"


namespace comms {

	
// The active class for 'cMeshContainer'.
// @see MeshCutterAndDivider
class MeshCutterAndDivider3 {
public:
	typedef UnlimitedBitset  vertices_t;
	typedef DWORDS2  edge_t;

public:
	// Cut, divide and harvest some vertices of mesh.
	virtual cMeshContainer operator()( const cMeshContainer& ) = 0;
};




template< class DivideFunctor >
class LinesDistanceCutterAndDivider3: public MeshCutterAndDivider3 {
public:
	const comms::cLineSetF3  lines;
	const float              radius;

public:
	LinesDistanceCutterAndDivider3( const comms::cLineSetF3&, float radius );
	virtual cMeshContainer operator()( const cMeshContainer& );

	const vertices_t& GetUndivided() const { return undivided; }

protected:
	DivideFunctor  divideFunctor;
	// lists of vertices which harvested
	vertices_t  undivided;
};




// @todo fine  Move to CPP-file.
template< class DivideFunctor >
inline
LinesDistanceCutterAndDivider3< DivideFunctor >::LinesDistanceCutterAndDivider3(
	const comms::cLineSetF3&  lines,
	float  radius
) :
	lines( lines ),
	radius( radius )
{
#ifdef _DEBUG
	for (int k = 0; k < lines.Count(); ++k) {
		assert( lines[ k ].IsValid() );
	}
	assert( radius > cMath::Epsilon );
#endif
}




template< class DivideFunctor >
inline cMeshContainer
LinesDistanceCutterAndDivider3< DivideFunctor >::operator()( const cMeshContainer& mesh ) {

	// process by raw of mesh
	const auto&  raw = mesh.GetRaw();
	const auto&  positions = mesh.GetPositions();

	cMeshContainer  r;
	r.SetDefaultObjMtl(); 
	// copy all positions for fast processing
	r.GetPositions() = positions;	

	undivided.clear();
	cList< int >  beginRaws;
	for (int i = 0; i < raw.Count(); ++i) {
		const int  n = raw[ i ][ 0 ];

		bool  nearby = false;
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + j + 1;
			const int  ai = raw[ aiRaw ][ 0 ];
			const cVec3&  a = positions[ ai ];
			if ( lines.HasNear( a, radius ) ) {
				nearby = true;
				break;
			}
		} // for (int j = 0; ...

		const bool  yes = (n >= 3) && nearby;
		if ( yes ) {
			// harvest a position of figure for divide
			beginRaws.Add( i );

		} // if ( yes )
		else {
			// direct copy a figure to the 'r'
			for (int j = 0; j <= n; ++j) {
				const int  q = i + j;
				const cVec3i&  v = raw[ q ];
				r.GetRaw().Add( v );
				// # All positions of the 'r' already copied. See above.
				// burn the undivided vertices
				const int  aiRaw = i + j + 1;
				const int  ai = raw[ aiRaw ][ 0 ];
				undivided.set( ai, true );
				//DbgLayer( "LinesDistanceCutterAndDivider3() undivided" );
				//AddDbgPoint( positions[ ai ], 0xFF22FF22 );
			} // for (int j = 0; ...
		} // else if ( yes )

		// @test
		//std::cout << "\nLinesDistanceCutterAndDivider3()  #i = " << i << "  mesh raw\n" <<
		//	r.GetRaw() << std::endl;

		i += n;
	} // for (int i = 0; ...


#if 0
	// process by figures for divide
	for (int e = 0; e < beginRaws.Count(); ++e) {
		const int  i = beginRaws[ e ];
		const int  n = raw[ i ][ 0 ];

		cMeshContainer::figure_t  fi;
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + j + 1;
			const int  ai = raw[ aiRaw ][ 0 ];
			const cVec3&  a = positions[ ai ];
			fi.Add( a );
		} // for (int j = 0; ...

		const cMeshContainer::figures_t  fis = divideFunctor( fi );
		for (int u = 0; u < fis.Count(); ++u) {
			const cMeshContainer::figure_t&  divfi = fis[ u ];
			cMeshContainer::figureI_t  divfiI;
			r.Insert( divfi, &divfiI );

			// review to nearest
			for (int k = 0; k < divfi.Count(); ++k) {
				const cVec3&  p = divfi[ k ];
				if ( !lines.HasNear( p, radius ) ) {
					// fixed figure to the 'undivided'
					for (int q = 0; q < divfi.Count(); ++q) {
						const int  index = divfiI[ q ];
                        if ( !undivided.get( index ) ) {
                            DbgLayer( "LinesDistanceCutterAndDivider3() undivided+" );
                            AddDbgPoint( divfi[ q ], 0xFFFF22FF );
                        }
                        undivided.set( index, true );
                    }
					break;
				} // else if ...
				else {
					for (int q = 0; q < divfi.Count(); ++q) {
						DbgLayer( "LinesDistanceCutterAndDivider3() undivided-" );
						AddDbgPoint( divfi[ q ], 0xFF995511 );
					}
				}
			} // for (int k = 0; ...
		} // for (int u = 0; ...
	} // for (int e = 0; ...
#else
    r.GetRaw() = raw;
#endif


	//std::cout << "\nLinesDistanceCutterAndDivider3() r\n" << r << std::endl;
	//DbgLayer( "LinesDistanceCutterAndDivider3() result" );
	//r.DrawDbg( cMat4::Identity, 0xFF0000FF, 0xFF000055 );
	//cLog::Message( "\n\nLinesDistanceCutterAndDivider3() isValid()\n" );
	//r.IsValid( true );
	//comms::cMeshIO::SaveMesh( r, "LinesDistanceCutterAndDivider3.result.auto.obj" );

	return r;
}


} // comms
