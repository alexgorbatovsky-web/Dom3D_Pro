#pragma once

class dMat2 {
public:
	dVec2 m[2];

	dMat2();
	dMat2(const dVec2 &row0, const dVec2 &row1);
	dMat2(const double m00, const double m01,
			 const double m10, const double m11);

	dMat2 & Copy(const double *pSrc);
	dMat2 & CopyTransposed(const double *pSrc);

    const dVec2 & operator [] ( const int row ) const;
    dVec2 & operator [] ( const int row );
    double operator () ( const int row, const int col ) const;
    double & operator () ( const int row, const int col );

	dMat2 operator - () const;

	dMat2 & operator += (const dMat2 &);
	dMat2 & operator -= (const dMat2 &);
	dMat2 & operator *= (const dMat2 &);
	dMat2 & operator *= (const double);
	dMat2 & operator /= (const double);
	void  Transform(dVec2& v);

	dMat2 operator + (const dMat2 &) const;
	dMat2 operator - (const dMat2 &) const;
	dMat2 operator * (const dMat2 &) const;
	dMat2 operator * (const double) const;
	friend dMat2 operator * (const double, const dMat2 &);
	dMat2 operator / (const double) const;

	bool operator == (const dMat2 &) const;
	bool operator != (const dMat2 &) const;
	bool Equals(const dMat2 &, const double Eps = cMath::Epsilon) const;

	double Trace() const;
	double Determinant() const;
	bool Invert();

	static const dMat2 Zero;
	static const dMat2 Identity;

	double * ToPtr();
	const double * ToPtr() const;
};

// dMat2.ctor : ()
inline dMat2::dMat2() {
}

// dMat2.ctor : (const dVec2 &, const dVec2 &)
inline dMat2::dMat2(const dVec2 &row0, const dVec2 &row1) {
	m[0] = row0;
	m[1] = row1;
}

// dMat2.ctor : (const double, const double,
//					const double, const double)
inline dMat2::dMat2(const double m00, const double m01,
						  const double m10, const double m11) {
	m[0].Set(m00, m01);
	m[1].Set(m10, m11);
}

// dMat2::operator [] : const dVec2 & (const int) const
inline const dVec2 & dMat2::operator [] ( const int Row ) const {
    cAssert( Row >= 0 && Row < 2 );
    return m[ Row ];
}

// dMat2::operator [] : dVec2 & (const int)
inline dVec2 & dMat2::operator [] ( const int Row ) {
    cAssert( Row >= 0 && Row < 2 );
    return m[ Row ];
}

// dMat2::operator () : double (const int, const int) const
inline double dMat2::operator () ( const int Row, const int Col ) const {
    cAssert( Row >= 0 && Row < 2 );
    cAssert( Col >= 0 && Col < 2 );
    return m[ Row ][ Col ];
}

// dMat2::operator () : double & (const int, const int)
inline double & dMat2::operator () ( const int Row, const int Col ) {
    cAssert( Row >= 0 && Row < 2 );
    cAssert( Col >= 0 && Col < 2 );
    return m[ Row ][ Col ];
}

inline void dMat2::Transform(dVec2& _v){
	dVec2 v=_v;
	_v.x=v.x*m[0][0]+v.y*m[1][0];
	_v.y=v.x*m[0][1]+v.y*m[1][1];	
}




::std::ostream& operator<<( ::std::ostream&,  const dMat2& );
