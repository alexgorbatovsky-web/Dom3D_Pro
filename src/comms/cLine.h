#pragma once

// @param Type  The type of line for store. See 'cLine::node_t'.
// @param Dim   The dimension of line. See 'cVec'.
template< class Type, int Dim  >
class cLine {
public:
	typedef cLine< Type, Dim >    line_t;
	typedef cVec< Type, Dim >     node_t;
	typedef cVec< float, Dim >    sharpNode_t;
	typedef cList< node_t >       nodes_t;
	typedef cList< sharpNode_t >  sharpNodes_t;
	typedef cVec< float, Dim >    direction_t;
	typedef cList< float >        distances_t;

public:
	cLine();

	// # Dublicates are remains.
	cLine( const nodes_t& );
	cLine( const node_t& a, const node_t& b );

	virtual ~cLine();

	cLine& operator=( const cLine& );

	// # Dublicates are remains.
	void AddToStart( const node_t& );
	void Add( const node_t& );
	void AddToEnd( const node_t& node ) { Add( node ); }
	cLine& operator+=( const node_t& );

	void Transform( const cMat4& );
    void TransformNode( int i, const cMat4& );

	void Clear();

	const node_t& GetFirst() const;
	const node_t& GetLast()  const;
	const node_t& operator[]( int i ) const;
	node_t& operator[]( int i );

	cLine< Type, Dim >  GetFirstSegment() const;
	cLine< Type, Dim >  GetLastSegment()  const;
	cLine< Type, Dim >  GetSegment( int segment ) const;

	// @return The 'node_t' has included to this line.
	bool Has( const node_t& ) const;

	// @return The 'line_t' has included to this line. Revert and search when
	//         direct 'line_t' is not found.
	bool Has( const line_t& ) const;

	int CountNode() const;
	int CountSegment() const;

	// @return  Revert this line and return as new.
	line_t Reverse() const;

	// @return List of distances from 'node_t' to this line.
	//         Return undefined value when line IsInvalid().
	//         Calculate it by Distances().
	float DistanceAvg( const node_t& ) const;
	float DistanceMax( const node_t& ) const;
	float DistanceMin( const node_t& ) const;
	distances_t Distances( const node_t& ) const;

	// @return Direction from first node to last node (as vector).
	direction_t Direction() const;

	float Length() const;
	float Length( int segment ) const;

	sharpNode_t Center() const;

	// @return Normal (for build surface) by all points of the line.
	// @todo optimize  Add 'allowFor' param for fast calc normal by some points.
	sharpNode_t Normal() const;

	// @return Coord of point on the line. 0.0 - begin line, 
	//         0.5 - middle line, 1.0 - end line.
	// @param t Diapason is [ 0.0; 1.0 ].
	// @throw comms::Exception When 't' is out of diapason or line IsEmpty() or !Isvalid().
	// @see NearestProjection() for calc 't' by 'node_t'.
	node_t Point( float t ) const;
	// Point() with 1 segment. Fast. Precision result.
	static sharpNode_t Point( const sharpNode_t& a, const sharpNode_t& b, float t );
	static cVec3 Point( const cVec3& a, const cVec3& b, float t );

	// @return Nearest coord (projection of 'point') on the segment of line.
	// @param nepper  Result for Point() by all line.
	// @param nepperSegment  Result for Point() by found segment.
	// @param segment  Found segment.
	// @see Point() for calc 'node_t' by 'nepper'.
	sharpNode_t NearestProjection(
		const node_t&  point,
		float*         nepper = nullptr,
		float*         nepperSegment = nullptr,
		int*           segment = nullptr
	) const;

	bool operator==( const cLine& ) const;
	bool operator!=( const cLine& ) const;

	bool IsEmpty() const;
	bool IsValid() const;

	// @return The line is closed.
	bool IsRing() const;

	cLine WithoutOrderedDublicate() const;

	// @return Line which all points to one surface (coplanar).
	// Surface intersect points 'a', 'b', 'c' when defined.
	// When points are undefined using first points from the lines.
	/* @todo extend  sharpNodes_t Coplanar(
		const node_t&  a = node_t::UNDEFINED(),
		const node_t&  b = node_t::UNDEFINED(),
		const node_t&  c = node_t::UNDEFINED()
	) const;
	*/

	// @return All nodes of line sorted by clockwise order.
	// If line is IsRing(), when result line IsRing() also.
	// @example 2D
	//     a----b   e
	//         /    |
	//        /     |
	//       c------d
	// after sort
	//     a----b---e
	//              |
	//              |
	//       c------d
	cLine SortClockwise() const;

	// Divide the line on 'quant' parts.
	// By analogy with QuadQuantSubd().
	void Subdivide( int quant );

