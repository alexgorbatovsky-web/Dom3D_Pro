////////////	реализация функций Астры на C
/////////	начало 16.07.2000
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

#define A(x,y,z) A[x-1+14*(y-1)+14*L*(z-1)]
#define RR(x) RR[x-1]
#define LR(x) LR[x-1]

extern int STAFM2(int IPR, int N, int LR[4], double D, double* A );
extern int VTKM(double RES[39], int KU[4],double* AU, double T);
extern int WFM(double* FG, double T,int L, int IX);

BOOL VTPM(double RES[24], int KU[3], double AU[56], double UV[2]);


//****************************************************************
//   Расчет коэффициентов Surface
//****************************************************************
//	SUBROUTINE STAPM(L,M,D,A,*,XX)
//*****РАСЧЕТ КОЭФФИЦ. СПЛАЙН-ПОВЕРХНОСТИ
//*****BEPCИЯ: 07 (17.11.94)
BOOL STAPM(int L, int M, double D, double* A)
{
//L- nlv число точек(линий 2-го семейства)
//M- nlu число линий 1-го семейства
	double RR[7];
	int ikp[26]={5,1,1,1,-1,-1,901,504,602,8,-216,100,901,605,-217,100,603,107,804,2,-118,202,-119,303,2000,0};
	int LR[8]={1,4,13,14,1,7,14,0};
	int I=1;
	if(L<2 || L>1024){
		message_error_("Размерность числа точек не равна 2-1024!");
		return BAD;
	}
	if(M<2 || M>1024){
		message_error_("Размерность числа линий не равна 2-1024!");
		return BAD;
	}
/////ПРОХОД ПО U
	int IP=0;
	for(I=2; I<=L; I++)
		if(A(13,I-1,1)>=A(13,I,1)) goto Lb14;
	IP=1;
Lb14:int J=1;
 for( J=1;J<=M; J++){
		if(J==1) goto Lb3;
		IP=1;
		for(I=1; I<=L; I++)
			A(13,I,J)=A(13,I,J-1);
Lb3:	if(STAFM2(IP,L,LR,D,&A(1,1,J)))
				return BAD;
	  }
/////ПРОХОД ПО V
	IP=0;
	LR(8)=14*L;
	for(J=2; J<=M; J++)
		if(A(14,1,J-1)>=A(14,1,J)) goto Lb16;
	IP=1;
Lb16:	for(I=1; I<=L; I++){
			if(I==1) goto Lb7;
			IP=1;
			for(J=1; J<=M; J++)
				A(14,I,J)=A(14,I-1,J);
Lb7:		if(STAFM2(IP,M,&LR(5),D,&A(1,I,1)))
				return BAD;
		}
/////УЧЕТ ЗАДАННЫХ ВЕКТОРОВ НОРМАЛИ
	for(J=1; J<=M; J++)
		for(I=1; I<=L; I++){
			  if(fabs(A(10,I,J))+fabs(A(11,I,J))+fabs(A(12,I,J))==0)
				  continue;
			  if(VOPC(&A(4,I,J),2,&A(10,I,J),&A(4,I,J),&A(10,I,J)))
				  return BAD;
			  if(VOPC(&A(7,I,J),2,&A(10,I,J),&A(7,I,J),&A(10,I,J)))
				  return BAD;
			  A(10,I,J)=0;
			  A(11,I,J)=0;
			  A(12,I,J)=0;
	}
/////РАСЧЕТ Ruv
	int N=M-1;
	for(I=1; I<=L; I++){
		if(M==2) goto Lb12;
		for(J=2; J<=N; J++){
			double D1=A(14,I,J)-A(14,I,J-1);
			double D2=A(14,I,J+1)-A(14,I,J);
			double Y[3];
			if(ASVOPM(RR,ikp,&A(4,I,J-1),&A(4,I,J),&A(4,I,J+1),&D1,&D2,Y,Y,Y))
				return BAD;
			if(RR(6)+RR(7)<=0.00001) 
				continue;
			for(int JJ=1; JJ<=3; JJ++)
				A(9+JJ,I,J)=RR(JJ)*RR(4)*RR(5)/(RR(6)+RR(7));
		  }
Lb12:	double D1=A(14,I,2)-A(14,I,1);
		double D2=A(14,I,M)-A(14,I,M-1);
		for(int JJ=1; JJ<=3; JJ++){
			A(9+JJ,I,1)=(A(3+JJ,I,2)-A(3+JJ,I,1))*2/D1-A(9+JJ,I,2);
			if(M==2) 
				A(9+JJ,I,1)=(A(3+JJ,I,2)-A(3+JJ,I,1))/D1;
			A(9+JJ,I,M)=(A(3+JJ,I,M)-A(3+JJ,I,M-1))*2/D2-A(9+JJ,I,M-1);
		}

	}
	return OK;

}
#define ISP(x) ISP[x-1]
BOOL INPW(double RES[24],int  ISP[8],double  UV[6],double* A);
BOOL INPV(double RES[24],int  ISP[8],double  UV[6],double* A);
BOOL INPR(double RES[24],int  ISP[8],double  UV[6],double* A);

