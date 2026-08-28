#pragma once

#ifndef PLASTYCAD

#include "cMeshContainer.h"
#include "../comms/cLine.h"
#include "../comms/cPlane.h"
#include "MeshCutter.h"
#include "MeshDivider.h"
#include "../../3D-Coat/MC/ProceduralPrim.h"
#include <gtest/gtest.h>


namespace test {
	namespace meshcontainervisual {


struct MeshAngle : public test::Mesh {
	const cVec3  a, b, c, d;

	MeshAngle() :
		a(  10,  35,   0 ),
		b(  45,  35,   0 ),
		c(  35,  50, -50 ),
		d(  30,  10, -90 )
	{
		cList< cVec3  >&  pos = mc->GetPositions();
		auto&  raw = mc->GetRaw();
		{
			// top
			raw.Add( cVec3i( 3, 0, 0 ) );
			raw.Add( cVec3i( pos.Add( a ), 0, 0 ) );
			raw.Add( cVec3i( pos.Add( b ), 0, 0 ) );
			raw.Add( cVec3i( pos.Add( c ), 0, 0 ) );
			// bottom
			raw.Add( cVec3i( 3, 0, 0 ) );
			raw.Add( cVec3i( pos.Add( a ), 0, 0 ) );
			raw.Add( cVec3i( pos.Add( d ), 0, 0 ) );
			raw.Add( cVec3i( pos.Add( b ), 0, 0 ) );
		}
		WeldMesh( mc, 50000 );
		mc->RemoveUnusedVerts();
		// # Disable this if-endif for running the tests faster.
#if 1
		EXPECT_EQ( 2, mc->GetPolyCount() );
		EXPECT_TRUE( mc->IsValid( true ) );
		EXPECT_EQ( 4, pos.Count() );
		EXPECT_EQ( 2, mc->GetTrisCount() );
		EXPECT_EQ( 2, mc->GetPolyCount() );
#endif
	}
};


// @see http://en.wikipedia.org/wiki/File:Tetrahedron.svg
struct MeshConeSide3Divided : public test::Mesh {
	MeshConeSide3Divided( float s ) : Mesh( "cone-side-3-divided.obj" ) {
		const comms::cMat4  scale( cVec3( s ), cVec3::Zero );
		mc->Transform( scale );
		mc->Triangulate();
	}
};


struct MeshPlane5WithMixedFaces1 : public test::Mesh {
	MeshPlane5WithMixedFaces1() : Mesh( "plane-5-with-mixed-faces-1.obj" ) {}
};


struct MeshCubeWithMixedFaces : public test::Mesh {
	MeshCubeWithMixedFaces() : Mesh( "cube-with-mixed-faces.obj" ) {}
};


template< size_t quads >
struct MeshCube : public test::Mesh {
	MeshCube( float size ) {
		Matrix4D  m;
		m.SetRow0( Vector4D( Vector3D( 1, 0, 0 ) * size / 2.0f, 0.0f ) );
		m.SetRow1( Vector4D( Vector3D( 0, 1, 0 ) * size / 2.0f, 0.0f ) );
		m.SetRow2( Vector4D( Vector3D( 0, 0, 1 ) * size / 2.0f, 0.0f ) );
		m.SetRow3( Vector4D( 0, 0, 0, 1 ) );

		mc->CreateCube( Vector3D( 2.0f ) );
		const float  unit = size / (float)quads;
		mc->QuadQuantSubd( m, unit, 0.9f );
		//mc->Triangulate();
		mc->Transform( m );
	}
};




class cMeshContainerVisualTest :
	public ::testing::Test {
public:
	typedef comms::cLine< float, 3 >  line_t;

protected:
	cMeshContainerVisualTest() {}
	virtual ~cMeshContainerVisualTest() {}
	virtual void SetUp() {}
	virtual void TearDown() {}

};




TEST_F( cMeshContainerVisualTest, DISABLED_CutMeshByPlane ) {

	MeshAngle  mesh;

	// cut by plane
	// point on the center of the line 'ab'
	const cVec3  pab = (mesh.a + mesh.b) / 2.0f;
	// normal from 'pc' to 'cd'
	const cVec3  pcd = (mesh.c + mesh.d) / 2.0f;
	const cVec3  pn = cVec3::Normalize( pcd - pab );
	// shift the point 'ab' by the normal 'pn'
	const float  abcdDistance = (pcd - pab).Length();
	// part of mesh for absorption
	const float  part = 5.0f;
	const cVec3  pabShifted = pab + pn * (abcdDistance / part);
	{
		static const DWORD  colorGreen = 0xFF00AA00;
		AddDbgPoint( pab, colorGreen );
		line_t  ln;
		ln += pabShifted;
		ln += pcd;
		AddDbgLine( ln, colorGreen );
	}
	const comms::cPlane  plane( pabShifted, pn );
	cList< cVec3 >  cutted;
	mesh.mc->CutByPlane( plane, &cutted );
	{
		EXPECT_EQ( 4, cutted.Count() );
		const cVec3  expected[] = {
			// # Recieved below and frozen here.
			cVec3( 42.3259f, 39.0111f, -13.3704f ),
			cVec3( 17.588f,  39.5528f, -15.1761f ),
			cVec3( 13.3027f, 30.8716f, -14.8621f ),
			cVec3( 42.8681f, 31.4469f, -12.7913f )
		};
		for (int i = 0; i < cutted.Count(); ++i) {
			const cVec3&  v = cutted[ i ];
			//std::cout << i << " " << static_cast< comms::cVec< float, 3 > >( v ) << "\n";
			EXPECT_EQ( expected[ i ], v );
			static const DWORD  colorRed = 0xFFAA0000;
			AddDbgPoint( v, colorRed );
			AddDbgText( v,  colorRed,  cStr::ToString( i ).ToCharPtr() );
		}
	}

	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );
}




// Top part of mesh is removed after cMeshContainer::CutMesh().
// @see cMeshContainerVisualTest::CutMesh()
TEST_F( cMeshContainerVisualTest, DISABLED_CutMeshByPlane_TotalAbsorptionTop ) {

	MeshAngle  mesh;

	// cut by plane
	// point on the center of the line 'ab'
	const cVec3  pab = (mesh.a + mesh.b) / 2.0f;
	// normal from 'pc' to 'cd'
	const cVec3  pcd = (mesh.c + mesh.d) / 2.0f;
	const cVec3  pn = cVec3::Normalize( pcd - pab );
	// shift the point 'ab' by the normal 'pn'
	const float  abcdDistance = (pcd - pab).Length();
	// total absorption of top part of mesh
	const float  part = 1.0f;
	const cVec3  pabShifted = pab + pn * (abcdDistance / part);
	{
		static const DWORD  colorGreen = 0xFF00AA00;
		AddDbgPoint( pab, colorGreen );
		line_t  ln;
		ln += pabShifted;
		ln += pcd;
		AddDbgLine( ln, colorGreen );
	}
	const comms::cPlane  plane( pabShifted, pn );
	cList< cVec3 >  cutted;
	mesh.mc->CutByPlane( plane, &cutted );
	{
		EXPECT_EQ( 2, cutted.Count() );
		for (int i = 0; i < cutted.Count(); ++i) {
			const cVec3&  v = cutted[ i ];
			static const DWORD  colorRed = 0xFFAA0000;
			AddDbgPoint( v, colorRed );
			AddDbgText( v,  colorRed,  cStr::ToString( i ).ToCharPtr() );
		}
	}

	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );
}




