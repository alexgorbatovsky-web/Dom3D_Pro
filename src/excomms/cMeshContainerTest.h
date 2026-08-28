#pragma once

#include "cMeshContainer.h"
#include "../3D-Coat/Exception.h"
#include <gtest/gtest.h>


namespace test {
	namespace meshcontainer {


// # Data for 'TestResources/cone-side-5.obj'.
struct MeshConeSide5 : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d, e, f;
	// absent vertices
	comms::cVec3  absentA, absentB;

	MeshConeSide5() :
		Mesh( "cone-side-5.obj" ),
		a( -1.902390f, 3.082199f, -1.724518f ),  // 1
		b( -1.902390f, 5.082199f, -0.724518f ),  // 2
		c( -0.951333f, 3.082199f, -1.033535f ),  // 3
		d( -1.314604f, 3.082199f,  0.084499f ),  // 4
		e( -2.490175f, 3.082199f,  0.084499f ),  // 5
		f( -2.853446f, 3.082199f, -1.033535f ),  // 6
		absentA(  10.0f,  30.0f,  70.0f ),
		absentB( 100.0f, 300.0f, 700.0f )
	{}
};


// @see cMeshContainerTest.jpg, "A"
struct MeshA : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d;
	int  ai, bi, ci, di;
	// points for insert
	comms::cVec3  e, f, g, h;

