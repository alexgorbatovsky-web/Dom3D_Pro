#include "comms.h"

template<class Type, int Dim>
const typename comms::cLine<Type, Dim>::sharpNode_t
node_t_to_sharpNode_t(const typename comms::cLine<Type, Dim>::node_t &N) {
	typename comms::cLine<Type, Dim>::sharpNode_t SN;
	for(int i = 0; i < Dim; i++) {
		SN[i] = (typename comms::cLine<Type, Dim>::sharpNode_t::value_type)N[i];
	}
	return SN;
}

template<class Type, int Dim>
const typename comms::cLine<Type, Dim>::node_t
sharpNode_t_to_node_t(const typename comms::cLine<Type, Dim>::sharpNode_t &SN) {
	typename comms::cLine<Type, Dim>::node_t N;
	for(int i = 0; i < Dim; i++) {
		N[i] = (typename comms::cLine<Type, Dim>::node_t::value_type)SN[i];
	}
	return N;
}

template< class Type, int Dim  >
typename comms::cLine< Type, Dim >::distances_t
comms::cLine< Type, Dim >::Distances( const node_t& point ) const {
	distances_t  r;
	const sharpNode_t  sp = node_t_to_sharpNode_t<Type, Dim>(point);
	for (int i = 0; i < CountSegment(); ++i) {
		const sharpNode_t&  a = ( *this )[ i     ];
		const sharpNode_t&  b = ( *this )[ i + 1 ];
		const float  d = cMath::Sqrt( sp.DistanceToLineSegSq( a, b ) );
		r.Add( d );
	}
	return r;
}

template
comms::cLine< int, 1 >::distances_t
comms::cLine< int, 1 >::Distances( const node_t& ) const;

template
comms::cLine< int, 2 >::distances_t
comms::cLine< int, 2 >::Distances( const node_t& ) const;

template
comms::cLine< int, 3 >::distances_t
comms::cLine< int, 3 >::Distances( const node_t& ) const;

template
comms::cLine< int, 4 >::distances_t
comms::cLine< int, 4 >::Distances( const node_t& ) const;

template
comms::cLine< float, 1 >::distances_t
comms::cLine< float, 1 >::Distances( const node_t& ) const;

template
comms::cLine< float, 2 >::distances_t
comms::cLine< float, 2 >::Distances( const node_t& ) const;

template
comms::cLine< float, 3 >::distances_t
comms::cLine< float, 3 >::Distances( const node_t& ) const;

template
comms::cLine< float, 4 >::distances_t
comms::cLine< float, 4 >::Distances( const node_t& ) const;




template< class Type, int Dim  >
float
comms::cLine< Type, Dim >::Length() const {
	float  len = 0.0f;
	for (int segment = 0; segment < CountSegment(); ++segment) {
		len += Length( segment );
	}
	return len;
}

template
float
comms::cLine< int, 1 >::Length() const;

template
float
comms::cLine< int, 2 >::Length() const;

template
float
comms::cLine< int, 3 >::Length() const;

template
float
comms::cLine< int, 4 >::Length() const;

template
float
comms::cLine< float, 1 >::Length() const;

template
float
comms::cLine< float, 2 >::Length() const;

template
float
comms::cLine< float, 3 >::Length() const;

template
float
comms::cLine< float, 4 >::Length() const;




template< class Type, int Dim  >
float
comms::cLine< Type, Dim >::Length( int segment ) const {
	if ((segment >= CountSegment()) || (segment < 0)) {
		cAssertM(0, "Out of line." );
	}
	const node_t&  a = ( *this )[ segment     ];
	const node_t&  b = ( *this )[ segment + 1 ];
	const node_t  ab = b - a;

	return node_t_to_sharpNode_t<Type, Dim>(ab).Length();
}

template
float
comms::cLine< int, 1 >::Length( int ) const;

template
float
comms::cLine< int, 2 >::Length( int ) const;

template
float
comms::cLine< int, 3 >::Length( int ) const;

template
float
comms::cLine< int, 4 >::Length( int ) const;

template
float
comms::cLine< float, 1 >::Length( int ) const;

template
float
comms::cLine< float, 2 >::Length( int ) const;

template
float
comms::cLine< float, 3 >::Length( int ) const;

template
float
comms::cLine< float, 4 >::Length( int ) const;