//****************************************************************
//
//****************************************************************
BOOL INPM(double RES[24],int  ISP[8],double  UV[6],double* A)
{
//*****BЫЧИCЛEHИE ЗНАЧЕНИЙ ПОВЕРХНОСТИ
//*****BEPCИЯ: 00 (25.01.97)
	switch (ISP(7)){
	case 42:
		return INPW(RES,ISP,UV,A);
	case 41:
		;
//		return INPV(RES,ISP,UV,A);
	case 40:
		;
//		return INPR(RES,ISP,UV,A);
	}
	message_error_("Тип поверхности не равен 40-42!");
	return BAD;
}

#undef A
#undef B

#define B(x,y) B[x-1+28*(y-1)]
#define UV(x) UV[x-1]
#define IJ(x) IJ[x-1]
#define TS(x) TS[x-1]
#define A(x) A[x-1]
#define KU(x) KU[x-1]

//****************************************************************
// [ {
//****************************************************************
BOOL INPW(double RES[24],int  ISP[8],double  UV[6],double* A)
{
//*****BЫЧИCЛEHИE TOЧEK ПOBEPXHOCTИ ОБЩЕГО ВИДА (ТИП=42)
//*****BEPCИЯ: 00 (25.01.97)
	static int IJ[2];
	static int KU[3];
	static double TS[2];
//	double B(28,2);
	double B[56];
 
	int L=ISP(5);
	if(L<2){
		message_error_("РАЗМЕРНОСТЬ СЕТКИ ПОВ-ТИ < 2!");
		return BAD;
	}
	int M=ISP(6);
	if(M<2){
		message_error_("РАЗМЕРНОСТЬ СЕТКИ ПОВ-ТИ < 2!");
		return BAD;
	}
//....РАСЧЕТ КОЭФФИЦИЕНТОВ СПЛАЙН-ПОВЕРХНОСТИ(если они нулевые)
//	if(ISP(8)==1)
//		if(STAPMM(L,M,-1.,A,*100,1))
//			return BAD;
	ISP(8)=0;
//....УCTAHOBKA B HAЧAЛЬHOE COCTOЯHИE UV(3)-UV(4)
	if(UV(3)!=0 || UV(5)!=0) goto Lb1;
	UV(3)=1;
	UV(5)=L;
Lb1: if(UV(4)!=0 || UV(6)!=0) goto Lb2;
	UV(4)=1;
	UV(6)=M;
//....OПPEДEЛEHИE HOMEPA КЛЕТКИ И PACЧET OTHOC. ПAPAMETPОВ
Lb2:int I=1;
    for( I=1; I<=2; I++){
		if((UV(I)-UV(I+2))*(UV(I+4)-UV(I))<0 && ISP(I)<2){
			message_error_("ЭKCTPAПOЛЯЦИЯ ЗAПPEЩEHA!");
			return BAD;
		}
		IJ(I)=(int)UV(I);
		if(IJ(I)<1)
			IJ(I)=1;
		if(IJ(I)>=ISP(4+I))
			IJ(I)=ISP(4+I)-1;
		TS(I)=UV(I)-IJ(I);
	 }
//....ВЫБОР КЛЕТКИ
	int K=((IJ(2)-1)*L+IJ(1)-1)*14;
	for(I=1; I<=2; I++){
		for(int J=1; J<=28; J++){
			K=K+1;
			B(J,I)=A(K);
		}
		K=K+L*14-28;
	}
//....ВЫЧИСЛЕНИЕ ТОЧКИ
	KU(1)=ISP(4);
	KU(2)=10*ISP(1)+ISP(2);
	KU(3)=ISP(3);
	return VTPM(RES,KU,B,TS);
}

#undef RR

#define RES(x,y) RES[x-1+3*(y-1)]
#define A(x) A[x-1]
#define AU(x) AU[x-1]
#define LR(x) LR[x-1]
#define KU(x) KU[x-1]
#define FG(x) FG[x-1]
#define R(x) R[x-1]
#define KU1(x) KU1[x-1]
#define RR(x,y) RR[x-1+3*(y-1)]