	MeshA() :
		a( 10, 15, 0 ),
		b( 45, 15, 0 ),
		c( 35, 35, 0 ),
		d( 55, 30, 0 ),
		e( 20.0f, 15.0f, 0.0f ),
		f( 40.0f, 15.0f, 0.0f ),
		g( 29.53125f, 30.625f, 0.0f ),
		h( 45.0f, 26.0f, 0.0f )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		di = pos.Add( d );
		auto&  raw = mc->GetRaw();
		// left
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		// right
		raw.Add( cVec3i( 3,  0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 2, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "B"
struct MeshB : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c;
	int  ai, bi, ci;
	// points for insert
	comms::cVec3  n, m;

	MeshB() :
		a(  75, 10, 0 ),
		b(  80, 40, 0 ),
		c( 100, 10, 0 ),
		n( 78.0f, 30.0f, 0.0f ),
		m( 95.0f, 10.0f, 0.0f )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 3, pos.Count() );
		EXPECT_EQ( 1, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "C"
struct MeshC : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d;
	int  ai, bi, ci, di;
	// points for insert
	comms::cVec3  n, m, k, l;

	MeshC() :
		a( 115, 10, 0 ),
		b( 125, 35, 0 ),
		c( 140, 30, 0 ),
		d( 150, 10, 0 ),
		n( cVec3::Zero ),
		m( cVec3::Zero ),
		k( cVec3::Zero ),
		l( cVec3::Zero )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		di = pos.Add( d );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  4, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 1, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "D"
struct MeshD : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d;
	int  ai, bi, ci, di;
	// points for insert
	comms::cVec3  n, m, k;

	MeshD() :
		a( 165, 10, 0 ),
		b( 175, 35, 0 ),
		c( 190, 30, 0 ),
		d( 200, 10, 0 ),
		n( b ),
		m( cVec3::Zero ),
		k( d )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		di = pos.Add( d );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  4, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 1, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "E"
struct MeshE : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d, e;
	int  ai, bi, ci, di, ei;
	// points for insert
	comms::cVec3  n, m, k;

	MeshE() :
		a( 215, 10, 0 ),
		b( 240, 40, 0 ),
		c( 260, 20, 0 ),
		d( 235, 10, 0 ),
		e( 260, 40, 0 ),
		n( cVec3::Zero ),
		m( cVec3::Zero ),
		k( cVec3::Zero )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		di = pos.Add( d );
		ei = pos.Add( e );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  4, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ei, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "F"
struct MeshF : public test::Mesh {
	// vertices
	comms::cVec3  a, b, c, d, e, f, g;
	int  ai, bi, ci, di, ei, fi, gi;

	MeshF() :
		a( -30, 25, 0 ),
		b( -30, 45, 0 ),
		c( -45, 30, 0 ),
		d( -15, 10, 0 ),
		e( -35, 10, 0 ),
		f( -25, 40, 0 ),
		g( -20, 45, 0 )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		di = pos.Add( d );
		ei = pos.Add( e );
		fi = pos.Add( f );
		gi = pos.Add( g );
		auto&  raw = mc->GetRaw();
		// abc
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		// adec
		raw.Add( cVec3i(  4, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( ei, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );
		// adfgb
		raw.Add( cVec3i(  5, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( fi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 7, pos.Count() );
		EXPECT_EQ( 3, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "G"
struct MeshG : public test::Mesh {
	// base vertices
	comms::cVec3  a, b, c;
	int  ai, bi, ci;
	// vertices for insert (as figure)
	comms::cVec3  d, e, f, g, h, k;

	MeshG() :
		a(  70, -45, 0 ),
		b(  80, -15, 0 ),
		c(  95, -45, 0 ),
		d(  60, -25, 0 ),
		e(  75, -30, 0 ),
		f(  95, -15, 0 ),
		g(  85, -25, 0 ),
		h( 100, -25, 0 ),
		k(  90, -35, 0 )
	{
		cList< cVec3 >&  pos = mc->GetPositions();
		ai = pos.Add( a );
		bi = pos.Add( b );
		ci = pos.Add( c );
		auto&  raw = mc->GetRaw();
		// abc
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( bi, 0, 0 ) );
		raw.Add( cVec3i( ci, 0, 0 ) );

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		EXPECT_EQ( 3, pos.Count() );
		EXPECT_EQ( 1, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "H"
struct MeshH : public test::Mesh {
	// vertices on the line
	const comms::cVec3  a, b, c, d, e, f, g;
	// vertices by both sides of the line
	const comms::cVec3  h, k, l, m, n;

	MeshH() :
		a( 120, -30, 0 ),
		b( 125, -30, 0 ),
		c( 130, -30, 0 ),
		d( 140, -30, 0 ),
		e( 145, -30, 0 ),
		f( 155, -30, 0 ),
		g( 160, -30, 0 ),
		h( 125, -20, 0 ),
		k( 140, -15, 0 ),
		l( 145, -25, 0 ),
		m( 135, -40, 0 ),
		n( 160, -40, 0 )
	{
		// empty mesh, build in the test
		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		cList< cVec3 >&  pos = mc->GetPositions();
		EXPECT_EQ( 0, pos.Count() );
		EXPECT_EQ( 0, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "K"
struct MeshK : public test::Mesh {
	// vertices on the line
	const comms::cVec3  a, b, c, d;
	// vertices by both sides of the line
	const comms::cVec3  e, f;

	MeshK() :
		a( 180, -30, 0 ),
		b( 185, -30, 0 ),
		c( 195, -30, 0 ),
		d( 210, -30, 0 ),
		e( 190, -20, 0 ),
		f( 195, -45, 0 )
	{
		// empty mesh, build in the test
		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		cList< cVec3 >&  pos = mc->GetPositions();
		EXPECT_EQ( 0, pos.Count() );
		EXPECT_EQ( 0, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "L"
struct MeshLEmpty : public test::Mesh {
	typedef comms::cVec< float, 3 >  vec_t;

	// vertices
	comms::cVec3  a, b, c, d, e, f;
	comms::cVec3  g, h, k, l, m, n, p, q, r, s;
	// length of sides
	float  de, fe, df, nb, mb;

	MeshLEmpty() :
		a( 235, -35, 0 ),
		b( 245, -20, 0 ),
		c( 250, -35, 0 ),
		d( 220, -45, 0 ),
		e( 245, -10, 0 ),
		f( 270, -45, 0 )
	{
		de = (d - e).Length();
		fe = (f - e).Length();
		df = (d - f).Length();
		g = vec_t( d, e,  6.5f / de );
		h = vec_t( d, e, 19.0f / de );
		k = vec_t( d, e, 25.5f / de );
		l = vec_t( f, e, 18.0f / fe );
		m = vec_t( d, f, 33.0f / df );
		mb = (m - b).Length();
		n = vec_t( d, f,  8.0f / df );
		nb = (n - b).Length();
		p = vec_t( n, b,  6.0f / nb );
		q = vec_t( n, b, 18.0f / nb );
		r = vec_t( m, b, 16.0f / mb );
		s = vec_t( d, f, 15.0f / df );

		// empty mesh, build in the test
		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		cList< cVec3 >&  pos = mc->GetPositions();
		EXPECT_EQ( 0, pos.Count() );
		EXPECT_EQ( 0, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "L"
struct MeshL : public MeshLEmpty {
	const figure_t  dgpn, ghqp, hkbq, keb, bel, blr, rlfm, scm, nas, sac, abc;

	MeshL() :
	    dgpn( mc->Insert( d, g, p, n ) ),
	    ghqp( mc->Insert( g, h, q, p ) ),
	    hkbq( mc->Insert( h, k, b, q ) ),
	    keb( mc->Insert( k, e, b ) ),
	    bel( mc->Insert( b, e, l ) ),
	    blr( mc->Insert( b, l, r ) ),
	    rlfm( mc->Insert( r, l, f, m ) ),
	    scm( mc->Insert( s, c, m ) ),
	    nas( mc->Insert( n, a, s ) ),
	    sac( mc->Insert( s, a, c ) ),
	    abc( mc->Insert( a, b, c ) )
	{
		//WeldMesh( mc, 1.0f );
		//mc->MergeFaces();

		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		cList< cVec3 >&  pos = mc->GetPositions();
		EXPECT_EQ( 16, pos.Count() );
		EXPECT_EQ( 11, mc->GetPolyCount() );
	}
};


// @see cMeshContainerTest.jpg, "M"
struct MeshM : public test::Mesh {
	const cVec3     a, b, c, d, e, f, g, h, i, j;
	const figure_t  abcd, dcef, cghe, ehij;
	const cVec3     k, l, m, n, o;
	int             ai, bi, ci, di, ei, fi, gi, hi, ii, ji;

	MeshM() :
		a( 10, -60, 0 ),
		b( 10, -80, 0 ),
		c( 20, -80, 0 ),
		d( 20, -60, 0 ),
		e( 30, -80, 0 ),
		f( 30, -60, 0 ),
		g( 20, -95, 0 ),
		h( 30, -95, 0 ),
		i( 40, -95, 0 ),
		j( 40, -80, 0 ),
		// dividers
		k( 10, -65, 0 ),
		l( 20, -70, 0 ),
		m( 25, -80, 0 ),
		n( 30, -87, 0 ),
		o( 40, -90, 0 ),
		// figures
	    abcd( mc->Insert( a, b, c, d,  &ai, &bi, &ci, &di ) ),
	    dcef( mc->Insert( d, c, e, f,  &di, &ci, &ei, &fi ) ),
	    cghe( mc->Insert( c, g, h, e,  &ci, &gi, &hi, &ei ) ),
	    ehij( mc->Insert( e, h, i, j,  &ei, &hi, &ii, &ji ) )
	{
		if ( !mc->IsValid( true ) ) {
			throw std::exception( "Mesh is invalid." );
		}
		cList< cVec3 >&  pos = mc->GetPositions();
		EXPECT_EQ( 10, pos.Count() );
		EXPECT_EQ( 4, mc->GetPolyCount() );
	}
};




// Meshes with error from Blender by import OBJ: "the same vertex of face
// used multiple times". Example from real OBJ-file:
//   f 557//557 608//608 624//624 625//625 626//626 593//593 626//626 624//624 557//557
struct MeshIncorrectRawA : public MeshLEmpty {
	int  di, gi, pi, ni;

	MeshIncorrectRawA() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgpnd
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  5, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
	}
};


struct MeshIncorrectRawB : public MeshLEmpty {
	int  di, gi, pi, ni;

	MeshIncorrectRawB() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// ddgpn
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  5, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};


struct MeshIncorrectRawC : public MeshLEmpty {
	int  di, gi, pi, ni;

	MeshIncorrectRawC() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgpnn
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  5, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};


struct MeshIncorrectRawD : public MeshLEmpty {
	int  di, gi, pi, ni;

	MeshIncorrectRawD() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgpppn
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  6, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};


struct MeshIncorrectRawE : public MeshLEmpty {
	int  di, gi, pi, ni;

	MeshIncorrectRawE() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgpgn
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  5, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};


struct MeshIncorrectRawF : public MeshLEmpty {
	int  di, gi, ni, pi;

	MeshIncorrectRawF() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgggpnnd
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  8, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
	}
};


struct MeshIncorrectRawG : public MeshLEmpty {
	int  ai, di, gi, ni, pi, si;

	MeshIncorrectRawG() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// dgggpnnd
		di = pos.Add( d );
		gi = pos.Add( g );
		pi = pos.Add( p );
		ni = pos.Add( n );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  8, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( pi, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( di, 0, 0 ) );
		// nnaasn
		ai = pos.Add( a );
		si = pos.Add( s );
		raw.Add( cVec3i(  6, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( ai, 0, 0 ) );
		raw.Add( cVec3i( si, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};
struct MeshIncorrectRawH : public MeshLEmpty {
	int  gi, ni;

	MeshIncorrectRawH() {
		cList< cVec3 >&  pos = mc->GetPositions();
		// ggg
		gi = pos.Add( g );
		auto&  raw = mc->GetRaw();
		raw.Add( cVec3i(  3, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		raw.Add( cVec3i( gi, 0, 0 ) );
		// nn
		ni = pos.Add( n );
		raw.Add( cVec3i(  2, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
		raw.Add( cVec3i( ni, 0, 0 ) );
	}
};


struct MeshWithEmptyFigure : public MeshLEmpty {
	figure_t  dgpn, nas, ef, bm, pointC, keb, blfmcr;

	MeshWithEmptyFigure() {
		auto&  raw = mc->GetRaw();
		// dgpn
		dgpn.Add( d );
		dgpn.Add( g );
		dgpn.Add( p );
		dgpn.Add( n );
		mc->Insert( dgpn );
		// -
		raw.Add( cVec3i( 0, 0, 0 ) );
		// nas
		nas.Add( n );
		nas.Add( a );
		nas.Add( s );
		mc->Insert( nas );
		// ef
		ef.Add( e );
		ef.Add( f );
		mc->Insert( ef );
		// bm
		bm.Add( b );
		bm.Add( m );
		mc->Insert( bm );
		// c
		pointC.Add( c );
		mc->Insert( pointC );
		// -
		raw.Add( cVec3i( 0, 0, 0 ) );
		// keb
		keb.Add( k );
		keb.Add( e );
		keb.Add( b );
		mc->Insert( keb );
		// blfmcr
		blfmcr.Add( b );
		blfmcr.Add( l );
		blfmcr.Add( f );
		blfmcr.Add( m );
		blfmcr.Add( c );
		blfmcr.Add( r );
		mc->Insert( blfmcr );

		EXPECT_EQ( 9 /* with empty blocks */, mc->GetPolyCount() );
		EXPECT_TRUE(  mc->Contains( dgpn ) );
		EXPECT_TRUE(  mc->Contains( nas ) );
		EXPECT_TRUE(  mc->Contains( ef ) );
		EXPECT_TRUE(  mc->Contains( bm ) );
		EXPECT_TRUE(  mc->Contains( c ) );
		EXPECT_TRUE(  mc->Contains( keb ) );
		EXPECT_TRUE(  mc->Contains( blfmcr ) );
		EXPECT_TRUE(  mc->IsValidRawSequence() );
	}
};


struct MeshWithConfluentFigure : public MeshL {
	figure_t  dgh, dhke, bqa, fsd, na, pointL;

	MeshWithConfluentFigure() {
		dgh  = mc->Insert( d, g, h );
		dhke = mc->Insert( d, h, k, e );
		bqa  = mc->Insert( b, q, a );
		fsd  = mc->Insert( f, s, d );
		na   = mc->Insert( n, a );
		pointL = mc->Insert( l );

		EXPECT_TRUE(  mc->Contains( dgh ) );
		EXPECT_TRUE(  mc->Contains( dhke ) );
		EXPECT_TRUE(  mc->Contains( bqa ) );
		EXPECT_TRUE(  mc->Contains( fsd ) );
		EXPECT_TRUE(  mc->Contains( na ) );
		EXPECT_TRUE(  mc->Contains( pointL ) );
	}
};




class cMeshContainerTest :
	public ::testing::Test {

protected:
	typedef Mesh::figure_t   figure_t;
	typedef Mesh::figureI_t  figureI_t;

	cMeshContainerTest() {}
	virtual ~cMeshContainerTest() {}

	virtual void SetUp() {}
	virtual void TearDown() {}

	std::string GetName() const {
		const ::testing::TestInfo* const  ti =
			::testing::UnitTest::GetInstance()->current_test_info();
		return (std::string)ti->test_case_name() + "_" + ti->name();
	}
};




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, GetNeighboursForFaceL ) {

	const MeshL  mesh;
	std::cout << "  all mesh\n" << *mesh.mc << std::endl;

	static const float  P = 0.0001f;

	cVec3  normal;
	// keb
	{
		figure_t  figure;
		figure.Add( mesh.k );
		figure.Add( mesh.e );
		figure.Add( mesh.b );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		const cList< int >  r = mesh.mc->GetNeighboursForFace( ftr, &normal );
		EXPECT_NEAR( 0.0f, normal[ 0 ], P );
		EXPECT_NEAR( 0.0f, normal[ 1 ], P );
		EXPECT_NEAR( 1.0f, normal[ 2 ], P );
		EXPECT_EQ( 4 + 1, r.Count() );
	}
	/* - @todo abc
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.b );
		figure.Add( mesh.c );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		const cList< int >  r = mesh.mc->GetNeighboursForFace( ftr, &normal );
		EXPECT_NEAR( 0.0f, normal[ 0 ], P );
		EXPECT_NEAR( 0.0f, normal[ 1 ], P );
		EXPECT_NEAR( 1.0f, normal[ 2 ], P );
		EXPECT_EQ( 10, r.Count() );
	}
	// dgpn
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.g );
		figure.Add( mesh.p );
		figure.Add( mesh.n );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		const cList< int >  r = mesh.mc->GetNeighboursForFace( ftr, &normal );
		EXPECT_NEAR( 0.0f, normal[ 0 ], P );
		EXPECT_NEAR( 0.0f, normal[ 1 ], P );
		EXPECT_NEAR( 1.0f, normal[ 2 ], P );
		EXPECT_EQ( 2, r.Count() );
	}
	// ghqp
	{
		figure_t  figure;
		figure.Add( mesh.g );
		figure.Add( mesh.h );
		figure.Add( mesh.q );
		figure.Add( mesh.p );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		const cList< int >  r = mesh.mc->GetNeighboursForFace( ftr, &normal );
		EXPECT_NEAR( 0.0f, normal[ 0 ], P );
		EXPECT_NEAR( 0.0f, normal[ 1 ], P );
		EXPECT_NEAR( 1.0f, normal[ 2 ], P );
		EXPECT_EQ( 5, r.Count() );
	}
	*/
}




TEST_F( cMeshContainerTest, GetPosition ) {

	MeshConeSide5  tm;

	EXPECT_EQ( tm.a,  tm.mc->GetPosition( 1 - 1 ) );
	EXPECT_EQ( tm.b,  tm.mc->GetPosition( 2 - 1 ) );
	EXPECT_EQ( tm.f,  tm.mc->GetPosition( 6 - 1 ) );

	EXPECT_THROW( tm.mc->GetPosition( -1 ),  comms::Exception );
	EXPECT_THROW( tm.mc->GetPosition(  6 ),  comms::Exception );
}




TEST_F( cMeshContainerTest, GetVertex ) {

	MeshConeSide5  tm;

	EXPECT_EQ( 1 - 1,  tm.mc->GetVertex( tm.a ) );
	EXPECT_EQ( 2 - 1,  tm.mc->GetVertex( tm.b ) );
	EXPECT_EQ( 6 - 1,  tm.mc->GetVertex( tm.f ) );

	EXPECT_EQ( -1,  tm.mc->GetVertex( tm.absentA ) );
}




// @see cMeshContainerTest.jpg, "A"
TEST_F( cMeshContainerTest, InsertA_EandF ) {

	MeshA  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();
	const auto&  raw = mesh.mc->GetRaw();

	std::cout << "before\n" << *mesh.mc << std::endl;
	// a -- +e+ -- b
	{
		const float t = 10.0f / 35.0f;
		const int  vi = mesh.mc->Insert( mesh.ai, mesh.bi, t );
		std::cout << "after Insert()\n" << *mesh.mc << std::endl;
		ASSERT_NE( -1, vi ) << "Point don't inserted.";
		EXPECT_EQ( mesh.e, mesh.mc->GetPosition( vi ) );

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 5, pos.Count() ) << *mesh.mc;
		EXPECT_EQ( 9, raw.Count() ) << *mesh.mc;
		EXPECT_EQ( 3, mesh.mc->GetTrisCount() ) << "Expected after triangulate.";
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.b ) );
	}

	// a -- e -- +f+ -- b
	{
		const int  ei =  mesh.mc->GetVertex( mesh.e );
		ASSERT_NE( -1, ei );
		const int  vi = mesh.mc->Insert( ei, mesh.bi, mesh.f );
		//std::cout << "after\n" << *mesh.mc << std::endl;
		ASSERT_NE( -1, vi ) << "Point don't inserted.";
		EXPECT_EQ( mesh.f, mesh.mc->GetPosition( vi ) );

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 6, pos.Count() ) << *mesh.mc;
		EXPECT_EQ( 10, raw.Count() ) << *mesh.mc;
		EXPECT_EQ( 4, mesh.mc->GetTrisCount() ) << "Expected after triangulate.";
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.e, mesh.b ) );
	}
}




// @see cMeshContainerTest.jpg, "A"
TEST_F( cMeshContainerTest, InsertA_G ) {

	MeshA  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// a -- +g+ -- c
	{
		const float t = 25.0f / 32.0f;
		const int  vi = mesh.mc->Insert( mesh.ai, mesh.ci, t );
		ASSERT_NE( -1, vi ) << "Point don't inserted.";
		EXPECT_EQ( mesh.g, mesh.mc->GetPosition( vi ) );

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
	}
}




// @see cMeshContainerTest.jpg, "A"
TEST_F( cMeshContainerTest, InsertA_GInverted ) {

	MeshA  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// c -- +g+ -- a
	{
		const float t = 1.0f - 25.0f / 32.0f;
		const int  vi = mesh.mc->Insert( mesh.ci, mesh.ai, t );
		ASSERT_NE( -1, vi ) << "Point don't inserted.";
		EXPECT_EQ( mesh.g, mesh.mc->GetPosition( vi ) );

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
	}
}




// @see cMeshContainerTest.jpg, "A"
TEST_F( cMeshContainerTest, InsertA_H ) {

	MeshA  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// a -- +h+ -- d
	{
		const float t = 37.0f / 48.0f;
		const int  vi = mesh.mc->Insert( mesh.ai, mesh.di, t );
		ASSERT_EQ( -1, vi ) << "Point inserted into the unpresent edge.";
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.h ) );
	}
}




// @see cMeshContainerTest.jpg, "A"
TEST_F( cMeshContainerTest, InsertA_X ) {

	MeshA  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// b -- +x+ -- c
	{
		const float tbc = 17.0f / 22.0f;
		//const float tcb = 1.0f - tbc;
		// insert into the triangles 'abc' and 'dbc'
		const int  vi = mesh.mc->Insert( mesh.bi, mesh.ci, tbc );
		ASSERT_NE( -1, vi ) << "Point don't inserted.";

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		const cVec3  x = mesh.mc->GetPosition( vi );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, x ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, x ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.c ) );
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, InsertF ) {

	Mesh  mesh;
	const MeshF  mf;
	mesh.mc->GetPositions() = mf.mc->GetPositions();
	// abc
	{
		figureI_t  figure;
		figure.Add( mf.ai );
		figure.Add( mf.bi );
		figure.Add( mf.ci );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// aced
	{
		figureI_t  figure;
		figure.Add( mf.ai );
		figure.Add( mf.ci );
		figure.Add( mf.ei );
		figure.Add( mf.di );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// adfgb
	{
		figureI_t  figure;
		figure.Add( mf.ai );
		figure.Add( mf.di );
		figure.Add( mf.fi );
		figure.Add( mf.gi );
		figure.Add( mf.bi );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// empty figure
	{
		figureI_t  figure;
		EXPECT_TRUE( figure.IsEmpty() );
		EXPECT_THROW( mesh.mc->Insert( figure ),  comms::Exception );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// figure with undefined vertex
	{
		const int  vi = mesh.mc->GetPositions().Count() + 1000;
		figureI_t  figure;
		figure.Add( mf.bi );
		figure.Add( vi );
		figure.Add( mf.ci );
		EXPECT_THROW( mesh.mc->Insert( figure ),  comms::Exception );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// figure with incorrect vertex
	{
		const int  vi = -1000;
		figureI_t  figure;
		figure.Add( mf.bi );
		figure.Add( vi );
		figure.Add( mf.ci );
		EXPECT_THROW( mesh.mc->Insert( figure ),  comms::Exception );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// add dublicate 'abc'
	{
		figureI_t  figure;
		figure.Add( mf.ai );
		figure.Add( mf.bi );
		figure.Add( mf.ci );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 4, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
}




// @see cMeshContainerTest.jpg, "G"
// @see InsertG_321()
TEST_F( cMeshContainerTest, InsertG_123 ) {

	const MeshG  mesh;
	// (1) adbe
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.d );
		figure.Add( mesh.b );
		figure.Add( mesh.e );
		EXPECT_TRUE( mesh.mc->Contains( mesh.a, mesh.b ) );
		mesh.mc->Insert( figure );
		std::cout << "  'adbe' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
		// before MergeFaces() 'e' must be into the 'adb' only
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.e ) );
		// after MergeFaces() 'e' must be into the 'adb' & 'abc'
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'adbe' after\n" << *mesh.mc << std::endl;
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.e ) );
	}
	// (2) bfg
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.f );
		figure.Add( mesh.g );
		EXPECT_TRUE( mesh.mc->Contains( mesh.c, mesh.b ) );
		mesh.mc->Insert( figure );
		std::cout << "  'bfg' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
		// before MergeFaces() 'g' must be into the 'bfg' only
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.g, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.g ) );
		// after MergeFaces() 'g' must be into the 'bfg' & 'bc'
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'bfg' after \n" << *mesh.mc << std::endl;
		EXPECT_FALSE( mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.g ) );
	}
	// (3) gfhk
	{
		figure_t  figure;
		figure.Add( mesh.g );
		figure.Add( mesh.f );
		figure.Add( mesh.h );
		figure.Add( mesh.k );
		EXPECT_TRUE( mesh.mc->Contains( mesh.g, mesh.c ) );
		mesh.mc->Insert( figure );
		std::cout << "  'gfhk' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 4, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
		// before MergeFaces() 'k' must be into the 'gfhk' only
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.k, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.h ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.k ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.d ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.e ) );
		// after MergeFaces() 'k' must be into the 'gfhk' & 'gc'
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'gfhk' after\n" << *mesh.mc << std::endl;
		EXPECT_FALSE( mesh.mc->Contains( mesh.c, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.k, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.h ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.k ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.d ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.e ) );
	}
}




// @see cMeshContainerTest.jpg, "G"
// @see InsertG_123()
TEST_F( cMeshContainerTest, InsertG_321 ) {

	const MeshG  mesh;
	// (3) gfhk
	{
		figure_t  figure;
		figure.Add( mesh.g );
		figure.Add( mesh.f );
		figure.Add( mesh.h );
		figure.Add( mesh.k );
		EXPECT_TRUE( mesh.mc->Contains( mesh.b, mesh.c ) );
		mesh.mc->Insert( figure );
		std::cout << "  'gfhk' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'gfhk' after\n" << *mesh.mc << std::endl;
		// 'k' must be into the 'gfhk' & 'gc'
		EXPECT_FALSE( mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.k, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.g, mesh.c ) );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.h ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.k ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.d ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.h, mesh.e ) );
	}
	// (2) bfg
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.f );
		figure.Add( mesh.g );
		EXPECT_TRUE( mesh.mc->Contains( mesh.g, mesh.b ) );
		mesh.mc->Insert( figure );
		std::cout << "  'bfg' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'bfg' after\n" << *mesh.mc << std::endl;
		// 'g' must be into the 'bfg' & 'bc'
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.g ) );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
	}
	// (1) adbe
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.d );
		figure.Add( mesh.b );
		figure.Add( mesh.e );
		EXPECT_TRUE( mesh.mc->Contains( mesh.a, mesh.b ) );
		mesh.mc->Insert( figure );
		std::cout << "  'abde' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'abde' after\n" << *mesh.mc << std::endl;
		// 'e' must be into the 'abde' & 'bc'
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.b ) );
		EXPECT_EQ( 4, mesh.mc->GetPolyCount() );
		EXPECT_TRUE(  mesh.mc->IsValid( true ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.e ) );
	}
}