template< class Type, int Dim  >
typename comms::cLine< Type, Dim >::sharpNode_t
comms::cLine< Type, Dim >::Normal() const {
	// @todo extend
	if ( IsEmpty() ) {
		cAssertM(0, "Line is empty." );
	}
	if ( !IsValid() ) {
		cAssertM(0, "Line is invalid." );
	}
	if (CountNode() <= 2) {
		cAssertM(0, "Count of nodes must be larger 2." );
	}
	// aggregate vectors from center and create 2 "average vectors" by
	// all points of line
	::std::pair< sharpNode_t, sharpNode_t >  vp =
		::std::make_pair( sharpNode_t::ZERO(), sharpNode_t::ZERO() );
	const sharpNode_t&  c = Center();
	int i = 0;
	for ( ;  i < (CountNode() / 2);  ++i) {
		{
			const sharpNode_t&  p = ( *this )[ i ];
			const sharpNode_t&  pc = p - c;
			vp.first += pc;
		}
		{
			const sharpNode_t&  p = ( *this )[ CountNode() - i - 1 ];
			const sharpNode_t&  pc = p - c;
			vp.second += pc;
		}
	} // for ( ;  i < (CountNode() / 2);  ++i)
	if ((CountNode() % 2) != 0) {
		vp.second += ( *this )[ i ];
	}

	sharpNode_t  r = sharpNode_t::Cross( vp.first, vp.second );
	r.Normalize();

	return r;
}


template
comms::cLine< int, 3 >::sharpNode_t
comms::cLine< int, 3 >::Normal() const;

template
comms::cLine< float, 3 >::sharpNode_t
comms::cLine< float, 3 >::Normal() const;




template< class Type, int Dim  >
typename comms::cLine< Type, Dim >::node_t
comms::cLine< Type, Dim >::Point( float t ) const {
	if ( (t < 0.0f) || (t > 1.0f) ) {
		cAssertM(0, "Out of line." );
	}
	if ( IsEmpty() ) {
		cAssertM(0, "Line is empty." );
	}
	if ( !IsValid() ) {
		cAssertM(0, "Line is invalid." );
	}

	// one segment, fast calc
	if (CountSegment() == 1) {
		const sharpNode_t  a = node_t_to_sharpNode_t<Type, Dim>( ( *this )[ 0 ] );
		const sharpNode_t  b = node_t_to_sharpNode_t<Type, Dim>( ( *this )[ 1 ] );
		const sharpNode_t  sr = Point( a, b, t );
		// # We save digits by truncate. See cVec<>::operator SC().
		return sharpNode_t_to_node_t<Type, Dim>( sr );
	}

	// many segments, look and calc
	const float  len = Length();
	float  nextDT = 0.0f;
	sharpNode_t  sr;
	for (int i = 0; i < CountSegment(); ++i) {
		// find segment
		const sharpNode_t  a = node_t_to_sharpNode_t<Type, Dim>( ( *this )[ i     ] );
		const sharpNode_t  b = node_t_to_sharpNode_t<Type, Dim>( ( *this )[ i + 1 ] );
		const sharpNode_t  v = b - a;
		const float  lenI = v.Length();
		const float  dt = lenI / len;
		if ((nextDT + dt) < t) {
			nextDT += dt;
			continue;
		}
		// calc point
		const float  k = (t - nextDT) / dt;
		sr = v * k + a;
		break;
	} // for (int i = 0; ...

	// # We save digits by truncate. See cVec<>::operator SC().
	return sharpNode_t_to_node_t<Type, Dim>(sr);
}

template
comms::cLine< int, 1 >::node_t
comms::cLine< int, 1 >::Point( float ) const;

template
comms::cLine< int, 2 >::node_t
comms::cLine< int, 2 >::Point( float ) const;

template
comms::cLine< int, 3 >::node_t
comms::cLine< int, 3 >::Point( float ) const;

template
comms::cLine< int, 4 >::node_t
comms::cLine< int, 4 >::Point( float ) const;

template
comms::cLine< float, 1 >::node_t
comms::cLine< float, 1 >::Point( float ) const;

template
comms::cLine< float, 2 >::node_t
comms::cLine< float, 2 >::Point( float ) const;

template
comms::cLine< float, 3 >::node_t
comms::cLine< float, 3 >::Point( float ) const;

template
comms::cLine< float, 4 >::node_t
comms::cLine< float, 4 >::Point( float ) const;