	static const cLine< Type, Dim >&  EMPTY();


private:
	// For SortClockwise().
	struct CompareClockwise : public cList< node_t >::CompareFunctor {
		const sharpNode_t  center;
		const sharpNode_t  normal;

		CompareClockwise( const sharpNode_t& c, const sharpNode_t& n ) :
			center( c ), normal( n )
		{}

		virtual bool operator()( const node_t& a, const node_t& b ) const;
	};

	// @see Has( line_t )
	bool HasDirect( const line_t& ) const;


private:
	nodes_t  raw;
};




typedef cLine< float, 2 >  cLineF2;
typedef cLine< float, 3 >  cLineF3;




template< class Type, int Dim  >
cLine< Type, Dim >::cLine() {
}




template< class Type, int Dim  >
cLine< Type, Dim >::cLine( const nodes_t& nodes ) {
	raw = nodes;
}




template< class Type, int Dim  >
cLine< Type, Dim >::cLine( const node_t& a, const node_t& b ) {
	Add( a );
	Add( b );
}




template< class Type, int Dim  >
cLine< Type, Dim >::~cLine() {
}




template< class Type, int Dim  >
cLine< Type, Dim >&
cLine< Type, Dim >::operator=( const cLine& b ) {
	raw = b.raw;
	return *this;
}




template< class Type, int Dim  >
void
cLine< Type, Dim >::AddToStart( const node_t& node ) {
	raw.Insert( 0, node );
}




template< class Type, int Dim  >
void
cLine< Type, Dim >::Add( const node_t& node ) {
	raw.Add( node );
}




template< class Type, int Dim  >
cLine< Type, Dim >&
cLine< Type, Dim >::operator+=( const node_t& node ) {
	Add( node );
	return *this;
}




template< class Type, int Dim  >
void
cLine< Type, Dim >::Transform( const cMat4& m ) {
	for (int i = 0; i < raw.Count(); ++i) {
        TransformNode( i, m );
	}
}




template< class Type, int Dim  >
void
cLine< Type, Dim >::TransformNode( int i, const cMat4& m ) {
    raw[ i ].TransformCoordinate( m );
}




template< class Type, int Dim  >
void
cLine< Type, Dim >::Clear() {
	raw.Free();
}




template< class Type, int Dim  >
const typename cLine< Type, Dim >::node_t&
cLine< Type, Dim >::GetFirst() const {
	return raw.GetFirst();
}




template< class Type, int Dim  >
const typename cLine< Type, Dim >::node_t&
cLine< Type, Dim >::GetLast() const {
	return raw.GetLast();
}




template< class Type, int Dim  >
const typename cLine< Type, Dim >::node_t&
cLine< Type, Dim >::operator[]( int i ) const {
	return raw[ i ];
}




template< class Type, int Dim  >
typename cLine< Type, Dim >::node_t&
cLine< Type, Dim >::operator[]( int i ) {
	return raw[ i ];
}




template< class Type, int Dim  >
cLine< Type, Dim >
cLine< Type, Dim >::GetFirstSegment() const {
	return GetSegment( 0 );
}




template< class Type, int Dim  >
cLine< Type, Dim >
cLine< Type, Dim >::GetLastSegment() const {
	return GetSegment( CountSegment() - 1 );
}




