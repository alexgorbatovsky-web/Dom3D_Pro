/////////	начало 19.06.2000
////////////	реализация функций Астры на C
/////////////////////////////////////////////////////////
#include "defines.h"

double ROP[36];

#define IU(x) IU[x-1]
#define ROP(x) ROP[x-1]
#define P1(x) P1[x-1]
#define P2(x) P2[x-1]
#define P3(x) P3[x-1]
#define P4(x) P4[x-1]
#define P5(x) P5[x-1]
#define P6(x) P6[x-1]
#define P7(x) P7[x-1]
#define P8(x) P8[x-1]
#define RES(x) RES[x-1]


int ASVOPM(double* RES,int* IU,\
		   double* P1,double* P2,double* P3,double* P4,double* P5,double* P6,double* P7,double* P8)
///////PEAЛИЗAЦИЯ BEKTOPHЫX OПEPAЦИЙ
/////BEPCИЯ: 07 (25.01.97)
{
	double X,Y,Z,X1,Y1,Z1,A,B,C;
    int JJ,N,NP,NN,IB,KOP,IRES,IOP1,IOP2,I,J,KB;

//     *         'OШИБKA B KOДE OПEPAЦИИ  102
//     *         'HOPMИPOBKA HУЛЬ-BEKTOPA 103
//     *         'HEBEPEH HOMEP 1-ГO OПEPAHДA 104
//     *         'HEBEPEH HOMEP 2-ГO OПEPAHДA 105
//     *         'ДEЛEHИE HA HУЛЬ 106
      N=IU(1);
      if(N<=0){
		  message_error_("OШИБKA B ЧИCЛE ПAPAMETPOB!");
		  return BAD;
	  }
      NP=0;
      for(I=1; I<=N; I++){
		JJ=IU(I+1);
		NP=NP+__IntAbs(JJ);
	  }
      if(NP<N){
		  message_error_("OШИБKA B ЧИCЛE ПAPAMETPOB!");
		  return BAD;
	  }
      if(NP>8){
		  message_error_("OШИБKA B ЧИCЛE ПAPAMETPOB!");
		  return BAD;
	  }
      NN=NP*3+12;
      for(I=13; I<=NN; I++)
		ROP(I)=7777777777;
//.....ПEPECЫЛKA OПEPAHДOB
      JJ=IU(2)*3;
      NN=__IntAbs(JJ);
      IB=NN;
      if(IU(2)<0) 
		  IB=1;
	for(I=1; I<=IB; I++)
		ROP(12+I)=P1(I);
	if(N==1) goto Lb10;
	J=12+NN;
	JJ=IU(3)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(3)<0)
		  IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P2(I);
	if(N==2) goto Lb10;
	J=J+NN;
	JJ=IU(4)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(4)<0)
		  IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P3(I);
	if(N==3) goto Lb10;
	J=J+NN;
	JJ=IU(5)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(5)<0) 
		IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P4(I);
	if(N==4) goto Lb10;
	J=J+NN;
	JJ=IU(6)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(6)<0)
		IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P5(I);
	if(N==5) goto Lb10;
	J=J+NN;
	JJ=IU(7)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(7)<0) 
		IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P6(I);
	if(N==6) goto Lb10;
	J=J+NN;
	JJ=IU(8)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(8)<0) 
		IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P7(I);
	if(N==7) goto Lb10;
	J=J+NN;
	JJ=IU(9)*3;
	NN=__IntAbs(JJ);
	IB=NN;
	if(IU(9)<0) 
		IB=1;
	for(I=1; I<=IB; I++)
		ROP(J+I)=P8(I);
Lb10: J=N+2;
	KB=NP*3+10;
//.....BЫДEЛEHИE KOДA OПEPAЦИИ, УKAЗATEЛEЙ PEЗ-TA, OПEPAHДOB
Lb11: IB=IU(J);
	if(IB<0) 
		IB=-IB;
	KOP=IB/100;
	IRES=IB-KOP*100;
	IB=IU(J+1)*3;
	IOP1=IB/100;
	IOP2=IB-IOP1*100+1;
	IOP1=IOP1+1;
	if(IOP1<=0){
	  message_error_("HOPMИPOBKA HУЛЬ-BEKTOPA!");
	  return BAD;
	}
	if(IOP1>KB){
	  message_error_("HEBEPEH HOMEP 1-ГO OПEPAHДA!");
	  return BAD;
	}
	if(IOP2<=0){
	  message_error_("HEBEPEH HOMEP 2-ГO OПEPAHДA!");
	  return BAD;
	}
	if(IOP2>KB){
	  message_error_("HEBEPEH HOMEP 2-ГO OПEPAHДA!");
	  return BAD;
	}