//****************************************************************
//
//****************************************************************
BOOL VTPM(double RES[24], int KU[3], double AU[56], double UV[2])
{
//*****BЫЧИCЛEHИE TOЧKИ HA KЛETKE ПOB-TИ
//*****BEPCИЯ: 00 (25.01.97)
	double FG[4];
	double R[14];
//	double RR(3,4);
	double RR[12];
	double P1=1;
	double P2=1;
	int LR[4];
    int KU1[4];
	int I,J;
	int L=0;
	int IP=6;
	int M=1;
	double R2=0;
	int IK=0;
	int IRR=0;
	int IRX=0;

	int K=KU(1);
	LR(1)=0;
	LR(2)=3;
	LR(3)=K;
	LR(4)=K+3;
	int IXU=KU(2)/10;
	int IXV=KU(2)-10*IXU;
	if(IXU*(4-IXU)<=0){
		message_error_("ПРИЗНАК ЭКСТРАПОЛЯЦИИ НЕ 1-3!");
		return BAD;
	}

	if(IXV*(4-IXV)<=0){
		message_error_("ПРИЗНАК ЭКСТРАПОЛЯЦИИ НЕ 1-3!");
		return BAD;
	}
	int LV=__IntAbs(KU(3));
	int IPR=1;
	if(LV>100) goto Lb70;
	IPR=0;
Lb70: LV=LV-100*IPR;
	int IU=LV/10;
	int IV=LV-10*IU;
	if(IU*(5-IU)<=0){
		message_error_("ОШИБКА В ПPИЗHAKЕ РEЗУЛЬТАТА!");
		return BAD;
	}
	if(IV*(5-IV)<=0){
		message_error_("ОШИБКА В ПPИЗHAKЕ РEЗУЛЬТАТА!");
		return BAD;
	}
	if(K<14){
		message_error_("HAPУШEHO TEЛO MOДУЛЯ!");
		return BAD;
	}
	LV=1;
	int KIN=1;
	double T=UV(1);
	if(IU==1) goto Lb3;
	if( WFM(FG,UV(2),1,IXV))
		return BAD;
	goto Lb31;
Lb2: if(IV==1) goto Lb4;
	KIN=IU+1;
Lb3: LV=2;
	T=UV(2);
	if(WFM(FG,UV(1),1,IXU))
		return BAD;
	goto Lb33;
Lb4: if(KU(3)>=0) goto Lb10;
	LV=3;
	KIN=IU+IV;
	T=UV(1);
	IU=2;
	if(WFM(FG,UV(2),2,IXV))
		return BAD;
	goto Lb31;
Lb10: if(IPR==0) 
		  return OK;
	L=1;
	I=K-1;
	J=IU+IV;
	if(KU(3)<0) 
		J=KIN+1;
Lb72: P1=AU(I);
	P2=AU(I+L*K);
	RES(1,J)=P1*(1-UV(L))+P2*UV(L);
	RES(2,J)=P1;
	RES(3,J)=P2;
	if(I==K) 
		return OK;
	I=K;
	J=J+1;
	L=2;
	goto Lb72;
Lb31: IP=6;
	M=1;
	R2=AU(3*K)-AU(K);
	P1=AU(K-1);
	P2=AU(2*K-1);
	IK=2*K;
	IRR=10+IU;
	IRX=IXU;
	goto Lb36;
Lb33: M=2;
	IK=K;
	IP=3;
	R2=AU(2*K-1)-AU(K-1);
	P1=AU(K);
	P2=AU(3*K);
	IRX=IXV;
	IRR=10+IV;
Lb36: L=1;
	J=1;
Lb34: I=1;
Lb35: int I1=I+LR(L)*M;
	int I2=I1+IK;
	int I3=I1+IP;
	int I4=I2+IP;
	R(J)=AU(I1)*FG(1)+AU(I2)*FG(2)+R2*(AU(I3)*FG(3)+AU(I4)*FG(4));
	J=J+1;
	I=I+1;
	if(I<=3) goto Lb35;
	L=L+1;
	if(L!=3) goto Lb37;
	R(J)=P1;
	J=J+1;
Lb37: if(L<=4) goto Lb34;
	R(J)=P2;
	KU1(1)=7;
	KU1(2)=0;
	KU1(3)=IRX;
	KU1(4)=IRR;
	if( VTKM(RR,KU1,R,T))
		return BAD;
	I=KIN;
	I1=1;
	IK=IRR-10;
	if(KIN==1 || LV==4) goto Lb38;
	I1=2;
Lb38: for(int N=I1; N<=IK; N++){
		for(J=1; J<=3; J++)
			RES(J,I)=RR(J,N);
		I=I+1;
	  }
	  switch (LV){
	  case 1:
		goto Lb2;
	  case 2:
		goto Lb4;
	  case 3:
		goto Lb10;
	  }
	message_error_("HAPУШEHO TEЛO MOДУЛЯ!");
	return BAD;
}

#ifdef ASTRA_SURF_EX

C****************************************************************
C
C****************************************************************

	SUBROUTINE VTP(RES,KU,AU,UV,*,XX)
C***	KU(3)= 14,11,22
C***	UV=1-2
C***AU	Kлетка p1(u=0,v=0)  p2(u=1,v=0) 
C***		p3(u=0,v=1) p4(u=1,v=1)
C***
	
