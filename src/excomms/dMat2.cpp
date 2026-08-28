#include "stdafx.h"
namespace comms{

const dMat2 dMat2::Zero(0.0f, 0.0f, 0.0f, 0.0f);
const dMat2 dMat2::Identity(1.0f, 0.0f, 0.0f, 1.0f);
bool dMat2::Invert(){
	dMat2 M=*this;
	double det=m[0][0]*m[1][1]-m[1][0]*m[0][1];
	if(det!=0){
		m[0][0]=M.m[1][1]/det;
		m[1][1]=M.m[0][0]/det;
		m[0][1]=-M.m[0][1]/det;
		m[1][0]=-M.m[1][0]/det;
		return true;
	}
	return false;
}
dMat2 & dMat2::operator += (const dMat2 & mat){
	m[0][0]+=mat.m[0][0];
	m[0][1]+=mat.m[0][1];
	m[1][0]+=mat.m[1][0];
	m[1][1]+=mat.m[1][1];
	return *this;
}
dMat2 & dMat2::operator -= (const dMat2 & mat){
	m[0][0]-=mat.m[0][0];
	m[0][1]-=mat.m[0][1];
	m[1][0]-=mat.m[1][0];
	m[1][1]-=mat.m[1][1];
	return *this;
}
dMat2 & dMat2::operator *= (const dMat2 & mat){
	double m00=m[0][0]*mat.m[0][0]+m[0][1]*mat.m[1][0];
	double m01=m[0][0]*mat.m[0][1]+m[0][1]*mat.m[1][1];
	double m10=m[1][0]*mat.m[0][0]+m[1][1]*mat.m[1][0];
	double m11=m[1][0]*mat.m[0][1]+m[1][1]*mat.m[1][1];
	m[0][0]=m00;
	m[0][1]=m01;
	m[1][0]=m10;
	m[1][1]=m11;
	return *this;
}
dMat2 & dMat2::operator *= (const double v){
	m[0][0]*=v;
	m[0][1]*=v;
	m[1][0]*=v;
	m[1][1]*=v;
	return *this;
}
dMat2 & dMat2::operator /= (const double v){
	m[0][0]/=v;
	m[0][1]/=v;
	m[1][0]/=v;
	m[1][1]/=v;
	return *this;
}




::std::ostream& operator<<( ::std::ostream& out, const dMat2& m ) {
    out << "[" << m( 0, 0 ) << "," << m( 0, 1 ) <<
          ", " << m( 1, 0 ) << "," << m( 1, 1 ) << "]";
    return out;
}


} // comms