//		  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20
//	GOTO(21,22,27,28,29,26,12,27,28,29,22,27,28,29,29,29,29,18,19,99),KOP
	 switch (KOP){
		case 1:
			goto Lb21;
		case 2:
			goto Lb22;
		case 3:
			goto Lb27;
		case 4:
			goto Lb28;
		case 5:
			goto Lb29;
		case 6:
			goto Lb26;
		case 7:
			goto Lb12;
		case 8:
			goto Lb27;
		case 9:
			goto Lb28;
		case 10:
			goto Lb29;
		case 11:
			goto Lb22;
		case 12:
			goto Lb27;
		case 13:
			goto Lb28;
		case 14:
			goto Lb29;
		case 15:
			goto Lb29;
		case 16:
			goto Lb29;
		case 17:
			goto Lb29;
		case 18:
			goto Lb18;
		case 19:
			goto Lb19;
		case 20:
			goto Lb99;
		default:
			message_error_("OШИБKA B KOДE OПEPAЦИИ!");
			return BAD;
	 }
//.....CKAЛЯPHOE ПPOИЗBEДEHИE
Lb21: ROP(1)=ROP(IOP1)*ROP(IOP2)+ROP(IOP1+1)*ROP(IOP2+1)+ROP(IOP1+2)*ROP(IOP2+2);
	goto Lb30;
//.....ДEЛEHИE HA CKAЛЯP
Lb12: X=ROP(IOP2);
	if(X==0){
	  message_error_("ДEЛEHИE HA HУЛЬ!");
	  return BAD;
	}
	X=1/X;
	goto Lb15;
//.....УMHOЖEHИE HA CKAЛЯP
Lb26: X=ROP(IOP2);
Lb15: ROP(1)=ROP(IOP1)*X;
	ROP(2)=ROP(IOP1+1)*X;
	ROP(3)=ROP(IOP1+2)*X;
	goto Lb30;
//.....CЛOЖEHИE BEKTOPOB
Lb27: ROP(1)=ROP(IOP1)+ROP(IOP2);
	ROP(2)=ROP(IOP1+1)+ROP(IOP2+1);
	ROP(3)=ROP(IOP1+2)+ROP(IOP2+2);
	goto Lb23;
//.....BЫЧИTAHИE BEKTOPOB
Lb28: ROP(1)=ROP(IOP1)-ROP(IOP2);
	ROP(2)=ROP(IOP1+1)-ROP(IOP2+1);
	ROP(3)=ROP(IOP1+2)-ROP(IOP2+2);
	goto Lb23;
//.....BEKTOPHOE (ДВОЙНОЕ) ПPOИЗBEДEHИE
Lb29: X=ROP(IOP1);
	Y=ROP(IOP1+1);
	Z=ROP(IOP1+2);
	X1=ROP(IOP2);
	Y1=ROP(IOP2+1);
	Z1=ROP(IOP2+2);
	ROP(1)=Y*Z1-Y1*Z;
	ROP(2)=Z*X1-Z1*X;
	ROP(3)=X*Y1-X1*Y;
	if(KOP<15) goto Lb23;
	A=ROP(1);
	B=ROP(2);
	C=ROP(3);
	ROP(1)=B*Z-Y*C;
	ROP(2)=C*X-Z*A;
	ROP(3)=A*Y-X*B;
	if(KOP==15) goto Lb30;
	goto Lb24;
//.....ДВОЙНОЕ ВЕКТОРНОЕ ПРОИЗВЕДЕНИЕ (MOD(A)=1)
Lb18: X=ROP(IOP1)*ROP(IOP2)+ROP(IOP1+1)*ROP(IOP2+1)+ROP(IOP1+2)*ROP(IOP2+2);
	ROP(1)=ROP(IOP2)-ROP(IOP1)*X;
	ROP(2)=ROP(IOP2+1)-ROP(IOP1+1)*X;
	ROP(3)=ROP(IOP2+2)-ROP(IOP1+2)*X;
	goto Lb30;
Lb23: if(KOP<=5) goto Lb24;
	if(KOP<=10) goto Lb30;
	goto Lb24;
//.....MOДУЛЬ BEKTOPA
Lb22: if(IOP1==1) goto Lb24;
	ROP(1)=ROP(IOP1);
	ROP(2)=ROP(IOP1+1);
	ROP(3)=ROP(IOP1+2);
Lb24: X=1;
	for(I=1; I<=3; I++){
		Y=fabs(ROP(I));
		if(Y<=X)
			continue;
		X=Y;
	}
	Z=0;
	for (I=1; I<=3; I++){
		Y=ROP(I)/X;
		Y=Y*Y;
		Z=Z+Y;
	}
	Z=sqrt(Z)*X;
	if(KOP>10 && KOP<15) goto Lb25;
	if(KOP==17) goto Lb25;
	ROP(1)=Z;
	goto Lb30;
//.....HOPMИPOBAHИE BEKTOPA
Lb25: if(Z==0){
	  message_error_("HOPMИPOBKA HУЛЬ-BEKTOPA!");
	  return BAD;
	}
	ROP(1)=ROP(1)/Z;
	ROP(2)=ROP(2)/Z;
	ROP(3)=ROP(3)/Z;
	goto Lb30;
