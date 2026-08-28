#pragma once

class PYCALL cMat2 {
public:
	cVec2 m[2];

	cMat2();
	cMat2(const cVec2 &row0, const cVec2 &row1);
	cMat2(const float m00, const float m01,
			 const float m10, const float m11);

	cMat2(const cMat2& v) { memcpy(&m, &v.m, sizeof(m)); }

	cMat2 & Copy(const float *pSrc);
	cMat2 & CopyTransposed(const float *pSrc);

    const cVec2 & operator [] ( const int row ) const;
    cVec2 & operator [] ( const int row );
    float operator () ( const int row, const int col ) const;
    float & operator () ( const int row, const int col );

	cMat2 operator - () const;

	cMat2 & operator += (const cMat2 &);
	cMat2 & operator -= (const cMat2 &);
	cMat2 & operator *= (const cMat2 &);
	cMat2 & operator *= (const float);
	cMat2 & operator /= (const float);
	void  Transform(cVec2& v);

	cMat2 operator + (const cMat2 &) const;
	cMat2 operator - (const cMat2 &) const;
	cMat2 operator * (const cMat2 &) const;
	cMat2 operator * (const float) const;
	friend cMat2 operator * (const float, const cMat2 &);
	cMat2 operator / (const float) const;

	bool operator == (const cMat2 &) const;
	bool operator != (const cMat2 &) const;
	bool Equals(const cMat2 &, const float Eps = cMath::Epsilon) const;

	float Trace() const;
	float Determinant() const;
	bool Invert();

	static const cMat2 Zero;
	static const cMat2 Identity;

	float * ToPtr();
	const float * ToPtr() const;
};

// cMat2.ctor : ()
inline cMat2::cMat2() {
}

// cMat2.ctor : (const cVec2 &, const cVec2 &)
inline cMat2::cMat2(const cVec2 &row0, const cVec2 &row1) {
	m[0] = row0;
	m[1] = row1;
}

// cMat2.ctor : (const float, const float,
//					const float, const float)
inline cMat2::cMat2(const float m00, const float m01,
						  const float m10, const float m11) {
	m[0].Set(m00, m01);
	m[1].Set(m10, m11);
}

// cMat2::operator [] : const cVec2 & (const int) const
inline const cVec2 & cMat2::operator [] ( const int Row ) const {
    cAssert( Row >= 0 && Row < 2 );
    return m[ Row ];
}

// cMat2::operator [] : cVec2 & (const int)
inline cVec2 & cMat2::operator [] ( const int Row ) {
    cAssert( Row >= 0 && Row < 2 );
    return m[ Row ];
}

// cMat2::operator () : float (const int, const int) const
inline float cMat2::operator () ( const int Row, const int Col ) const {
    cAssert( Row >= 0 && Row < 2 );
    cAssert( Col >= 0 && Col < 2 );
    return m[ Row ][ Col ];
}

// cMat2::operator () : float & (const int, const int)
inline float & cMat2::operator () ( const int Row, const int Col ) {
    cAssert( Row >= 0 && Row < 2 );
    cAssert( Col >= 0 && Col < 2 );
    return m[ Row ][ Col ];
}

inline void cMat2::Transform(cVec2& _v){
	cVec2 v=_v;
	_v.x=v.x*m[0][0]+v.y*m[1][0];
	_v.y=v.x*m[0][1]+v.y*m[1][1];	
}




::std::ostream& operator<<( ::std::ostream&,  const cMat2& );
