#pragma once

#include "cBounds.h"
#include <gtest/gtest.h>


namespace test {
	namespace bounds {


class BoundsTest :
	public ::testing::Test
{
protected:
	typedef comms::Bounds< int, 2 >  bounds_t;
	typedef bounds_t::coord_t        coord_t;
	typedef comms::Bounds< int, 3 >  bounds3_t;
	typedef bounds3_t::coord_t       coord3_t;


protected:
	BoundsTest() {}
	virtual ~BoundsTest() {}
	virtual void SetUp() {}
	virtual void TearDown() {}
};




TEST_F( BoundsTest, ConstructEmpty ) {

	using namespace comms;

	{
		const bounds_t  bounds;
		ASSERT_EQ( bounds_t::ZERO(), bounds );
		ASSERT_TRUE( bounds.IsEmpty() );
	}
	{
		const bounds_t  bounds( coord_t::ZERO(), coord_t::ZERO() );
		ASSERT_TRUE( bounds.IsEmpty() );
	}
	{
		const bounds_t  bounds( coord_t::ZERO(), coord_t::ONE() );
		ASSERT_TRUE( !bounds.IsEmpty() );
	}
}




TEST_F( BoundsTest, ConstructIncorrect ) {

	using namespace comms;

	ASSERT_THROW( bounds_t( coord_t::ONE(), coord_t::ZERO() ),  comms::Exception );
	ASSERT_NO_THROW( bounds_t( coord_t( -1 ), coord_t( 1 ) ) );
}




TEST_F( BoundsTest, AddPoint ) {

	using namespace comms;

	const coord_t  ca(  0,  0 );
	const coord_t  cb( 10, 20 );
	bounds_t  bounds( ca, cb );
	const bounds_t  etalon = bounds;

	// center
	const coord_t  p1( 5, 6 );
	EXPECT_FALSE( bounds.AddPoint( p1 ) );
	EXPECT_EQ( etalon, bounds );

	// border
	const coord_t  p2( 0, 0 );
	EXPECT_FALSE( bounds.AddPoint( p2 ) );
	EXPECT_EQ( etalon, bounds );
	const coord_t  p3( 10, 20 );
	EXPECT_FALSE( bounds.AddPoint( p3 ) );
	EXPECT_EQ( etalon, bounds );

	// out of border
	const coord_t  p4( -1, 0 );
	EXPECT_TRUE( bounds.AddPoint( p4 ) );
	EXPECT_EQ( bounds_t( p4, cb ),  bounds );
	const coord_t  p5( -100, 200 );
	EXPECT_TRUE( bounds.AddPoint( p5 ) );
	EXPECT_EQ( bounds_t( coord_t( -100, 0 ), coord_t( 10, 200 ) ),  bounds );
}




TEST_F( BoundsTest, ContainsPointInEmpty ) {

	using namespace comms;

	const bounds_t  bounds;

	const coord_t  p1( 0, 0 );
	EXPECT_FALSE( bounds.ContainsPoint( p1 ) );
	const coord_t  p2( -4, 10 );
	EXPECT_FALSE( bounds.ContainsPoint( p2 ) );
}




TEST_F( BoundsTest, ContainsPoint ) {

	using namespace comms;

	const coord_t  ca(  0,  0 );
	const coord_t  cb( 10, 20 );
	const bounds_t  bounds( ca, cb );

	// center
	const coord_t  p1( 5, 6 );
	EXPECT_TRUE( bounds.ContainsPoint( p1 ) );

	// border
	const coord_t  p2( 0, 0 );
	EXPECT_TRUE( bounds.ContainsPoint( p2 ) );
	const coord_t  p3( 10, 20 );
	EXPECT_TRUE( bounds.ContainsPoint( p3 ) );

	// out of border
	const coord_t  p4( -1, 0 );
	EXPECT_FALSE( bounds.ContainsPoint( p4 ) );
	const coord_t  p5( 150, 250 );
	EXPECT_FALSE( bounds.ContainsPoint( p5 ) );
}




/* Good, no testing!
   error LNK2001: unresolved external symbol Bounds<>.ContainsLine()
TEST_F( BoundsTest, ContainsLineMismatchDimensions ) {

	using namespace comms;

	// 2D bounds, 3D line
	{
		const coord_t  ca(  0,  0 );
		const coord_t  cb( 10, 20 );
		bounds_t  bounds( ca, cb );
		const coord3_t  a( 0, 0, 0 );
		const coord3_t  b( 9, 19, 39 ); 
		ASSERT_THROW( bounds.ContainsLine( a, b ),  comms::Exception );
	}
	// 3D bounds, 2D line
	{
		const coord3_t  ca(  0,  0, 0 );
		const coord3_t  cb( 10, 20, 50 );
		bounds3_t  bounds( ca, cb );
		const coord_t  a( 0, 0 );
		const coord_t  b( 9, 19 ); 
		ASSERT_THROW( bounds.ContainsLine( a, b ), comms::Exception );
	}
}
*/




TEST_F( BoundsTest, ContainsLine2D ) {

	using namespace comms;

	const coord_t  ca(  0,  0 );
	const coord_t  cb( 10, 20 );
	bounds_t  bounds( ca, cb );
	const bounds_t  etalon = bounds;

	// inner
	{
		const coord_t  a( 5, 6 );
		const coord_t  b( 9, 1 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// one node on border
	{
		const coord_t  a( 0, 0 );
		const coord_t  b( 9, 19 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// one node out of border
	{
		const coord_t  a( -3, 5 );
		const coord_t  b( 9, 19 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// two nodes out of border
	{
		const coord_t  a( -3, 5 );
		const coord_t  b( 11, 22 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// line out of bounds
	{
		const coord_t  a( -3, -5 );
		const coord_t  b( -6, -7 ); 
		EXPECT_FALSE( bounds.ContainsLine( a, b ) );
	}
	{
		const coord_t  a( 11, 22 );
		const coord_t  b( 30, 40 ); 
		EXPECT_FALSE( bounds.ContainsLine( a, b ) );
	}
}




TEST_F( BoundsTest, ContainsLine3D ) {

	using namespace comms;

	const coord3_t  ca(  0,  0,  0 );
	const coord3_t  cb( 10, 30, 70 );
	bounds3_t  bounds( ca, cb );
	const bounds3_t  etalon = bounds;

	// inner
	{
		const coord3_t  a( 5, 6, 7 );
		const coord3_t  b( 9, 1, 3 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// one node on border
	{
		const coord3_t  a( 0,  0,  0 );
		const coord3_t  b( 9, 19, 62 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// one node out of border
	{
		const coord3_t  a( -3, 5, -14 );
		const coord3_t  b( 9, 19,  62 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// two nodes out of border
	{
		const coord3_t  a( -3,  5, -14 );
		const coord3_t  b( 11, 33,  77 ); 
		EXPECT_TRUE( bounds.ContainsLine( a, b ) );
	}
	// line out of bounds
	{
		const coord3_t  a( -3, -5, -14 );
		const coord3_t  b( -6, -7,  -8 ); 
		EXPECT_FALSE( bounds.ContainsLine( a, b ) );
	}
	{
		const coord3_t  a(  11,  33,  77 );
		const coord3_t  b( 110, 330, 770 ); 
		EXPECT_FALSE( bounds.ContainsLine( a, b ) );
	}
}


} }  //namespaces