//.....ПЕРЕСЫЛКА 1-ГО ОПЕРАНДА
Lb19: ROP(1)=ROP(IOP1);
	ROP(2)=ROP(IOP1+1);
	ROP(3)=ROP(IOP1+2);
//.....ЗAHECEHИE PEЗУЛЬTATA
Lb30: if(IRES==0) goto Lb31;
	if(IU(J)<0) goto Lb33;
	IRES=IRES*3+1;
Lb33: if(IRES>12) goto Lb32;
	ROP(IRES)=ROP(1);
	if(KOP<=5 || KOP==16) goto Lb31;
	ROP(IRES+1)=ROP(2);
	ROP(IRES+2)=ROP(3);
	goto Lb31;
Lb32: IRES=IRES-12;
	RES(IRES)=ROP(1);
	if(KOP<=5 || KOP==16) goto Lb31;
	RES(IRES+1)=ROP(2);
	RES(IRES+2)=ROP(3);
Lb31: J=J+2;
	goto Lb11;


Lb99: return OK;
}

////*********************************************
//
//*********************************************
	int VOPA(double* RES,int IPR, double* V)
//*****ОПЕРАЦИИ НАД ОДНИМ ВЕКТОРОМ
//*****ВЕРСИЯ: 01 (25.01.97)
{
	 if(IPR<0 || IPR>4){
		 message_error_("ПРИЗНАК ОПЕРАЦИИ НЕ 1-4.\nVOPA");
		return BAD;
	 }
	int K[4]={1,8,14,20};
	int IU[28]={2,1,-1,604,405,2000,0,
				1,1,204,400,2000,0,1,
				1,1104,400,2000,0,2,1,
				-1,1100,400,604,5,2000,0};

	int I=K[IPR-1];
	double Y[3]={7777777777., 0, 0};
	double V2[3]={-1., 0, 0};
	return ASVOPM(RES, &IU[I-1],V, V2,Y,Y,Y,Y,Y,Y);
}
//****************************************************************
//
//****************************************************************
int VOPB(double* RES,int IPR, double* V1, double* V2)
//*****OПEPAЦИИ HAД ДВУМЯ BEKTOPAMИ
//*****BEPCИЯ: 01 (25.01.97)
{
	 if(IPR<0 || IPR>14){
		 message_error_("ПРИЗНАК ОПЕРАЦИИ НЕ 1-14.\nVOPB");
		return BAD;
	 }
	int K[14]={1,8,15,22,29,36,43,50,57,67,74,81,91,98};
	int IU[106]={2,1,1,104,405,2000,0,      2,1,1,1004,405,2000,0,
				2,1,1,504,405,2000,0,      2,1,1,1404,405,2000,0,
				2,1,1,904,405,2000,0,      2,1,1,404,405,2000,0,
				2,1,1,1304,405,2000,0,     2,1,1,804,405,2000,0,
				3,1,1,-1,800,405,604,6,2000,0,
				2,1,1,304,405,2000,0,      2,1,1,1204,405,2000,0,
				3,1,1,-1,1200,405,604,6,2000,0,
				2,1,-1,604,405,2000,0,     2,1,-1,1100,400,604,5,2000,0};
	double Y[3]={7777777777., 0,0}; 
    int I=K[IPR-1];
	double V3[3]={-1., 0, 0};
	return ASVOPM(RES, &IU[I-1],V1,V2, V3,Y,Y,Y,Y,Y);
}

//****************************************************************
//
//****************************************************************

int VOPC(double* RES,int IPR, double* V1, double* V2, double* V3)
//*****OПEPAЦИИ HAД ТРЕМЯ BEKTOPAMИ
//*****BEPCИЯ: 01 (25.01.97)
{
	 if(IPR<0 || IPR>15){
		 message_error_("ПРИЗНАК ОПЕРАЦИИ НЕ 1-15.\nVOPC");
		return BAD;
	 }
	int IU[152]={3,1,1,1,1000,405,104,6,2000,0, 
             3,1,1,1,1000,405,1004,6,2000,0,
             3,1,1,1,1000,405,504,6,2000,0,
             3,1,1,1,1000,405,1404,6,2000,0,
             3,1,1,1,800,405,104,6,2000,0,
             3,1,1,1,800,405,1004,6,2000,0,
             3,1,1,1,800,405,504,6,2000,0,
             3,1,1,1,800,405,1404,6,2000,0,
             3,1,1,1,900,405,104,6,2000,0,
             3,1,1,1,900,405,1004,6,2000,0,
             3,1,1,1,900,405,504,6,2000,0,
             3,1,1,1,900,405,1404,6,2000,0,
             3,1,1,-1,1400,405,604,6,2000,0,
             3,1,1,-1,1100,500,600,6,804,4,2000,0,
             3,1,1,-1,600,506,804,4,2000,0};
	double Y[3]={7777777777., 0,0}; 

	int I=10*IPR-9;
	if(IPR==15)
		I=I+2;
    return ASVOPM(RES, &IU[I-1],V1,V2,V3,Y,Y,Y,Y,Y);
      
}