// The mesh is removed totally after cMeshContainer::CutMesh().
// @see cMeshContainerVisualTest::CutMesh()
TEST_F( cMeshContainerVisualTest, DISABLED_CutMeshByPlane_TotalAbsorption ) {

	MeshAngle  mesh;

	// cut by plane
	// point on the center of the line 'ab'
	const cVec3  pab = (mesh.a + mesh.b) / 2.0f;
	// normal from 'pc' to 'cd'
	const cVec3  pcd = (mesh.c + mesh.d) / 2.0f;
	const cVec3  pn = cVec3::Normalize( pcd - pab );
	// shift the point 'ab' by the normal 'pn'
	const float  abcdDistance = (pcd - pab).Length();
	// total absorption of mesh
	const float  part = 0.5f;
	const cVec3  pabShifted = pab + pn * (abcdDistance / part);
	{
		static const DWORD  colorGreen = 0xFF00AA00;
		AddDbgPoint( pab, colorGreen );
		line_t  ln;
		ln += pabShifted;
		ln += pcd;
		AddDbgLine( ln, colorGreen );
	}
	const comms::cPlane  plane( pabShifted, pn );
	cList< cVec3 >  cutted;
	mesh.mc->CutByPlane( plane, &cutted );
	{
		EXPECT_EQ( 0, cutted.Count() );
		for (int i = 0; i < cutted.Count(); ++i) {
			const cVec3&  v = cutted[ i ];
			static const DWORD  colorRed = 0xFFAA0000;
			AddDbgPoint( v, colorRed );
			AddDbgText( v,  colorRed,  cStr::ToString( i ).ToCharPtr() );
		}
	}

	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );
}




TEST_F( cMeshContainerVisualTest, DISABLED_CutMeshByLineDistance_Simplest_Segment1 ) {

	MeshAngle  mesh;

	// cut by distance to line
	comms::cLineF3  l;
	l += mesh.a;
	l += mesh.b;
	{
		static const DWORD  colorGreen = 0xFF00AA00;
		AddDbgLine( l, colorGreen );
	}
	comms::LineDistanceCutter::openVerts_t  cutted;
	comms::LineDistanceCutter  cutter( l, 30, &cutted );
	mesh.mc->CutMesh( cutter );
	{
		EXPECT_EQ( 4, cutted.Count() );
		for (int i = 0; i < cutted.Count(); ++i) {
			const cVec3&  v = cutted[ i ].second;
			static const DWORD  colorRed = 0xFFAA0000;
			AddDbgPoint( v, colorRed );
			AddDbgText( v,  colorRed,  cStr::ToString( i ).ToCharPtr() );
		}
	}

	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );
}




