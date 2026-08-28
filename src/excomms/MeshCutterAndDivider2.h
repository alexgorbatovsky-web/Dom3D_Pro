#pragma once

namespace comms {

	
// The active class for 'cMeshContainer'.
// @see MeshCutterAndDivider
class MeshCutterAndDivider2 {
public:
	typedef UnlimitedBitset  vertices_t;
	typedef DWORDS2  edge_t;
	typedef ::std::pair< cVec< float, 3 >, int >  pp_t;

public:
	// Cut, divide and harvest vertices of mesh.
	virtual void operator()( cMeshContainer* ) = 0;

	const vertices_t& Get() const { return harvested; }

protected:
	// lists of vertices which harvested
	// @example Vertices near with dividers.
	vertices_t  harvested;
	
	// @see Divide()
	mutable uni_hash< pp_t, edge_t >  insertedEdges;
};




class LinesDistanceCutterAndDivider2: public MeshCutterAndDivider2 {
public:
	const comms::cLineSetF3  lines;
	const float              radius;

public:
	LinesDistanceCutterAndDivider2( const comms::cLineSetF3&, float radius );
	virtual ~LinesDistanceCutterAndDivider2();
	virtual void operator()( cMeshContainer* );

protected:
	// # Old figures don't remove from raw of mesh, don't any mark.
	// @see operator()
	bool Divide( cMeshContainer*, int beginRawFigure ) const;
};




// @todo fine  Move to CPP-file.
inline
LinesDistanceCutterAndDivider2::LinesDistanceCutterAndDivider2(
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


	
	
inline
LinesDistanceCutterAndDivider2::~LinesDistanceCutterAndDivider2() {
}




inline void
LinesDistanceCutterAndDivider2::operator()( cMeshContainer* mesh ) {
	assert( mesh );

	// process by raw of mesh
	auto&  raw = mesh->GetRaw();
	const auto&  positions = mesh->GetPositions();
	// # All vertices in 'dfs' are sorted by ascendance.
	cList< int >  dfs;
	const int  rawCount = raw.Count();
	for (int i = 0; i < rawCount; ++i) {
		const int  n = raw[ i ][ 0 ];
		// skip empty, points and lines
		if (n < 3) {
			continue;
		}
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + j + 1;
			const int  ai = raw[ aiRaw ][ 0 ];
			const cVec3&  a = positions[ ai ];
			if ( lines.HasNear( a, radius * 1.1f ) ) {
				const bool  divided = Divide( mesh, i );
				if ( divided ) {
					dfs.Add( i );
					for (int k = 0; k < n; ++k) {
						const int  piRaw = i + k + 1;
						const int  pi = raw[ piRaw ][ 0 ];
						harvested.set( pi, true );
					}
				}
				break;
			}
		} // for (int j = 0; j < n; ++j)
		i += n;
	} // for (int i = 0; i < rawCount; ++i)


	// erase divided figures
	// @see Convention for Divide().
	for (int k = 0, delta = 0; k < dfs.Count(); ++k) {
		const int  i = dfs[ k ] - delta;
		assert( i >= 0 );
		const int  n = raw[ i ][ 0 ];
		mesh->Erase( i );
		delta += n + 1;
	}


	// add inserted points to same edges
	for (int i = 0; i < rawCount; ++i) {
		const int  n = raw[ i ][ 0 ];
		// skip empty, points and lines
		if (n < 3) {
			continue;
		}
		for (int j = 0; j < n; ++j) {
			const int  iRaw1 = i + j + 1;
			const int  iRaw2 = i + 1 + (j + 1) % n;
			const int  ip1 = raw[ iRaw1 ][ 0 ];
			const int  ip2 = raw[ iRaw2 ][ 0 ];
			const edge_t  edge( ip1, ip2 );
			const pp_t*  pp = insertedEdges.get( edge );
			if ( pp ) {
				const cVec3i  ip( pp->second, raw[ iRaw1 ][ 1 ], raw[ iRaw1 ][ 2 ] );
				raw.Insert( iRaw2, ip );
				raw[ i ][ 0 ]++;
				// shift for added 'i'
				// #! Don't skip 'for (i)', because a mesh consist of
				//    separate edges by every faces.
				//    See cMeshContainerTest.InsertA_X() for example.
				++i;
				// skip 'j' loops
				break;
			}
		} // for (int j = 0; j < n; ++j)
		i += n;
	} // for (int i = 0; i < rawCount; ++i)
}




inline bool
LinesDistanceCutterAndDivider2::Divide( cMeshContainer* mesh, int beginRawFigure ) const {
	assert( mesh );

	// @test
	DbgLayer( "LinesDistanceCutterAndDivider2::Divide() beginRawFigure" );
	AddDbgFigure( *mesh, beginRawFigure, 0xFFFF0000 );

	const auto&  raw = mesh->GetRaw();
	auto&  positions = mesh->GetPositions();
	const int  i = beginRawFigure;
	const int  n = raw[ i ][ 0 ];
	assert( (n >= 3) &&
		"Don't work with empty, points and lines." );

	// find edges for divide and prepare figure
	cMeshContainer::figureI_t  figure;
	cList< ::std::pair< edge_t, pp_t > >  inserted;
	for (int j = 0; j < n; ++j) {
		const int  iRaw1 = i + j + 1;
		const int  iRaw2 = i + 1 + (j + 1) % n;
		const int  ip1 = raw[ iRaw1 ][ 0 ];
		figure.Add( ip1 );
		const int  ip2 = raw[ iRaw2 ][ 0 ];
		const cVec3&  p1 = positions[ ip1 ];
		const cVec3&  p2 = positions[ ip2 ];
		const bool  near1 = lines.HasNear( p1, radius );
		const bool  near2 = lines.HasNear( p2, radius );
		if (near1 != near2) {
			// @todo optimize  Don't harvest a coord, an index is enough.
			const cVec< float, 3 >  p( p1, p2, 0.5f );
			const int  ip = positions.Add( p );
			figure.Add( ip );
			const edge_t  edge( ip1, ip2 );
			const pp_t  pp( p, ip );
			inserted.Add( ::std::make_pair( edge, pp ) );
		}
	} // for (int j = 0; j < n; ++j)

	if (inserted.Count() != 2) {
		// @test
		DbgLayer( "LinesDistanceCutterAndDivider2::Divide() beginRawFigure skip" );
		AddDbgFigure( *mesh, beginRawFigure, 0xFF0000FF );
		// # Skip when the figure is undivided.
		return false;
	}

	for (int k = 0; k < inserted.Count(); ++k) {
		const edge_t  edge = inserted[ k ].first;
		const pp_t&  pp = inserted[ k ].second;
		insertedEdges.add( edge, pp );
	}

	// divide the figure
	cMeshContainer::figureI_t  figure1, figure2;
	cMeshContainer::figureI_t* fptr = &figure1;
	int  prevAddedIP = -1;
	for (int j = 0; j < figure.Count(); ++j) {
		const int  ip = figure[ j ];
		if (prevAddedIP == ip) {
			// switch to work with other figure
			fptr = (fptr == &figure1) ? &figure2 : &figure1;
		} else {
			fptr->Add( ip );
		}
		const int  a = inserted.GetFirst().second.second;
		const int  b = inserted.GetLast().second.second;
		if ((ip == a) || (ip == b)) {
			if (prevAddedIP != ip) {
				fptr->Add( (ip == a) ? b : a );
			}
			// switch to work with other figure
			fptr = (fptr == &figure1) ? &figure2 : &figure1;
		}
		prevAddedIP = ip;
	} // for (int j = 0; j < figure.Count(); ++j)

	// @test
	//std::cout << "  mesh  " << m_Raw << std::endl;
	//std::cout << "  figure1  " << figure1 << std::endl;
	//std::cout << "  figure2  " << figure2 << std::endl;

	// # Insert to the end of 'raw'.
	int  brf1, brf2;
	mesh->Insert( figure1, &brf1 );
	mesh->Insert( figure2, &brf2 );

	// @test
	DbgLayer( "LinesDistanceCutterAndDivider2::Divide() figure1" );
	AddDbgFigure( *mesh, brf1, 0xFFFF00FF );
	DbgLayer( "LinesDistanceCutterAndDivider2::Divide() figure2" );
	AddDbgFigure( *mesh, brf2, 0xFFFF99FF );

	return true;
}


} // comms