// @see cMeshContainerTest.jpg, "H"
// @see Simplest InsertK_12()
TEST_F( cMeshContainerTest, InsertH_12345 ) {

	const MeshH  mesh;
	// (1) ach
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.c );
		figure.Add( mesh.h );
		mesh.mc->Insert( figure );
		std::cout << "  'ach' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// (2) bme
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.m );
		figure.Add( mesh.e );
		mesh.mc->Insert( figure );
		std::cout << "  'bme' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'bme' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.e ) );
		// 1
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.c ) );
		// 2
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.e ) );
	}
	// (3) cdk
	{
		figure_t  figure;
		figure.Add( mesh.c );
		figure.Add( mesh.d );
		figure.Add( mesh.k );
		mesh.mc->Insert( figure );
		std::cout << "  'cdk' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'cdk' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.e ) );
		// 1
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.c ) );
		// 2
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.e ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.d ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.c, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.e ) );
		// 3
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.d ) );
	}
	// (4) dfl
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.f );
		figure.Add( mesh.l );
		mesh.mc->Insert( figure );
		std::cout << "  'dfl' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'dfl' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 4, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.f ) );
		// 4
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.f ) );
	}
	// (5) gnmef
	{
		figure_t  figure;
		figure.Add( mesh.g );
		figure.Add( mesh.n );
		figure.Add( mesh.m );
		figure.Add( mesh.e );
		// included node on the edge 'eg'
		figure.Add( mesh.f );
		mesh.mc->Insert( figure );
		std::cout << "  'gnmef' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'gnmef' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 5, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.g ) );
		// 4
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.f ) );
		// 5
		EXPECT_FALSE( mesh.mc->Contains( mesh.e, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.m ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.n, mesh.e ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.n, mesh.f ) );
	}

	comms::cMeshIO::SaveMesh( *mesh.mc, "Bevel.InsertH_12345.auto.obj" );
}




