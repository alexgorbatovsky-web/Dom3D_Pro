#include "stdafx.h"
namespace comms{

const cMat2 cMat2::Zero(0.0f, 0.0f, 0.0f, 0.0f);
const cMat2 cMat2::Identity(1.0f, 0.0f, 0.0f, 1.0f);
bool cMat2::Invert(){
	cMat2 M=*this;
	float det=m[0][0]*m[1][1]-m[1][0]*m[0][1];
	if(det!=0){
		m[0][0]=M.m[1][1]/det;
		m[1][1]=M.m[0][0]/det;
		m[0][1]=-M.m[0][1]/det;
		m[1][0]=-M.m[1][0]/det;
		return true;
	}
	return false;
}
cMat2 & cMat2::operator += (const cMat2 & mat){
	m[0][0]+=mat.m[0][0];
	m[0][1]+=mat.m[0][1];
	m[1][0]+=mat.m[1][0];
	m[1][1]+=mat.m[1][1];
	return *this;
}
cMat2 & cMat2::operator -= (const cMat2 & mat){
	m[0][0]-=mat.m[0][0];
	m[0][1]-=mat.m[0][1];
	m[1][0]-=mat.m[1][0];
	m[1][1]-=mat.m[1][1];
	return *this;
}
cMat2 & cMat2::operator *= (const cMat2 & mat){
	float m00=m[0][0]*mat.m[0][0]+m[0][1]*mat.m[1][0];
	float m01=m[0][0]*mat.m[0][1]+m[0][1]*mat.m[1][1];
	float m10=m[1][0]*mat.m[0][0]+m[1][1]*mat.m[1][0];
	float m11=m[1][0]*mat.m[0][1]+m[1][1]*mat.m[1][1];
	m[0][0]=m00;
	m[0][1]=m01;
	m[1][0]=m10;
	m[1][1]=m11;
	return *this;
}
cMat2 & cMat2::operator *= (const float v){
	m[0][0]*=v;
	m[0][1]*=v;
	m[1][0]*=v;
	m[1][1]*=v;
	return *this;
}
cMat2 & cMat2::operator /= (const float v){
	m[0][0]/=v;
	m[0][1]/=v;
	m[1][0]/=v;
	m[1][1]/=v;
	return *this;
}




::std::ostream& operator<<( ::std::ostream& out, const cMat2& m ) {
    out << "[" << m( 0, 0 ) << "," << m( 0, 1 ) <<
          ", " << m( 1, 0 ) << "," << m( 1, 1 ) << "]";
    return out;
}


} // comms