template< class Type, int Dim  >
cLine< Type, Dim >
cLine< Type, Dim >::GetSegment( int segment ) const {
	if ((segment < 0) || (segment >= CountSegment()) ) {
		cAssertM(0, "Out of line." );
	}
	cLine< Type, Dim >  r;
	r += raw[ segment     ];
	r += raw[ segment + 1 ];
	return r;
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::Has( const node_t& search ) const {
    return (raw.IndexOf( search ) != -1);
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::Has( const line_t& search ) const {
	const bool  r = HasDirect( search );
	return r ? true : HasDirect( search.Reverse() );
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::HasDirect( const line_t& search ) const {
	if (search.CountNode() < 2) {
		cAssertM(0, "The line for search... is not line :(" );
	}
	const node_t&  sf = search.GetFirst();
	for (int i = 0; i < raw.Count(); ++i) {
		const node_t&  a = raw[ i ];
		if (a != sf) { continue; }
		bool  yes = true;
		for (int j = 1; j < search.CountNode(); ++j) {
			const int  k = i + j;
			if (k < raw.Count()) {
				const node_t&  a = raw[ i + j ];
				const node_t&  b = search[ j ];
				if (a == b) { continue; }
			}
			yes = false;
		}
		if ( yes ) { return true; }
	} // for (int i = 0; i < raw.Count(); ++i)

	return false;
}




template< class Type, int Dim  >
int
cLine< Type, Dim >::CountNode() const {
    return raw.Count();
}



template< class Type, int Dim  >
int
cLine< Type, Dim >::CountSegment() const {
	return (CountNode() < 2) ? 0 : (CountNode() - 1);
}




template< class Type, int Dim  >
typename cLine< Type, Dim >::line_t
cLine< Type, Dim >::Reverse() const {
	nodes_t  reverseRaw = raw;
	reverseRaw.Reverse();
	return line_t( reverseRaw );
}




template< class Type, int Dim  >
float
cLine< Type, Dim >::DistanceAvg( const node_t& point ) const {
	float  r = 0.0f;
	const distances_t  distances = Distances( point );
	for (int i = 0; i < distances.Count(); ++i) {
		r += distances[ i ];
	}
    return r / (float)distances.Count();
}




template< class Type, int Dim  >
float
cLine< Type, Dim >::DistanceMax( const node_t& point ) const {
	float  r = -FLT_MAX;
	const distances_t  distances = Distances( point );
	for (int i = 0; i < distances.Count(); ++i) {
		const float  d = distances[ i ];
		if (d > r) { r = d; }
	}
    return r;
}




template< class Type, int Dim  >
float
cLine< Type, Dim >::DistanceMin( const node_t& point ) const {
	float  r = FLT_MAX;
	const distances_t  distances = Distances( point );
	for (int i = 0; i < distances.Count(); ++i) {
		const float  d = distances[ i ];
		if (d < r) { r = d; }
	}
    return r;
}




template< class Type, int Dim  >
typename cLine< Type, Dim >::direction_t
cLine< Type, Dim >::Direction() const {
	const direction_t  r = GetLast() - GetFirst();
	return r;
}




template< class Type, int Dim  >
typename cLine< Type, Dim >::sharpNode_t
cLine< Type, Dim >::Center() const {
	sharpNode_t  r( 0.0f );
	for (int i = 0; i < CountNode(); ++i) {
		r += ( *this )[ i ];
	}
	r /= (float)CountNode();
	return r;
}




template< class Type, int Dim  >
typename cLine< Type, Dim >::sharpNode_t
cLine< Type, Dim >::Point( const sharpNode_t& a, const sharpNode_t& b, float t ) {
	return (b - a) * t + a;
}




template< class Type, int Dim  >
cVec3
cLine< Type, Dim >::Point( const cVec3& a, const cVec3& b, float t ) {
	return (b - a) * t + a;
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::operator==( const cLine& b ) const {
	if (CountNode() != b.CountNode()) { return false; }
	for (int i = 0; i < CountNode(); ++i) {
		const node_t&  na = ( *this )[ i ];
		const node_t&  nb = b[ i ];
		if (na != nb) { return false; }
	}
	return true;
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::operator!=( const cLine& b ) const {
	return !(*this == b);
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::IsEmpty() const {
	return (CountNode() == 0);
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::IsValid() const {
	return (CountNode() == 0) || (CountSegment() > 0);
}




template< class Type, int Dim  >
bool
cLine< Type, Dim >::IsRing() const {
	return ((CountSegment() > 1) && (GetFirst() == GetLast()));
}




template< class Type, int Dim  >
cLine< Type, Dim >
cLine< Type, Dim >::WithoutOrderedDublicate() const {
	cLine r;
	for (int i = 0; i < CountNode(); ++i) {
		const node_t&  node = ( *this )[ i ];
		if ( r.IsEmpty() || (node != r.GetLast()) ) {
			r.Add( node );
		}
	}
	return r;
}




template< class Type, int Dim >
const cLine< Type, Dim >&
cLine< Type, Dim >::EMPTY() {
    static const cLine< Type, Dim >  r;
	return r;
}



template< class Type, int Dim  >
bool
cLine< Type, Dim >::CompareClockwise::operator()(
	const node_t&  a,
	const node_t&  b
) const {
	// Algorithm.
	// @thanks http://stackoverflow.com/a/14371081/963948
	// We have the center C and the normal n. To determine whether point B is
	// clockwise or counterclockwise from point A,
	// calculate dot( n, cross( A-C, B-C ) ). If the result is positive, B is
	// counterclockwise from A; if it's negative, B is clockwise from A.
	const sharpNode_t  cross = sharpNode_t::Cross( a - center, b - center );
	const float          dot = sharpNode_t::Dot( normal, cross );
	return (dot < 0);
}




template< class Type, int Dim  >
::std::ostream& operator<<( ::std::ostream& out,  const cLine< Type, Dim >& l ) {
	out << "[";
	for (int i = 0; i < l.CountNode(); ++i) {
		const bool needSeparator = ((i + 1) != l.CountNode());
		out << l[ i ] << (needSeparator ? ", " : "");
	}
	out << "]";
	return out;
}