template< class Type, int Dim  >
typename comms::cLine< Type, Dim >::sharpNode_t
comms::cLine< Type, Dim >::NearestProjection(
	const node_t&  point,
	float*         nepper,
	float*         nepperSegment,
	int*           segment
) const {
	const sharpNode_t  sp = node_t_to_sharpNode_t<Type, Dim>( point );
	// # Visit by all segments.
	sharpNode_t  nearestCoord = sharpNode_t::UNDEFINED();
	float  nearestDistanceSq = ::std::numeric_limits< float >::max();
	int foundSegment = -1;
	float  foundNepper = FLT_MAX;
	for (int i = 0; i < CountSegment(); ++i) {
		const sharpNode_t&  a = ( *this )[ i     ];
		const sharpNode_t&  b = ( *this )[ i + 1 ];
		// # By analogy a calc of distance from point to segment.
		//   See cVec::DistanceToLineSegSq().
		const sharpNode_t  diff = sp - a;
		const sharpNode_t  ab = b - a;
		const float  lengthSq = ab.LengthSq();
		const float  scale = (lengthSq < cMath::Epsilon) ?
			0.5f : (sharpNode_t::Dot( diff, ab ) / lengthSq);
		// @todo Fix a potential unsharp in the cVec::DistanceToLineSegSq().
		const sharpNode_t  to =
			(scale < cMath::Epsilon)          ? a :
			(scale > (1.0f - cMath::Epsilon)) ? b : (a + ab * scale);
		const float  distanceSq = sharpNode_t::DistanceSq( sp, to );
		if (distanceSq < nearestDistanceSq) {
			nearestDistanceSq = distanceSq;
			nearestCoord = to;
			foundSegment = i;
			foundNepper = cMath::Clamp01( scale );
		}
	} // for (int i = 0; i < CountSegment(); ++i)
	cAssert( nearestCoord != sharpNode_t::UNDEFINED() );

	if ( nepper ) {
		float  len = 0.0f;
		for (int i = 0; i < foundSegment; ++i) {
			len += Length( i );
		}
		const float  lnp = foundNepper * Length( foundSegment );
		*nepper = (len + lnp) / Length();
	}
	if ( nepperSegment ) {
		*nepperSegment = foundNepper;
	}
	if ( segment ) {
		*segment = foundSegment;
	}

	return nearestCoord;
}

template
comms::cLine< int, 1 >::sharpNode_t
comms::cLine< int, 1 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< int, 2 >::sharpNode_t
comms::cLine< int, 2 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< int, 3 >::sharpNode_t
comms::cLine< int, 3 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< int, 4 >::sharpNode_t
comms::cLine< int, 4 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< float, 1 >::sharpNode_t
comms::cLine< float, 1 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< float, 2 >::sharpNode_t
comms::cLine< float, 2 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< float, 3 >::sharpNode_t
comms::cLine< float, 3 >::NearestProjection( const node_t&, float*, float*, int* ) const;

template
comms::cLine< float, 4 >::sharpNode_t
comms::cLine< float, 4 >::NearestProjection( const node_t&, float*, float*, int* ) const;




/* @todo extend
template< class Type, int Dim  >
typename cLine< Type, Dim >::sharpNodes_t
cLine< Type, Dim >::Coplanar(
	const node_t&  a,
	const node_t&  b,
	const node_t&  c
) const {
	sharpNodes_t  r;

	throw comms::Exception( "@todo" );

	return r;
}
*/




template< class Type, int Dim  >
comms::cLine< Type, Dim >
comms::cLine< Type, Dim >::SortClockwise() const {

	const sharpNode_t&  c = Center();
	const sharpNode_t&  n = Normal();
	nodes_t  l = raw;
	const CompareClockwise  cf( c, n );
	l.Sort( cf );

	return cLine< Type, Dim >( l );
}

template
comms::cLine< int, 3 >
comms::cLine< int, 3 >::SortClockwise() const;

template
comms::cLine< float, 3 >
comms::cLine< float, 3 >::SortClockwise() const;




template< class Type, int Dim  >
void
comms::cLine< Type, Dim >::Subdivide( int quant ) {

	if (quant < 2) { return; }
	if ( IsEmpty() ) { return; }
	if ( !IsValid() ) { return; }

    float  step = 1.0f / ( float )quant;
    cMath::ClampLow( step, ::std::numeric_limits< float >::epsilon() );
	nodes_t  r;
	for (int i = 0; i < CountSegment(); ++i) {
		const sharpNode_t&  a = ( *this )[ i     ];
		const sharpNode_t&  b = ( *this )[ i + 1 ];
		const cLine< float, Dim >  segment( a, b );
        r.Add( a );
        for ( float t = step; t < 1.0f; t += step ) {
			const sharpNode_t  point = segment.Point( t );
			r.Add( sharpNode_t_to_node_t<Type, Dim>( point ) );
		} // for (float t = step; ...
    } // for (int i = 0; i < CountSegment(); ++i)
    r.Add( GetLast() );

	raw = r;
}

template
void
comms::cLine< int, 3 >::Subdivide( int );

template
void
comms::cLine< float, 3 >::Subdivide( int );