// @see cMeshContainerTest.jpg, "H"
TEST_F( cMeshContainerTest, InsertH_1342 ) {

	const MeshH  mesh;
	// (1) ach
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.c );
		figure.Add( mesh.h );
		mesh.mc->Insert( figure );
		std::cout << "  'ach' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// (3) cdk
	{
		figure_t  figure;
		figure.Add( mesh.c );
		figure.Add( mesh.d );
		figure.Add( mesh.k );
		mesh.mc->Insert( figure );
		std::cout << "  'cdk' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'cdk' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		// 3
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.d ) );
	}
	// (4) dfl
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.f );
		figure.Add( mesh.l );
		mesh.mc->Insert( figure );
		std::cout << "  'dfl' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'dfl' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.f ) );
		// 4
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.f ) );
	}
	// (2) bme
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.m );
		figure.Add( mesh.e );
		mesh.mc->Insert( figure );
		std::cout << "  'bme' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'bme' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 4, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.f ) );
		// 1
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.c ) );
		// 2
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.e ) );
		// 3
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.d ) );
		// 4
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.f ) );
		// incorrect added
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.d ) );
	}

	//WeldMesh( mesh.mc, 1.0f );
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.InsertH_1342.auto.obj" );
}




// @see cMeshContainerTest.jpg, "K"
// @see Complex InsertH_12345()
TEST_F( cMeshContainerTest, InsertK_12 ) {

	const MeshK  mesh;
	// (1) ace
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.c );
		figure.Add( mesh.e );
		mesh.mc->Insert( figure );
		std::cout << "  'ace' before\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// (2) bfd
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.f );
		figure.Add( mesh.d );
		mesh.mc->Insert( figure );
		std::cout << "  'bfd' before\n" << *mesh.mc << std::endl;
		mesh.mc->MergeFaces( 0.01f );
		std::cout << "  'bfd' after\n" << *mesh.mc << std::endl;
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// line
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.d ) );
		// 1
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.c ) );
		// 2
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.d ) );
	}
}




// @see cMeshContainerTest.jpg, "L"
// same directions, 2 triangles
TEST_F( cMeshContainerTest, InsertL_nbm_abc ) {

	const MeshLEmpty  mesh;
	std::cout << "  'nbm' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.n );
		figure.Add( mesh.b );
		figure.Add( mesh.m );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	std::cout << "  'abc' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.b );
		figure.Add( mesh.c );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}

	mesh.mc->MergeFaces( 0.2f );
	std::cout << "  mesh after merge faces\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	{
		// mbn
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.m ) );
		// abc
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.a ) );
	}
}




