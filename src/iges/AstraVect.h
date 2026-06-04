/******************************** AstraVect.h *****************************/
#ifndef _LIB_ASTRA_VECT_AG_H
#define _LIB_ASTRA_VECT_AG_H

extern double ROP[36];

int ASVOPM(double* RES,int* IU,\
		   double* P1,double* P2,double* P3,double* P4,double* P5,double* P6,double* P7,double* P8);
int VOPA(double* RES,int IPR, double* V);
int VOPB(double* RES,int IPR, double* V1, double* V2);
int VOPC(double* RES,int IPR, double* V1, double* V2, double* V3);

#endif // _LIB_ASTRA_VECT_AG_H