C*****BЫЧИCЛEHИE TOЧKИ HA KЛETKE ПOB-TИ
C*****BEPCИЯ: 09 (25.09.90)
	DIMENSION RES(3,8),KU(3),AU(56),UV(2),FG(4),
     *	    R(14),RR(3,4),LR(4),KU1(4)
	DIMENSION IYY(3),WW(25)
	COMMON IRAS
	SAVE
	DATA IYY/0,'VTP ','  09'/
	DATA WW/'HEДO','ПУCT','ИMЫЙ',' ПPИ','ЗHAK',' ЭKC','TP-И','И   ',
     *	  '****',
     *	  'HEДO','ПУCT','ИMЫЙ',' ПPИ','ЗHAK',' PEЗ','-TA ','****',
     *	  'K<14','****',
     *	  'HAPУ','ШEHO',' TEЛ','O MO','ДУЛЯ','****'/
	CALL ATRASW(0,IYY)
	K=KU(1)
	LR(1)=0
	LR(2)=3
	LR(3)=K
	LR(4)=K+3
	IXU=KU(2)/10
	IXV=KU(2)-10*IXU
	if(IXU*(5-IXU)<=0) goto Lb101
	if(IXV*(5-IXV)<=0) goto Lb101
   43 LV=abs(KU(3))
	IPR=1
	if(LV>100) goto Lb70
	IPR=0
   70 LV=LV-100*IPR
	IU=LV/10
	IV=LV-10*IU
	if(IU*(5-IU)<=0) goto Lb102
	if(IV*(5-IV)<=0) goto Lb102
	if(K<14) goto Lb103
    1 LV=1
	IN=1
	T=UV(1)
	if(IU==1) goto Lb3
	CALL WF(FG,UV(2),1,IXV,*100,1)
	goto Lb31
    2 if(IV==1) goto Lb4
	IN=IU+1
    3 LV=2
	T=UV(2)
	CALL WF(FG,UV(1),1,IXU,*100,2)
	goto Lb33
    4 if(KU(3)>=0) goto Lb10
	LV=3
	IN=IU+IV
	T=UV(1)
	IU=2
	CALL WF(FG,UV(2),2,IXV,*100,3)
	goto Lb31
   10 if(IPR==0) goto Lb5
	L=1
	I=K-1
	J=IU+IV
	if(KU(3)<0) J=IN+1
   72 P1=AU(I)
	P2=AU(I+L*K)
	RES(1,J)=P1*(1-UV(L))+P2*UV(L)
	RES(2,J)=P1
	RES(3,J)=P2
	if(I==K) goto Lb5
	I=K
	J=J+1
	L=2
	goto Lb72
   31 IP=6
	M=1
	R2=AU(3*K)-AU(K)
	P1=AU(K-1)
	P2=AU(2*K-1)
	IK=2*K
	IRR=10+IU
	IRX=IXU
	goto Lb36
   33 M=2
	IK=K
	IP=3
	R2=AU(2*K-1)-AU(K-1)
	P1=AU(K)
	P2=AU(3*K)
	IRX=IXV
	IRR=10+IV
   36 L=1
	J=1
   34 I=1
   35 I1=I+LR(L)*M
	I2=I1+IK
	I3=I1+IP
	I4=I2+IP
	R(J)=AU(I1)*FG(1)+AU(I2)*FG(2)+R2*(AU(I3)*FG(3)+AU(I4)*
     *FG(4))
	J=J+1
	I=I+1
	if(I<=3) goto Lb35
	L=L+1
	if(L!=3) goto Lb37
	R(J)=P1
	J=J+1
   37 if(L<=4) goto Lb34
	R(J)=P2
	KU1(1)=7
	KU1(2)=0
	KU1(3)=IRX
	KU1(4)=IRR
	CALL VTK(RR,KU1,R,T,*100,4)
	I=IN
	I1=1
	IK=IRR-10
	if(IN==1 || LV==4) goto Lb38
	I1=2
   38 DO 39 N=I1,IK
	DO 57 J=1,3
   57 RES(J,I)=RR(J,N)
   39 I=I+1
	GOTO(2,4,10),LV
	goto Lb104
    5 RETURN
  104 IRAS=IRAS+1
  103 IRAS=IRAS+1
  102 IRAS=IRAS+1
  101 IRAS=IRAS+1
  100 CALL ATRAS(IYY(2),WW,XX)
	RETURN 1
	END
C*********************************************
C		5
C*********************************************
C
	SUBROUTINE VTPG(RES,KU,AU,UV,iwork)
C*****
	DIMENSION RES(3,8),KU(3),AU(56),UV(2)
C*****
	CALL VTP(RES,KU,AU,UV,*100,1)
	iwork=0
	RETURN	
100	iwork=1
	CALL ATRASR
	write(*,*)'BAD WORK VTPG!!!',iwork
	RETURN
	END

C****************************************************************
C
C****************************************************************
	SUBROUTINE INPV(RES,ISP,UV,A,*,NNN)
C*****BЫЧИCЛEHИE TOЧEK ПOB. ВРАЩЕНИЯ, ДАННЫЕ КОТОРОЙ РАСПОЛОЖЕНЫ В ОП
C*****BEPCИЯ: 01 (25.07.97)
	DIMENSION RES(27),ISP(8),A(7,100),ISK(7),RS(13),RP(24)
	DIMENSION IYY(3),IWW(16)
	REAL*8 UV(6),T(3)
	INTEGER*2 NU(20)
	COMMON IRAS
	SAVE
	DATA IYY/0,'INPV','  01'/
	DATA IWW/'ЧИСЛ','О ТО','ЧЕК ','ОБРА','ЗУЮЩ','ЕЙ <',' 2  ','****',
     *	   'ОШИБ','КА В',' ПРИ','ЗНАК','Е РЕ','ЗУЛЬ','ТАТА','****'/
	DATA NU/2,2,1,900,604,1004,500,1008,5,2000,0,
     *	  2,1,1,1004,405,1008,4,2000,0/  
	DATA Y/7777777777./
	CALL ATRASW(0,IYY)
	L=ISP(5)
	if(L<2) GO TO 101
	CALL VOPA(A(4,1),3,A(4,1),*100,1)
