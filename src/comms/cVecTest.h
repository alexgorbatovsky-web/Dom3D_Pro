#pragma once

#include "cVec.h"
#include <gtest/gtest.h>


namespace test {
	namespace vec {


class cVecTest :
	public ::testing::Test
{
protected:
	typedef comms::cVec< float, 3 >  vec_t;

protected:
	cVecTest() {}
	virtual ~cVecTest() {}
	virtual void SetUp() {}
	virtual void TearDown() {}
};




TEST_F( cVecTest, ConstructPuttingAsideLine ) {

	using namespace comms;

	const vec_t  a( 10, 30, 0 );
	const vec_t  b( 50, 30, 0 );

	// on line
	{
		const vec_t  r( a, b, 10.0f / 40.0f );
		const vec_t  m( 20, 30, 0 );
		EXPECT_EQ( m, r );
	}
	// on edges
	{
		EXPECT_EQ( a,  vec_t( a, b, 0.0f ) );
		EXPECT_EQ( b,  vec_t( a, b, 1.0f ) );
	}
	// out of edges
	{
		EXPECT_ANY_THROW( vec_t( a, b, -0.2f ) );
		EXPECT_ANY_THROW( vec_t( a, b,  1.2f ) );
	}
}


} }  //namespaces