// @see cMeshContainerTest.jpg, "L"
// different directions, 2 triangles
TEST_F( cMeshContainerTest, InsertL_mbn_abc ) {

	const MeshLEmpty  mesh;
	std::cout << "  'mbn' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.m );
		figure.Add( mesh.b );
		figure.Add( mesh.n );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	std::cout << "  'abc' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.b );
		figure.Add( mesh.c );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}

	mesh.mc->MergeFaces( 0.1f );
	std::cout << "  mesh after merge faces\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	{
		// mbn
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.m ) );
		// abc
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.a ) );
	}
}




// @see cMeshContainerTest.jpg, "L"
// different directions, 3 triangles
TEST_F( cMeshContainerTest, InsertL_def_mbn_abc ) {

	const MeshLEmpty  mesh;
	std::cout << "  'def' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.e );
		figure.Add( mesh.f );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	std::cout << "  'mbn' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.m );
		figure.Add( mesh.b );
		figure.Add( mesh.n );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	std::cout << "  'abc' before\n" << *mesh.mc << std::endl;
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.b );
		figure.Add( mesh.c );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}

	mesh.mc->MergeFaces( 0.1f );
	std::cout << "  mesh after merge faces\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	{
		// def
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.f, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.m ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.d ) );
		// mbn
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.m ) );
		// abc
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.a ) );
	}
}




// @see cMeshContainerTest.jpg, "L"
// different directions, all figures
TEST_F( cMeshContainerTest, InsertL ) {

	const MeshLEmpty  mesh;
	std::cout << "  '(green) nbm'\n";
	{
		figure_t  figure;
		figure.Add( mesh.n );
		figure.Add( mesh.b );
		figure.Add( mesh.m );
		mesh.mc->Insert( figure );
		EXPECT_EQ( 1, mesh.mc->GetPolyCount() );
	}
	std::cout << "  '(orange) def'\n";
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.e );
		figure.Add( mesh.f );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(1) npgd'\n";
	{
		figure_t  figure;
		figure.Add( mesh.n );
		figure.Add( mesh.p );
		figure.Add( mesh.g );
		figure.Add( mesh.d );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(2) pghq'\n";
	{
		figure_t  figure;
		figure.Add( mesh.p );
		figure.Add( mesh.g );
		figure.Add( mesh.h );
		figure.Add( mesh.q );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(3) qhkb'\n";
	{
		figure_t  figure;
		figure.Add( mesh.q );
		figure.Add( mesh.h );
		figure.Add( mesh.k );
		figure.Add( mesh.b );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(4) bke'\n";
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.k );
		figure.Add( mesh.e );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(5) ble'\n";
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.l );
		figure.Add( mesh.e );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(6) blr'\n";
	{
		figure_t  figure;
		figure.Add( mesh.b );
		figure.Add( mesh.l );
		figure.Add( mesh.r );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(7) rmfl'\n";
	{
		figure_t  figure;
		figure.Add( mesh.r );
		figure.Add( mesh.m );
		figure.Add( mesh.f );
		figure.Add( mesh.l );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(8) smc'\n";
	{
		figure_t  figure;
		figure.Add( mesh.s );
		figure.Add( mesh.m );
		figure.Add( mesh.c );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(9) sna'\n";
	{
		figure_t  figure;
		figure.Add( mesh.s );
		figure.Add( mesh.n );
		figure.Add( mesh.a );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(10) sca'\n";
	{
		figure_t  figure;
		figure.Add( mesh.s );
		figure.Add( mesh.c );
		figure.Add( mesh.a );
		mesh.mc->Insert( figure );
	}
	std::cout << "  '(11) abc'\n";
	{
		figure_t  figure;
		figure.Add( mesh.a );
		figure.Add( mesh.b );
		figure.Add( mesh.c );
		mesh.mc->Insert( figure );
	}

	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	std::cout << "  all before merge\n" << *mesh.mc << std::endl;

	mesh.mc->MergeFaces( 0.1f );
	std::cout << "  all after merge\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	{
		// def
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.e ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.e, mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.f, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d, mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.h ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.k ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.e ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.l ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.f ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.f, mesh.m ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.m, mesh.s ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.s, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.d ) );
		// nbm
		EXPECT_FALSE( mesh.mc->Contains( mesh.n, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.m ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.m, mesh.n ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.n, mesh.p ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.p, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.q ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.q, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b, mesh.r ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.r, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.m ) );
		// abc
		EXPECT_FALSE( mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.b, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.c ) );
		// edges from orange to green
		EXPECT_TRUE(  mesh.mc->Contains( mesh.g, mesh.p ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.h, mesh.q ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.k, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.e, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.l, mesh.r ) );
		// edges into the green triangle
		EXPECT_TRUE(  mesh.mc->Contains( mesh.s, mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c, mesh.s ) );
	}

	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.InsertL.auto.obj" );
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, IsClearConfluent ) {

	MeshWithConfluentFigure  mesh;

	static const float  tolerance = 0.1f;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.dgh,  tolerance ) )
		<< "dgh " << mesh.dgh;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.dhke, tolerance ) )
		<< "dhke " <<  mesh.dhke;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.bqa,  tolerance ) )
		<< "bqa " <<  mesh.bqa;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.fsd,  tolerance ) )
		<< "fsd " <<  mesh.fsd;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.na,   tolerance ) )
		<< "na " <<  mesh.na;
	EXPECT_TRUE( mesh.mc->IsClearConfluent( mesh.pointL, tolerance ) )
		<< "pointL " <<  mesh.pointL;

	EXPECT_FALSE( mesh.mc->IsClearConfluent( mesh.dgpn, tolerance ) )
		<< "dgpn " <<  mesh.dgpn;
	EXPECT_FALSE( mesh.mc->IsClearConfluent( mesh.nas,  tolerance ) )
		<< "nas " <<  mesh.nas;
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, IsClockwiseOrderL ) {

	const MeshL  mesh;
	std::cout << "  all mesh\n" << *mesh.mc << std::endl;

	// nas
	figure_t  figureNAS;
	figureNAS.Add( mesh.n );
	figureNAS.Add( mesh.a );
	figureNAS.Add( mesh.s );
	const int  ftrNAS = mesh.mc->Find( figureNAS );
	const cVec3  normal = mesh.mc->GetFaceNormal( ftrNAS );
	const cVec3  observer = normal * cVec3( 1, 1, 100 );
	{
		ASSERT_NE( -1, ftrNAS );
		EXPECT_TRUE( mesh.mc->IsClockwiseOrder( ftrNAS, observer ) );
	}
	mesh.mc->Invert( ftrNAS );
	// -san-
	{
		figure_t  figureSAN;
		figureSAN.Add( mesh.s );
		figureSAN.Add( mesh.a );
		figureSAN.Add( mesh.n );
		const int  ftrSAN = mesh.mc->Find( figureSAN );
		ASSERT_NE( -1, ftrSAN );
		EXPECT_FALSE( mesh.mc->IsClockwiseOrder( ftrSAN, observer ) );
	}
	// dgpn
	{
		figure_t  figure;
		figure.Add( mesh.d );
		figure.Add( mesh.g );
		figure.Add( mesh.p );
		figure.Add( mesh.n );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		EXPECT_TRUE( mesh.mc->IsClockwiseOrder( ftr, observer ) );
	}
	// incorrect 'beginRawFigure'
	{
		const int  ftr = -1;
		EXPECT_THROW( mesh.mc->IsClockwiseOrder( ftr, observer ), comms::Exception );
	}
	{
		const int  ftr = mesh.mc->GetRaw().Count();
		EXPECT_THROW( mesh.mc->IsClockwiseOrder( ftr, observer ), comms::Exception );
	}
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, InvertL ) {

	const MeshL  mesh;
	std::cout << "  all mesh\n" << *mesh.mc << std::endl;

	// nas <--> san
	{
		figure_t  figure;
		{
			figure.Add( mesh.n );
			figure.Add( mesh.a );
			figure.Add( mesh.s );
		}
		figure_t  invertFigure;
		{
			invertFigure.Add( mesh.s );
			invertFigure.Add( mesh.a );
			invertFigure.Add( mesh.n );
		}
		EXPECT_TRUE( mesh.mc->Contains( figure ) );
		EXPECT_FALSE( mesh.mc->Contains( invertFigure ) );
		const int  ftr = mesh.mc->Find( figure );
		ASSERT_NE( -1, ftr );
		mesh.mc->Invert( ftr );
		EXPECT_FALSE( mesh.mc->Contains( figure ) );
		EXPECT_TRUE( mesh.mc->Contains( invertFigure ) );
	}
}