TEST_F( cMeshContainerVisualTest, DISABLED_CutMeshByLineDistance_Edges3Divided_Segment ) {

	static const float  S = 100.0f;
	MeshConeSide3Divided  mesh( S );

	// cut by distance to line
	comms::cLineF3  l;
	const cVec3  x = cVec3(  0.000000,  1.500000,  0.000000 ) * S;
	l += x;
	const cVec3  a = cVec3(  0.866025, -1.500000,  0.500000 ) * S;
	l += a;
	const cVec3  b = cVec3( -0.866025, -1.500000,  0.500000 ) * S;
	l += b;
	const cVec3  c = cVec3(  0.000000, -1.500000, -1.000000 ) * S;
	l += c;
	{
		static const DWORD  colorGreen = 0xFF00AA00;
		AddDbgLine( l, colorGreen );
	}
	comms::LineDistanceCutter::openVerts_t  cutted;
	// @see create 'l' above
	const float  sizeBottomEdge = (a - b).Length();
	comms::LineDistanceCutter  cutter( l, sizeBottomEdge / 3 / 3, &cutted );
	mesh.mc->CutMesh( cutter );
	{
		EXPECT_EQ( 162, cutted.Count() );
		for (int i = 0; i < cutted.Count(); ++i) {
			const cVec3&  v = cutted[ i ].second;
			static const DWORD  colorRed = 0xFFAA0000;
			AddDbgPoint( v, colorRed );
			AddDbgNumber( v, i, colorRed );
		}
	}

	mesh.mc->DrawDbg( comms::cMat4::Identity, 0x55000000, 0x55AAAAAA );
}




TEST_F( cMeshContainerVisualTest, DISABLED_CorrectFacesForPlane5WithMixedFaces1 ) {

	MeshPlane5WithMixedFaces1  mesh;
	std::cout << "  mesh before correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.CorrectFacesForPlane5WithMixedFaces1.before.auto.obj" );
	DbgLayer( "Original" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA, 0xFFFFFFFF );

	try {
		mesh.mc->CorrectFaces( 0 );
	} catch ( const std::exception& ex ) {
		std::cout << ex.what() << std::endl;
	} catch ( ... ) {
		std::cout << "Unknown exception." << std::endl;
	}
	std::cout << "  mesh after correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.CorrectFacesForPlane5WithMixedFaces1.after.auto.obj" );
	DbgLayer( "Result" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA, 0xFFFFFFFF );
}




TEST_F( cMeshContainerVisualTest, DISABLED_CorrectFacesForCubeWithMixedFaces ) {

	MeshCubeWithMixedFaces  mesh;
	std::cout << "  mesh before correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.CorrectFacesForCubeWithMixedFaces.before.auto.obj" );
	DbgLayer( "Original" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0x77AAAAAA );

	try {
		mesh.mc->CorrectFaces( 0 );
	} catch ( const std::exception& ex ) {
		std::cout << ex.what() << std::endl;
	} catch ( ... ) {
		std::cout << "Unknown exception." << std::endl;
	}
	std::cout << "  mesh after correct faces\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.CorrectFacesForCubeWithMixedFaces.after.auto.obj" );
	DbgLayer( "Result" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0x77AAAAAA );
}




TEST_F( cMeshContainerVisualTest, DISABLED_QuadQuantSubd_All ) {

	const size_t  quads = 5;
	const float  side = 200.0f;
	MeshCube< quads >  mesh( side );
	//mesh.mc->Triangulate();

	std::cout << "  mesh before\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.QuadQuantSubd.before.auto.obj" );
	DbgLayer( "Original" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );

	const float  unit = side * 2 / (float)quads;
	mesh.mc->QuadQuantSubd( comms::cMat4::Identity, unit, 0.9f );
	std::cout << "  mesh after\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.QuadQuantSubd.after.auto.obj" );
	DbgLayer( "Result" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xFFAAAAAA );
}




TEST_F( cMeshContainerVisualTest, DISABLED_QuadQuantSubd_Partial ) {

	const size_t  quads = 5;
	const float  side = 200.0f;
	MeshCube< quads >  mesh( side );
	//mesh.mc->Triangulate();

	std::cout << "  mesh before\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.QuadQuantSubd.before.auto.obj" );
	DbgLayer( "Original" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xAAAAAAAA );

	FAIL() << "@todo Realize cMeshContainer::QuadQuantSubd( MeshDivider )";
	mesh.mc->QuadQuantSubd(
		comms::SphereDividerIndices( cVec3::Zero,  side / 2.0f ),
		2
	);
	std::cout << "  mesh after\n" << *mesh.mc << std::endl;
	comms::cMeshIO::SaveMesh( *mesh.mc, "cMeshContainerVisualTest.QuadQuantSubd.after.auto.obj" );
	DbgLayer( "Result" );
	mesh.mc->DrawDbg( comms::cMat4::Identity, 0xFF000000, 0xAAAAAAAA );
}


} }  //namespaces


#endif  //PLASTYCAD