//....УCTAHOBKA B HAЧAЛЬHOE COCTOЯHИE UV(3)-UV(4)
	if(UV(3)==0 && UV(5)==0) THEN
	   UV(3)=1
	   UV(5)=L
	ENDif
	if(UV(4)==0 && UV(6)==0) THEN
	   UV(4)=0
	   UV(6)=6.283185312
	ENDif
//....ВЫЧИСЛЕНИЕ ЗНАЧЕНИЙ
	L=abs(ISP(3))
	IP=L/100
	IR=L-IP*100
	IU=IR/10
	IV=IR-IU*10
	if(IU==1 && (IV>1 || ISP(3)<0)) IU=IU+1
	if(IV<1 || IV>4) goto Lb102
	N=IU*3+1
	ISK(1)=ISP(1)
	ISK(2)=100*IP+IU+10
	ISK(3)=ISP(4)
	ISK(4)=ISP(5)
	ISK(5)=1
	ISK(6)=30
	ISK(7)=ISP(8)
	T(1)=UV(1)
	T(2)=UV(3)
	T(3)=UV(5)
	CALL INKM(RS,ISK,T,A(1,2),*100,1)
	ISP(8)=0
	CALL ASVOPM(RP,NU,A,RS,Y,Y,Y,Y,Y,Y,*100,1)
	CALL VOPA(R1,2,RP,*100,2)
	if(IU==1) goto Lb1 
	DO I=2,IU
	   L=3*I-2
	   CALL ASVOPM(RP(L),NU(12),A(4,1),RS(L),Y,Y,Y,Y,Y,Y,*100,2)
	ENDDO
    1 SI=DSIN(UV(2))
	CO=DCOS(UV(2))
	CN=CO-1
	IU=IR/10
	K=0
	DO L=1,IU
	   DO I=1,3
		K=K+1
		RES(K)=RS(K)+SI*RP(K)+CN*RP(K+12)
	   ENDDO
	ENDDO
	L=K
	if(IV==1 && ISP(3)>0) goto Lb10
	DO I=1,3
	   K=L+I
	   if(IV==1) goto Lb2 
	   B=RP(I)
	   C=RP(I+12)
	   if(R1==0) THEN
		B=RP(I+3)
		C=RP(I+15)
	   ENDif
	   RES(K)=CO*B-SI*C
	   if(IV==2) goto Lb2 
	   K=K+3
	   RES(K)=-(SI*B+CO*C)
	   if(IV==3) goto Lb2 
	   K=K+3
	   RES(K)=-RES(K-6)
    2    if(ISP(3)<0) THEN
	     K=K+3
	     RES(K)=(CO*RP(I+3)-SI*RP(I+15))
	   ENDif
	ENDDO
//....ВОЗВРАТ ВНУТРЕННИХ ПАРАМЕТРОВ
   10 if(IP==0) return OK
	if(IP!=1) goto Lb102
	RES(K+1)=RS(N)
	RES(K+2)=RS(N+1)
	RES(K+3)=RS(N+2)
	CALL VOPC(S,11,A,A(1,2),A(4,1),*100,1)
	RES(K+4)=UV(2)*S
	RES(K+5)=0
	RES(K+6)=S*6.283185312
   99 RETURN
  102 IRAS=IRAS+1
  101 IRAS=IRAS+1
  100 CALL ATRAS(IYY(2),IWW,NNN)
	RETURN 1
	END

C****************************************************************
C
C****************************************************************
	SUBROUTINE INPR(RES,ISP,UV,A,*,NNN)
C*****BЫЧИCЛEHИE TOЧEK ЛИНЕЙЧАТОЙ ПOBEPXHOCTИ, ПО ДАННЫМ В ОП
C*****BEPCИЯ: 01 (25.07.97)
	DIMENSION RES(30),ISP(8),A(200),ISK(7),RS(30)
	DIMENSION IYY(3),IWW(38)
	REAL*8 UV(6),T(3)
	COMMON IRAS
	SAVE
	DATA IYY/0,'INPR','  01'/
	DATA IWW/'ПАРА','МЕТР',' К О','ТЛИЧ','ЕН О','Т 13','****',
     *	   'ЧИСЛ','О ТО','ЧЕК ','ОБРА','ЗУЮЩ','ЕЙ <',' 2  ','****',
     *	   'ОШИБ','КА В',' ПРИ','ЗНАК','Е РЕ','ЗУЛЬ','ТАТА','****',
     *	   'ПРИЗ','НАК ','ЭКСТ','РАПО','ЛЯЦИ','И НЕ',' 1-3','****',	 
     *	   'ЭKCT','PAПO','ЛЯЦИ','Я ЗA','ПPEЩ','EHA ','****'/
	CALL ATRASW(0,IYY)
	if(ISP(4)!=13) goto Lb101     
	L=ISP(5)
	if(L<2) goto Lb102
//....УCTAHOBKA B HAЧAЛЬHOE COCTOЯHИE UV(3)-UV(4)
	if(UV(3)==0 && UV(5)==0) THEN
	   UV(3)=1
	   UV(5)=L
	ENDif
	if(UV(4)==0 && UV(6)==0) THEN
	   UV(4)=0
	   UV(6)=1
	ENDif