TEST_F( cMeshContainerTest, IsValidRawSequence ) {
	{
		const Mesh  mesh;
		EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
	}
	{
		const MeshF  mesh;
		EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
	}
	{
		const MeshL  mesh;
		EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
	}
	{
		const MeshIncorrectRawA  mesh;
		EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	}
	{
		const MeshIncorrectRawE  mesh;
		EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	}
}




TEST_F( cMeshContainerTest, CorrectRawSequenceA ) {

	const MeshIncorrectRawA  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceB ) {

	const MeshIncorrectRawB  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceC ) {

	const MeshIncorrectRawC  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceD ) {

	const MeshIncorrectRawD  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceE ) {

	const MeshIncorrectRawE  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	EXPECT_EQ( 6, mesh.mc->GetRaw().Count() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
	EXPECT_EQ( 5, mesh.mc->GetRaw().Count() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceF ) {

	const MeshIncorrectRawF  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceG ) {

	const MeshIncorrectRawG  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, CorrectRawSequenceH ) {

	const MeshIncorrectRawH  mesh;
	EXPECT_FALSE( mesh.mc->IsValidRawSequence() );
	EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	std::cout << "  before\n" << *mesh.mc << std::endl;
	mesh.mc->CorrectRawSequence();
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValidRawSequence() );
	EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	EXPECT_TRUE( mesh.mc->Contains( mesh.g ) );
	EXPECT_TRUE( mesh.mc->Contains( mesh.n ) );
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, CorrectFacesL ) {

	const MeshL  mesh;
	std::cout << "  all before merge and invert and correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.before.Merge.Invert.CorrectFacesL.auto.obj" );

	mesh.mc->MergeFaces( 0.01f );
	std::cout << "  all before invert and correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.before.Invert.CorrectFacesL.auto.obj" );

	figure_t  figureSAC;
	{
		figureSAC.Add( mesh.s );
		figureSAC.Add( mesh.a );
		figureSAC.Add( mesh.c );
		ASSERT_TRUE( mesh.mc->Contains( figureSAC ) );
	}
	const int  ftrSAC = mesh.mc->Find( figureSAC );
	ASSERT_NE( -1, ftrSAC );
	mesh.mc->Invert( ftrSAC );
	ASSERT_FALSE( mesh.mc->Contains( figureSAC ) );
	figure_t  figureCAS;
	{
		figureCAS.Add( mesh.c );
		figureCAS.Add( mesh.a );
		figureCAS.Add( mesh.s );
		ASSERT_TRUE( mesh.mc->Contains( figureCAS ) );
	}
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	std::cout << "  all before correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.before.CorrectFacesL.auto.obj" );

	mesh.mc->CorrectFaces( 0 );
	EXPECT_TRUE( mesh.mc->IsValid( true ) );
	std::cout << "  all after correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerTest.after.CorrectFacesL.auto.obj" );
	{
		EXPECT_TRUE(  mesh.mc->Contains( figureSAC ) );
		EXPECT_FALSE( mesh.mc->Contains( figureCAS ) );
	}
}




// @see cMeshContainerTest.jpg, "B"
TEST_F( cMeshContainerTest, DivideB ) {

	MeshB  mesh;
	const cList< cVec3  >&  pos = mesh.mc->GetPositions();

	// m--n
	{
		std::cout << "  before\n" << *mesh.mc << "." << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		// abc
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.ai ) );
		}
		const float tm = 20.0f / 30.0f;
		const float tn = 20.0f / 25.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ai, mesh.bi, tm,
			mesh.ai, mesh.ci, tn
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << "." << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << "." << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'm' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		const int  mi = mesh.mc->GetVertex( vi.first );
		const int  ni = mesh.mc->GetVertex( vi.second );
		EXPECT_NE( -1, mi );
		EXPECT_NE( -1, ni );
		EXPECT_FALSE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.ai, mesh.ci ) );
		// amn
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai,      mi ) );
			EXPECT_TRUE( mesh.mc->Contains(      mi,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni, mesh.ai ) );
		}
		// mbcn
		{
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni,      mi ) );
		}
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "C"
TEST_F( cMeshContainerTest, DivideC ) {

	MeshC  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// m--n
	int mi, ni;
	{
		std::cout << "  before 'm--n'\n" << *mesh.mc << std::endl;
		// abcd
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		}
		const float tm = 20.0f / 35.0f;
		const float tn = 11.0f / 16.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ai, mesh.di, tm,
			mesh.bi, mesh.ci, tn
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after 'm--n'\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'm' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		mi = mesh.mc->GetVertex( vi.first );
		ni = mesh.mc->GetVertex( vi.second );
		EXPECT_NE( -1, mi );
		EXPECT_NE( -1, ni );
		EXPECT_FALSE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
		// abnm
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni,      mi ) );
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.ai ) );
		}
		// mdcn
		{
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni,      mi ) );
		}
		EXPECT_EQ( 6, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}

	// k--l
	// #! Divide the figure 'abnm'
	{
		const float tk =  6.0f / 27.0f;
		const float tl = 10.0f / 20.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ai, mesh.bi, tk,
			mesh.ai, mi,      tl
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after 'm--n'\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'm' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		const int  ki = mesh.mc->GetVertex( vi.first );
		const int  li = mesh.mc->GetVertex( vi.second );
		EXPECT_NE( -1, ki );
		EXPECT_NE( -1, li );
		EXPECT_FALSE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.ai,      mi ) );
		// akl
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai,      ki ) );
			EXPECT_TRUE( mesh.mc->Contains(      ki,      li ) );
			EXPECT_TRUE( mesh.mc->Contains(      li, mesh.ai ) );
		}
		// lmnbk
		{
			EXPECT_TRUE( mesh.mc->Contains(      li,      mi ) );
			EXPECT_TRUE( mesh.mc->Contains(      mi,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi,      ki ) );
			EXPECT_TRUE( mesh.mc->Contains(      ki,      li ) );
		}
		// mdcn
		{
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni,      mi ) );
		}
		EXPECT_EQ( 8, pos.Count() );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "D"
