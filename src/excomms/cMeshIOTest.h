#pragma once

#include "cMeshIO.h"
#include <gtest/gtest.h>


namespace test {
	namespace meshio {


class cMeshIOTest :
	public ::testing::Test {
protected:
	cMeshIOTest() {}
	virtual ~cMeshIOTest() {}
	virtual void SetUp() {}
	virtual void TearDown() {}
};




TEST_F( cMeshIOTest, LoadMesh ) {

	comms::cMeshContainer*  mc =
		comms::cMeshIO::LoadMesh( "C:/test210/TestResources/cone-side-5.obj" );
	ASSERT_TRUE( mc );
	const int  polyCount = mc->GetPolyCount();
	ASSERT_EQ( 6, polyCount ) << "Mesh is not 3D-object? Count of polygons is " << polyCount;

	// @todo Add tests for all supported formats.
}




TEST_F( cMeshIOTest, SaveMesh ) {

	comms::cMeshContainer*  mc =
		comms::cMeshIO::LoadMesh( "C:/test210/TestResources/cone-side-5.obj" );
	ASSERT_TRUE( mc );
	const int  polyCount = mc->GetPolyCount();
	const bool  r =
		comms::cMeshIO::SaveMesh( *mc, "C:/test210/TestResources/cone-side-5.test.obj" );
	ASSERT_TRUE( r ) << "Mesh is not saved.";
	comms::cMeshContainer*  loaded =
		comms::cMeshIO::LoadMesh( "C:/test210/TestResources/cone-side-5.test.obj" );
	ASSERT_TRUE( loaded );
	const int  loadedPolyCount = loaded->GetPolyCount();
	ASSERT_EQ( polyCount, loadedPolyCount );

	// @todo Add tests for all supported formats.
}


} }  //namespaces