//....ВЫЧИСЛЕНИЕ ЗНАЧЕНИЙ
	K=1
	L=abs(ISP(3))
	IP=L/100
	IR=L-IP*100
	IU=IR/10
	IV=IR-IU*10
	if(IV<1 || IV>4) goto Lb103
	if(IU<1 || IU>4) goto Lb103
	IUU=IU
	if(ISP(3)<0 && IU==1) IUU=2
	ISK(1)=ISP(1)
	ISK(2)=-(100*IP+20+IUU)
	ISK(3)=13
	ISK(4)=ISP(5)
	ISK(5)=1
	ISK(6)=30
	ISK(7)=0
	T(1)=UV(1)
	T(2)=UV(3)
	T(3)=UV(5)
	CALL INKM(RS,ISK,T,A,*100,1)
	ISP(8)=0
	if(ISP(2)<1 || ISP(2)>3) goto Lb104
	if((UV(2)-UV(4))*(UV(6)-UV(2))<0 && ISP(2)==1) goto Lb105
	L=IUU*3
	J=IU*3
	DO I=1,J
	   RES(I)=RS(I)+RS(L+I)*UV(2)
	ENDDO
	K=J
	if(IV==1 && ISP(3)>0) goto Lb2
	DO I=1,3
	   K=J+I
	   if(IV==1) goto Lb1
	   RES(K)=RS(L+I)
	   K=K+3
	   if(IV==2) goto Lb1
	   RES(K)=0
	   K=K+3
	   if(IV==3) goto Lb1
	   RES(K)=0
	   K=K+3
    1    if(ISP(3)<0) THEN
		RES(K)=RS(L+I+3)
	   ENDif
	ENDDO
//....ВОЗВРАТ ПАРАМЕТРОВ
    2 if(IP==0) return OK
	if(IP!=1) goto Lb103
	L=2*L+1
	RES(K+1)=RS(L)
	RES(K+2)=RS(L+1)
	RES(K+3)=RS(L+2)
	CALL VOPA(S,2,A(7),*100,1)
	RES(K+4)=UV(2)*S
	RES(K+5)=0
	RES(K+6)=S
   99 RETURN
  105 IRAS=IRAS+1
  104 IRAS=IRAS+1
  103 IRAS=IRAS+1
  102 IRAS=IRAS+1
  101 IRAS=IRAS+1
  100 CALL ATRAS(IYY(2),IWW,NNN)
	RETURN 1
	END

C****************************************************************
C
C****************************************************************
	SUBROUTINE STAPMM(L,M,D,A,*,NNN)
C*****РАСЧЕТ КОЭФФИЦ. СПЛАЙН-ПОВЕРХНОСТИ
C*****BEPCИЯ: 00 (25.01.97)
	DIMENSION A(14,L,M),RR(7)
	DIMENSION IYY(3),IWW(9)
	INTEGER*2 IN(26),LR(8)
	SAVE
	DATA IYY/0,'STAP','MM00'/
	DATA IWW/'РАЗМ','ЕРНО','СТЬ ','L ИЛ','И М ','НЕ [','2-10','24] ',
     *	   '****'/
	DATA IN/5,1,1,1,-1,-1,901,504,602,8,-216,100,901,605,-217,100,
     *	  603,107,804,2,-118,202,-119,303,2000,0/
	DATA LR/1,4,13,14,1,7,14,0/
	CALL ATRASW(0,IYY)
	if(L<2 || L>1024) goto Lb101
	if(M<2 || M>1024) goto Lb101
//....ПРОХОД ПО U
	IP=0
	DO 13 I=2,L
	if(A(13,I-1,1)>=A(13,I,1)) goto Lb14
   13 CONTINUE
	IP=1
   14 DO 3 J=1,M
	if(J==1) goto Lb3
	IP=1
	DO 5 I=1,L
    5 A(13,I,J)=A(13,I,J-1)
    3 CALL STAFMM(IP,L,LR,D,A(1,1,J),*100,1)
//....ПРОХОД ПО V
	IP=0
	LR(8)=14*L
	DO 15 J=2,M
	if(A(14,1,J-1)>=A(14,1,J)) goto Lb16
   15 CONTINUE
	IP=1
   16 DO 7 I=1,L
	if(I==1) goto Lb7
	IP=1
	DO 6 J=1,M
    6 A(14,I,J)=A(14,I-1,J)
    7 CALL STAFMM(IP,M,LR(5),D,A(1,I,1),*100,2)
//....УЧЕТ ЗАДАННЫХ ВЕКТОРОВ НОРМАЛИ
	DO 2 J=1,M
	DO 2 I=1,L
	if(ABS(A(10,I,J))+ABS(A(11,I,J))+ABS(A(12,I,J))==0) goto Lb2
	CALL VOPA(A(10,I,J),3,A(10,I,J),*100,1)
	CALL VOPC(A(4,I,J),2,A(10,I,J),A(4,I,J),A(10,I,J),*100,1)
	CALL VOPC(A(7,I,J),2,A(10,I,J),A(7,I,J),A(10,I,J),*100,2)
	A(10,I,J)=0
	A(11,I,J)=0
	A(12,I,J)=0
    2 CONTINUE