TEST_F( cMeshContainerTest, DivideD_MN ) {

	MeshD  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// m--n
	int mi, ni;
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		// abcd
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		}
		const float tm = 10.0f / 35.0f;
		const float tn =  0.0f / 16.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ai, mesh.di, tm,
			mesh.bi, mesh.ci, tn
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'm' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		mi = mesh.mc->GetVertex( vi.first );
		ni = mesh.mc->GetVertex( vi.second );
		EXPECT_NE(      -1, mi );
		EXPECT_EQ( mesh.bi, ni );
		EXPECT_FALSE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		// abm
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi,      mi ) );
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.ai ) );
		}
		// mncd
		{
			EXPECT_TRUE( mesh.mc->Contains(      mi,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di,      mi ) );
		}
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "D"
TEST_F( cMeshContainerTest, DivideD_NM ) {

	MeshD  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// n--m
	int mi, ni;
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		// abcd
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		}
		const float tm = 10.0f / 35.0f;
		const float tn =  0.0f / 16.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.bi, mesh.ci, tn,
			mesh.ai, mesh.di, tm
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'n' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'm' don't inserted.";
		ni = mesh.mc->GetVertex( vi.first );
		mi = mesh.mc->GetVertex( vi.second );
		EXPECT_EQ( mesh.bi, ni );
		EXPECT_NE(      -1, mi );
		EXPECT_FALSE( mesh.mc->Contains( mesh.di, mesh.ai ) );
		// abm
		{
			EXPECT_TRUE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.bi,      mi ) );
			EXPECT_TRUE( mesh.mc->Contains(      mi, mesh.ai ) );
		}
		// mncd
		{
			EXPECT_TRUE( mesh.mc->Contains(      mi,      ni ) );
			EXPECT_TRUE( mesh.mc->Contains(      ni, mesh.ci ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.ci, mesh.di ) );
			EXPECT_TRUE( mesh.mc->Contains( mesh.di,      mi ) );
		}
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "D"
TEST_F( cMeshContainerTest, DivideD_NK ) {

	MeshD  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// n--k
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		const float tn =  0.0f / 16.0f;
		const float tk = 22.0f / 22.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.bi, mesh.ci, tn,
			mesh.ci, mesh.di, tk
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'n' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'k' don't inserted.";
		EXPECT_EQ( mesh.bi, mesh.mc->GetVertex( vi.first  ) );
		EXPECT_EQ( mesh.di, mesh.mc->GetVertex( vi.second ) );
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "D"
TEST_F( cMeshContainerTest, DivideD_KN ) {

	MeshD  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// k--n
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		const float tn =  0.0f / 16.0f;
		const float tk = 22.0f / 22.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ci, mesh.di, tk,
			mesh.bi, mesh.ci, tn
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'k' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		EXPECT_EQ( mesh.di, mesh.mc->GetVertex( vi.first ) );
		EXPECT_EQ( mesh.bi, mesh.mc->GetVertex( vi.second  ) );
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "D"
TEST_F( cMeshContainerTest, DivideD_MNplusNK ) {

	MeshD  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// m--n
	int mi, ni;
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		const float tm = 10.0f / 35.0f;
		const float tn =  0.0f / 16.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.ai, mesh.di, tm,
			mesh.bi, mesh.ci, tn
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		mi = mesh.mc->GetVertex( vi.first );
		ni = mesh.mc->GetVertex( vi.second );
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'm' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'n' don't inserted.";
		EXPECT_NE(      -1, mi );
		EXPECT_EQ( mesh.bi, ni );
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}

	// n--k
	{
		std::cout << "  before\n" << *mesh.mc << std::endl;
		const float tn =  0.0f / 16.0f;
		const float tk = 22.0f / 22.0f;
		const std::pair< cVec3, cVec3 >  vi = mesh.mc->Divide< false >(
			mesh.bi, mesh.ci, tn,
			mesh.ci, mesh.di, tk
		);

		EXPECT_FALSE( mesh.mc->IsValid( true ) ) << *mesh.mc;
		mesh.mc->EraseClearConfluentByIndexOnly();
		std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		std::cout << "  after\n" << *mesh.mc << std::endl;
		ASSERT_NE( cVec3::Infinity, vi.first  ) << "Point 'n' don't inserted.";
		ASSERT_NE( cVec3::Infinity, vi.second ) << "Point 'k' don't inserted.";
		EXPECT_EQ( mesh.bi, mesh.mc->GetVertex( vi.first  ) );
		EXPECT_EQ( mesh.di, mesh.mc->GetVertex( vi.second ) );
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 3, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "E"
TEST_F( cMeshContainerTest, DivideE ) {

	MeshE  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	// m--n
	{
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
		const float tm = 16.0f / 27.0f;
		const float tn = 10.0f / 20.0f;
		ASSERT_THROW( mesh.mc->Divide< false >(
			mesh.di, mesh.ci, tm,
			mesh.bi, mesh.ei, tn
		), comms::Exception );

		EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

		EXPECT_EQ( 5 + 0, pos.Count() ) <<
			"Points 'm', 'n' must be absent.";
		EXPECT_EQ( 2, mesh.mc->GetPolyCount() );
	}
}




// @see cMeshContainerTest.jpg, "M"
TEST_F( cMeshContainerTest, DivideM ) {

	MeshM  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();

	std::cout << "  before\n" << *mesh.mc << std::endl;
	const float  t = 0.5f;
	typedef std::pair< cVec3, cVec3 >  divider_t;

	std::cout << "  before divide 'kl'\n" << *mesh.mc << std::endl;
	const divider_t  kl =
		mesh.mc->Divide< false >( mesh.ai, mesh.bi, t,  mesh.ci, mesh.di, t );
	std::cout << "  after divide 'kl'\n" << *mesh.mc << std::endl;

	std::cout << "  before divide 'lm'\n" << *mesh.mc << std::endl;
	const divider_t  lm =
		mesh.mc->Divide< false >( mesh.ci, mesh.di, t,  mesh.ci, mesh.ei, t );
	std::cout << "  after divide 'lm'\n" << *mesh.mc << std::endl;

	std::cout << "  before divide 'mn'\n" << *mesh.mc << std::endl;
	const divider_t  mn =
		mesh.mc->Divide< false >( mesh.ci, mesh.ei, t,  mesh.ei, mesh.hi, t );
	std::cout << "  after divide 'mn'\n" << *mesh.mc << std::endl;

	std::cout << "  before divide 'no'\n" << *mesh.mc << std::endl;
	const divider_t  no =
		mesh.mc->Divide< false >( mesh.ei, mesh.hi, t,  mesh.ii, mesh.ji, t );
	std::cout << "  after divide 'no'\n" << *mesh.mc << std::endl;

	// # Do erase() after all inserts.
	mesh.mc->EraseClearConfluentByIndexOnly();
	std::cout << "after EraseClearConfluentByIndexOnly()\n" << *mesh.mc << std::endl;
	EXPECT_TRUE( mesh.mc->IsValid( true ) ) << *mesh.mc;

	EXPECT_FALSE( mesh.mc->Contains( mesh.ai, mesh.bi ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.ci, mesh.ei ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.ii, mesh.ji ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.abcd ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.dcef ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.cghe ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.ehij ) );
	EXPECT_EQ( 10 + 5, pos.Count() );
	EXPECT_EQ( 8, mesh.mc->GetPolyCount() );
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, EraseABC ) {

	MeshF  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();
	{
		const cList< int >  polys = mesh.mc->Find( mesh.bi, mesh.ci );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		ASSERT_TRUE( ftr >= 0 );
		mesh.mc->Erase( ftr );
		EXPECT_EQ( 7, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// nodes
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.c ) );
		// edges
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.c ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.c, mesh.b ) );
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, EraseADFGB_WithOptimizeAfter ) {

	MeshF  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();
	{
		const cList< int >  polys = mesh.mc->Find( mesh.gi, mesh.fi );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		ASSERT_TRUE( ftr >= 0 );
		mesh.mc->Erase( ftr, true );
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// nodes
		// unused nodes are removed!
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.g ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b ) );
		// edges
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.f, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.g, mesh.b ) );
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, EraseADFGB_WithoutOptimizeAfter ) {

	MeshF  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();
	{
		const cList< int >  polys = mesh.mc->Find( mesh.gi, mesh.fi );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		ASSERT_TRUE( ftr >= 0 );
		mesh.mc->Erase( ftr );
		EXPECT_EQ( 7, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
		// nodes
		// all nodes are remains!
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.d ) );
		EXPECT_TRUE(  mesh.mc->GetVertex( mesh.f ) != -1 );
		EXPECT_TRUE(  mesh.mc->GetVertex( mesh.g ) != -1 );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.b ) );
		// edges
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.d ) );
		EXPECT_TRUE(  mesh.mc->Contains( mesh.a, mesh.b ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.d, mesh.f ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.f, mesh.g ) );
		EXPECT_FALSE( mesh.mc->Contains( mesh.g, mesh.b ) );
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, EraseAll ) {

	MeshF  mesh;
	const cList< cVec3 >&  pos = mesh.mc->GetPositions();
	// 'adec' by edge 'ec'
	{
		const cList< int >  polys = mesh.mc->Find( mesh.e, mesh.c );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		mesh.mc->Erase( ftr, true );
		EXPECT_EQ( 6, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// 'abc' by edge 'ac'
	{
		const cList< int >  polys = mesh.mc->Find( mesh.a, mesh.c );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		mesh.mc->Erase( ftr, true );
		EXPECT_EQ( 5, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
	// 'adfgb' by node 'a'
	{
		const cList< int >  polys = mesh.mc->Find( mesh.a );
		ASSERT_EQ( 1, polys.Count() );
		const int  ftr = polys.GetFirst();
		mesh.mc->Erase( ftr, true );
		EXPECT_EQ( 0, pos.Count() );
		EXPECT_TRUE( mesh.mc->IsValid( true ) );
	}
}




TEST_F( cMeshContainerTest, EraseByCount_0 ) {

	MeshWithEmptyFigure  mesh;
	std::cout << "  before\n" << *mesh.mc << std::endl;

	mesh.mc->EraseByCount( 0 );
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_EQ( 9 - 2, mesh.mc->GetPolyCount() );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.dgpn ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.nas ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.ef ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.bm ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.c ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.keb ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.blfmcr ) );
	EXPECT_TRUE(  mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, EraseByCount_0_2 ) {

	MeshWithEmptyFigure  mesh;
	std::cout << "  before\n" << *mesh.mc << std::endl;

	mesh.mc->EraseByCount( 0, 2 );
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_EQ( 9 - 5, mesh.mc->GetPolyCount() );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.dgpn ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.nas ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.ef ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.bm ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.c ) ) << "'c' is live as part 'blfmcr'";
	EXPECT_TRUE(  mesh.mc->Contains( mesh.keb ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.blfmcr ) );
	EXPECT_TRUE(  mesh.mc->IsValid( true ) );
}


TEST_F( cMeshContainerTest, EraseByCount_4_INF ) {

	MeshWithEmptyFigure  mesh;
	std::cout << "  before\n" << *mesh.mc << std::endl;

	mesh.mc->EraseByCount( 4, INT_MAX );
	std::cout << "  after\n" << *mesh.mc << std::endl;
	EXPECT_EQ( 9 - 2, mesh.mc->GetPolyCount() );
	EXPECT_FALSE( mesh.mc->Contains( mesh.dgpn ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.nas ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.ef ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.bm ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.c ) );
	EXPECT_TRUE(  mesh.mc->Contains( mesh.keb ) );
	EXPECT_FALSE( mesh.mc->Contains( mesh.blfmcr ) );
	EXPECT_TRUE(  mesh.mc->IsValidRawSequence() );
}


TEST_F( cMeshContainerTest, EraseByCount_IncorrectDiapason ) {

	MeshWithEmptyFigure  mesh;
	EXPECT_THROW( mesh.mc->EraseByCount( 1, 0 ), comms::Exception );
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, EraseClearConfluent ) {

	MeshWithConfluentFigure  mesh;

	cList< figure_t >  all;
	all.AddUnique( mesh.dgh );
	all.AddUnique( mesh.dhke );
	all.AddUnique( mesh.bqa );
	all.AddUnique( mesh.fsd );
	all.AddUnique( mesh.na );
	all.AddUnique( mesh.pointL );
	std::cout << "  all " << all << std::endl;
	std::cout << "  mesh " << *mesh.mc << std::endl;
	//EXPECT_EQ( 6, all.Count() ) << all;

	const int  polyCount = mesh.mc->GetPolyCount();
	mesh.mc->EraseClearConfluent( 0.1f );
	EXPECT_LT( mesh.mc->GetPolyCount(), polyCount );
	for (int k = 0; k < all.Count(); ++k) {
		const figure_t&  figure = all[ k ];
		EXPECT_FALSE( mesh.mc->Contains( figure ) ) << "#" << k << "  " << figure;
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, FindByNode ) {

	MeshF  mesh;

	// a
	{
		// @todo fine  Return an object 'Block'. Add operations for class 'Block'.
		const cList< int >  polys = mesh.mc->Find( mesh.a );
		ASSERT_EQ( 3, polys.Count() );
	}
	// b
	{
		const cList< int >  polys = mesh.mc->Find( mesh.b );
		ASSERT_EQ( 2, polys.Count() );
	}
	// e
	{
		const cList< int >  polys = mesh.mc->Find( mesh.e );
		ASSERT_EQ( 1, polys.Count() );
	}
	// fi
	{
		const cList< int >  polys = mesh.mc->Find( mesh.fi );
		ASSERT_EQ( 1, polys.Count() );
	}
}




// @see cMeshContainerTest.jpg, "F"
TEST_F( cMeshContainerTest, FindByEdge ) {

	MeshF  mesh;

	// a--b
	{
		const cList< int >  polys = mesh.mc->Find( mesh.a, mesh.b );
		ASSERT_EQ( 2, polys.Count() );
	}
	// b--c
	{
		const cList< int >  polys = mesh.mc->Find( mesh.b, mesh.c );
		ASSERT_EQ( 1, polys.Count() );
	}
	// gi--fi
	{
		const cList< int >  polys = mesh.mc->Find( mesh.gi, mesh.fi );
		ASSERT_EQ( 1, polys.Count() );
	}
	// gi--di
	{
		const cList< int >  polys = mesh.mc->Find( mesh.gi, mesh.di );
		ASSERT_EQ( 0, polys.Count() );
	}
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, FindByFigure ) {

	MeshL  mesh;
	// nas
	{
		figure_t  figure;
		{
			figure.Add( mesh.n );
			figure.Add( mesh.a );
			figure.Add( mesh.s );
		}
		const int  poly = mesh.mc->Find( figure );
		EXPECT_NE( -1, poly );
	}
	// -san-
	{
		figure_t  figure;
		{
			figure.Add( mesh.s );
			figure.Add( mesh.a );
			figure.Add( mesh.n );
		}
		const int  poly = mesh.mc->Find( figure );
		EXPECT_EQ( -1, poly );
	}
	// hkbq
	{
		figure_t  figure;
		{
			figure.Add( mesh.h );
			figure.Add( mesh.k );
			figure.Add( mesh.b );
			figure.Add( mesh.q );
		}
		const int  poly = mesh.mc->Find( figure );
		EXPECT_NE( -1, poly );
	}
	// -qbkh-
	{
		figure_t  figure;
		{
			figure.Add( mesh.q );
			figure.Add( mesh.b );
			figure.Add( mesh.k );
			figure.Add( mesh.h );
		}
		const int  poly = mesh.mc->Find( figure );
		EXPECT_EQ( -1, poly );
	}
	// def
	{
		figure_t  figure;
		{
			figure.Add( mesh.d );
			figure.Add( mesh.e );
			figure.Add( mesh.f );
		}
		const int  poly = mesh.mc->Find( figure );
		EXPECT_EQ( -1, poly );
	}
}




// @see cMeshContainerTest.jpg, "L"
TEST_F( cMeshContainerTest, FindClearConfluent ) {

	MeshWithConfluentFigure  mesh;

	cList< int >  all;
	all.AddUnique( mesh.mc->Find( mesh.dgh ) );
	all.AddUnique( mesh.mc->Find( mesh.dhke ) );
	all.AddUnique( mesh.mc->Find( mesh.bqa ) );
	all.AddUnique( mesh.mc->Find( mesh.fsd ) );
	all.AddUnique( mesh.mc->Find( mesh.na ) );
	all.AddUnique( mesh.mc->Find( mesh.pointL ) );
	std::cout << "  all " << all << std::endl;
	std::cout << "  mesh " << *mesh.mc << std::endl;
	EXPECT_EQ( 6, all.Count() ) << all;

	const cList< int >  fs = mesh.mc->FindClearConfluent( 0.1f );
	EXPECT_EQ( 6, fs.Count() ) << fs;
	for (int k = 0; k < fs.Count(); ++k) {
		const int  iRaw = fs[ k ];
		ASSERT_NE( -1, iRaw );
		EXPECT_TRUE( all.Contains( iRaw ) ) << iRaw;
	}
}





TEST_F( cMeshContainerTest, ContainsLine ) {

	MeshConeSide5  tm;

	// faces from 'TestResources/cone-side-5.obj'
	//   f 5 2 6
	//   f 1 2 3
	//   f 4 2 5
	//   f 6 2 1
	//   f 3 2 4
	//   f 1 3 4 5 6

	//   f 1 3 4 5 6
	EXPECT_TRUE( tm.mc->Contains( tm.a, tm.c ) );
	EXPECT_TRUE( tm.mc->Contains( tm.c, tm.d ) );
	EXPECT_TRUE( tm.mc->Contains( tm.e, tm.f ) );
	EXPECT_TRUE( tm.mc->Contains( tm.f, tm.e ) );

	//   f 1 2 3
	EXPECT_TRUE( tm.mc->Contains( tm.a, tm.b ) );
	EXPECT_TRUE( tm.mc->Contains( tm.b, tm.a ) );

	EXPECT_FALSE( tm.mc->Contains( tm.f, tm.d ) );

	EXPECT_FALSE( tm.mc->Contains( tm.a, tm.absentA ) );
	EXPECT_FALSE( tm.mc->Contains( tm.absentA, tm.a ) );
	EXPECT_FALSE( tm.mc->Contains( tm.absentA, tm.absentB ) );
}




TEST_F( cMeshContainerTest, ContainsNode ) {

	MeshConeSide5  tm;

	EXPECT_TRUE( tm.mc->Contains( tm.a ) );
	EXPECT_TRUE( tm.mc->Contains( tm.b ) );
	EXPECT_TRUE( tm.mc->Contains( tm.c ) );
	EXPECT_TRUE( tm.mc->Contains( tm.d ) );
	EXPECT_TRUE( tm.mc->Contains( tm.e ) );
	EXPECT_TRUE( tm.mc->Contains( tm.f ) );

	EXPECT_FALSE( tm.mc->Contains( tm.absentA ) );
	EXPECT_FALSE( tm.mc->Contains( tm.absentB ) );
}


} }  //namespaces