//....РАСЧЕТ Ruv
	N=M-1
	DO 4 I=1,L
	if(M==2) goto Lb12
	DO 8 J=2,N
	D1=A(14,I,J)-A(14,I,J-1)
	D2=A(14,I,J+1)-A(14,I,J)
	CALL ASVOPM(RR,IN,A(4,I,J-1),A(4,I,J),A(4,I,J+1),D1,D2,
     *Y,Y,Y,*100,5)
	if(RR(6)+RR(7)<=0.00001) goto Lb8
	DO 9 JJ=1,3
    9 A(9+JJ,I,J)=RR(JJ)*RR(4)*RR(5)/(RR(6)+RR(7))
    8 CONTINUE
   12 D1=A(14,I,2)-A(14,I,1)
	D2=A(14,I,M)-A(14,I,M-1)
	DO 10 JJ=1,3
	A(9+JJ,I,1)=(A(3+JJ,I,2)-A(3+JJ,I,1))*2/D1-A(9+JJ,I,2)
	if(M==2) A(9+JJ,I,1)=(A(3+JJ,I,2)-A(3+JJ,I,1))/D1
   10 A(9+JJ,I,M)=(A(3+JJ,I,M)-A(3+JJ,I,M-1))*2/D2-A(9+JJ,I,M-1)
    4 CONTINUE
   99 RETURN
  101 IRAS=IRAS+1
  100 CALL ATRAS(IYY(2),IWW,NNN)
	RETURN 1
	END

C****************************************************************
C
C****************************************************************
	SUBROUTINE STAFM(IPR,N,LR,D,A,*,XX)
C*****РАСЧЕТ КОЭФФИЦ. СПЛАЙНОВОЙ КРИВОЙ
C*****ВЕРСИЯ: 01 (25.02.92)
	DIMENSION B(15,5),IB(75),KU(4),RS(14),A(100)
	DIMENSION IYY(3),WW(41)
	INTEGER*2 IU(37),LR(4)
	COMMON IRAS
	EQUIVALENCE (B,IB)
	SAVE
	DATA IYY/0,'STAF','M 01'/
	DATA WW/'ПРИЗ','НАК ','ПАРА','М-ЦИ','И НЕ',' [0-','1]  ','****',
     *	  'ЧИСЛ','О ТО','ЧЕК ','НЕ [','2-10','24] ','****',
     *	  'ОШИБ','КА В',' ИНД','ЕКСА','Х   ','****',
     *	  'ПАРА','МЕТР',' НЕ ','ВОЗР','АСТА','ЕТ  ','****',
     *	  'СОВП','АДЕН','ИЕ Т','ОЧЕК','****',
     *	  'ВЫРО','ЖДЕН','НЫЙ ','ОТРЕ','ЗОК ','КРИВ','ОЙ  ','****'/
	DATA KU/7,0,1,12/
	DATA IU/2,1,1,904,405,-216,0,2000,0,
     *	  2,1,1,101,405,800,404,600,1,1304,5,2000,0,
     *	  2,2,2,801,507,1302,406,-113,101,-114,102,-415,406,2000,0/
	CALL ATRASW(0,IYY)
//....КОНТРОЛЬ ПАРАМЕТРОВ
	if(IPR<0 || IPR>1) goto Lb101
	if(N<2 || N>1024) goto Lb102
	if(LR(1)<1 || LR(1)>26) goto Lb103
	if(LR(2)<LR(1)+3 || LR(2)>29) goto Lb103
	if(LR(3)<LR(2)+3 || LR(3)>32) goto Lb103
	if(LR(4)<7 || LR(4)>6000) goto Lb103
	DO 1 I=1,75
    1 IB(I)=0
	JT=0
	JL=1
	PRM=A(LR(3))
//....СЕКЦИЯ 1
   10 DO 2 I=1,15
    2 IB(I)=0
	JT=JT+1
	if(JT>N) goto Lb20
	J=(JT-1)*LR(4)
	if(IPR>0) B(14,1)=A(LR(3)+J)
	DO 11 I=7,9
	B(I,1)=A(LR(1)+J)
	B(I+3,1)=A(LR(2)+J)
   11 J=J+1
	IB(1)=JT
	if(JT==1) goto Lb16
	if(IPR>0 && B(14,1)<=B(14,2)) goto Lb104
	CALL ASVOPM(B(3,1),IU,B(7,1),B(7,2),Y,Y,Y,Y,Y,Y,*100,1)
	H=B(6,1)
	if(H!=0) goto Lb15
	if(D<=0) goto Lb105
	if(JL<=2 || JT==N) goto Lb106
	JL=1
	goto Lb16
   15 CALL VOPA(B(3,1),3,B(3,1),*100,1)
	CALL VOPB(B(15,2),3,B(3,1),B(3,2),*100,1)
   16 IB(2)=JL
	JL=JL+1
//....СЕКЦИЯ 2
   20 if(IB(32)<=0) goto Lb40
	if(ABS(B(10,3))+ABS(B(11,3))+ABS(B(12,3))!=0) goto Lb5
	if(IB(32)==1) goto Lb30
	if(IB(32)!=2) goto Lb21
	if(IB(17)!=3) goto Lb23
	if(IB(2)!=4) goto Lb25
	goto Lb28
   21 if(IB(32)!=3) goto Lb22
	if(IB(17)!=4) goto Lb23
	if(IB(2)!=5) goto Lb19
	goto Lb9
   22 if(IB(17)<=1) goto Lb23
	if(IB(2)<=1) goto Lb19
	goto Lb9
//....по 2-ум точкам в последней
   23 if(ABS(B(10,4))+ABS(B(11,4))+ABS(B(12,4))!=0) goto Lb24
	B(10,3)=B(3,3)
	B(11,3)=B(4,3)
	B(12,3)=B(5,3)
	goto Lb5
   24 CALL ASVOPM(B(10,3),IU(10),B(3,3),B(10,4),Y,Y,Y,Y,Y,Y,
     **100,2)
	goto Lb4
//....по 3-ем точкам в средней
   25 AL=1
	BE=1
	goto Lb8
//....по 4-ем точкам во второй
   28 AL=1
	BE=B(15,2)/(B(15,2)+0.005)
	goto Lb8
//....по 4-ем точкам в третьей
   19 BE=1
	AL=B(15,4)/(B(15,4)+0.005)
	goto Lb8
//....по 5-ти точкам
    9 AL=B(15,4)/(B(15,4)+0.005)
	BE=B(15,2)/(B(15,2)+0.005)
    8 if(AL+BE>=0.8) goto Lb7
	AL=1
	BE=1
    7 AL=AL*B(6,3)
	BE=BE*B(6,2)
	DO 6 I=3,5
    6 B(I+7,3)=AL*B(I,2)+BE*B(I,3)
    5 CALL VOPA(B(10,3),3,B(10,3),*100,2)
//....в 1-ой точке
	if(IB(47)!=1) goto Lb30
    4 if(ABS(B(10,4))+ABS(B(11,4))+ABS(B(12,4))!=0) goto Lb30
	CALL ASVOPM(B(10,4),IU(10),B(3,3),B(10,3),Y,Y,Y,Y,Y,Y,
     **100,3)
//....СЕКЦИЯ 3
   30 if(IB(31)==0) goto Lb40
	B(13,3)=0
	if(IB(31)==1) goto Lb50
	if(IB(32)>1) goto Lb31
	B(13,3)=B(13,4)
	if(D>0) B(13,3)=B(13,3)+D
	goto Lb40
   31 CALL ASVOPM(RS,IU(23),B(7,3),B(7,4),Y,Y,Y,Y,Y,Y,*100,4)
	U=16+RS(2)*RS(2)-RS(1)
	B(13,3)=B(13,4)+6*RS(3)/(SQRT(U)+RS(2))
	if(D<=0) goto Lb40
	if(IB(17)!=1) goto Lb32
	U=1
	AL=-1
	J=3
	goto Lb33
   32 if(IB(47)!=1) goto Lb40
	if(IB(46)==1) goto Lb40
	U=0
	AL=1
	J=4
   33 DO 34 I=1,6
	RS(I)=B(I+6,4)
   34 RS(I+7)=B(I+6,3)
	RS(7)=B(13,4)
	RS(14)=B(13,3)
	U=ABS(U-D/(RS(14)-RS(7)))
	CALL VTK(B(7,J),KU,RS,U,*100,1)
	CALL VOPA(B(10,J),3,B(10,J),*100,3)
	B(13,J)=B(13,J)+AL*D
//....СЕКЦИЯ 4
   40 if(IB(46)<=1) goto Lb50
	if(IPR>0) goto Lb41
	B(14,4)=B(13,4)
	goto Lb50
   41 if(IB(47)<=1) goto Lb50
   42 if(IB(32)<=1) goto Lb45
//....расчет S' по 3-ем точкам
   44 S1=(B(13,3)-B(13,4))/(B(14,3)-B(14,4))
	S2=(B(13,4)-B(13,5))/(B(14,4)-B(14,5))
	S=S1*S2*(S1+S2)/(S1*S1+S2*S2)
	goto Lb46
//....расчет S' по 2-ум точкам в последней
   45 if(IB(47)==2) S=(B(13,4)-B(13,5))/(B(14,4)-B(14,5))
	if(IB(47)>2) S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,5)
   46 CALL VOPB(B(10,4),13,B(10,4),S,*100,3)
	B(15,4)=S
//....расчет S' по 2-ум точкам в первой
	if(IB(62)!=1) goto Lb50
	S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,4)
	CALL VOPB(B(10,5),13,B(10,5),S,*100,4)
//....СЕКЦИЯ 5
   50 if(IB(61)==0) goto Lb54
	J=(IB(61)-1)*LR(4)
	if(IPR==0) A(LR(3)+J)=B(14,5)+PRM
   52 DO 51 I=7,9
	A(J+LR(1))=B(I,5)
	A(J+LR(2))=B(3+I,5)
   51 J=J+1
   54 if(IB(61)==N) return OK
	I=60
   55 IB(I+15)=IB(I)
	I=I-1
	if(I>0) goto Lb55
	goto Lb10
   99 RETURN
  106 IRAS=IRAS+1
  105 IRAS=IRAS+1
  104 IRAS=IRAS+1
  103 IRAS=IRAS+1
  102 IRAS=IRAS+1
  101 IRAS=IRAS+1
  100 CALL ATRAS(IYY(2),WW,XX)
	RETURN 1
	END

#endif
