////////////	реализация функций Астры на C
/////////	начало 27.07.2000 конец 31.07.
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

#define R(x) R[x-1]
#define PP(x,y) PP[x-1+4*(y-1)]
#define RES(x,y) RES[x-1+7*(y-1)]
#define IS1(x) IS1[x-1]
#define IS2(x) IS2[x-1]
#define TS1(x) TS1[x-1]
#define TS2(x) TS2[x-1]
#define EQV(x) EQV[x-1]

extern  BOOL ASSHG(int IPR, double* T, double TN, double TK, double DT);
extern BOOL ASSHG2(int* IPR, double D[2], double UV[6]);
extern BOOL INPH(double RS[30],int IS[8], double TS[22], double EQV, double* A);
extern BOOL INPW(double RES[24],int  ISP[8],double  UV[6],double* A);
extern BOOL ASM1(double TSR[4],int K, int L, double TS[12], double D[4], double* B);
extern BOOL ASM6W(double F[17], double TS[22], int* K1, int K);
extern BOOL ASM6WM(double F[17], double TS[22], int* K1, int K);


BOOL SCSF0(int IS1[8],int IS2[8],double TS1[22],double TS2[22],\
		   int* LP,double EQV[2],double* A1,double* A2,int* NRP, double* RP);
BOOL SCSF1(int IS1[8],int IS2[8],double TS1[6],double TS2[6],\
		   int* LG,double EQV[2],double E,double* A1, double* A2,int* NRP,double* RP);

BOOL SCSF2(double* RES,int* IS1, int* IS2,double* TS1,double* TS2,\
		   int* IPR,double* EQV, double E,double* A1,double* A2);
BOOL SCSF3(double RS[30],int* IS1,int* IS2,double* TS1,double* TS2,\
		   int* IPR, double EQV[2],double E, double* PT, double* A1,double* A2);

int IRAS=0;

//***************************************************************
//C
//***************************************************************
BOOL SCSF(int* NRES, double* RES, double* PP,int IS1[8],int IS2[8],\
		  double TS1[22],double TS2[22],int LG,double EQV[2], double E, double* A, double* B)
{
//****ПEPECEЧEHИE ПOBEPXHOCTEЙ
//****BEPCИЯ: 01 (11.03.97)
	double R[3];
	int NU[15]={2,2,2,801,507,1302,406,-113,101,-114,102,-415,406,2000,0};
	 if(LG<0 || LG>4){
		message_error_("ПРИЗНАК ГРАНИЦЫ НЕ 1,4!");
		return BAD;
	 }
	 if(*NRES<=IS1(5) || *NRES<=IS1(6)){
		message_error_("ПОЛЕ РЕЗУЛЬТАТА МАЛО!");
		return BAD;
	 }
	int J=1;
	int NVAR=LG;
	int L=0;
	double PT=1.0;
//....поиск нач. приближения
	if( SCSF1(IS1,IS2,TS1,TS2,&NVAR,EQV,E,A,B,NRES,RES))
		return BAD;

//....расчет очередной точки
Lb1: if( SCSF2(&RES(1,J), IS1, IS2, TS1, TS2, &NVAR, EQV, E, A, B))
		 goto Lb2;
	goto Lb3;
//....последняя точка
Lb2: if(J==1)
		return BAD;
	L=0;
	if(TS2(1)==TS2(3) || TS2(1)==TS2(5)) 
		L=3;
	if(TS2(2)==TS2(4) || TS2(2)==TS2(6)) 
		L=2;
	if(L==0)
		return BAD;
	R(1)=EQV(2);
	R(2)=EQV(1);
	if( SCSF3(&RES(1,J), IS2, IS1, TS2, TS1, &L, R, E, &PT, B, A))
		return BAD;
	double Q;
	if( VOPB(&Q,1,&RES(4,J-1), &RES(4,J)))
		return BAD;
	if(Q<0) 
		if(VOPA(&RES(4,J),1, &RES(4,J)))
			return BAD;
	NVAR=5;
//....подсчет параметра в точке
Lb3: RES(7,J)=0;
	PP(1,J)=TS1(1);
	PP(2,J)=TS1(2);
	PP(3,J)=TS2(1);
	PP(4,J)=TS2(2);
	PP(5,J)=0;
	if(J==1) goto Lb4;
	double Y[3];
	if( ASVOPM(R,NU, &RES(1,J), &RES(1,J-1),Y,Y,Y,Y,Y,Y))
		return BAD;
	Q=16+R(2)*R(2)-R(1);
	RES(7,J)=RES(7,J-1)+6*R(3)/(sqrt(Q)+R(2));
//....конец цикла по числу точек сечения 
Lb4: J=J+1;;
	if(J>*NRES-8){
		message_astra("ПOЛE PEЗУЛЬTATA ИCЧEPПAHO!");
		return BAD;
	 }
	if(NVAR!=5) goto Lb1;
	*NRES=J-1;
	return OK;
}

#define RP(x,y) RP[x-1+3*(y-1)]
#define JJ(x) JJ[x-1]
#define  DP(x)  DP[x-1]
#define RS(x) RS[x-1]
#define UV(x) UV[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCSF0(int IS1[8],int IS2[8],double TS1[22],double TS2[22],\
		   int* LP,double EQV[2],double* A1,double* A2,int* NRP, double* RP)
{
//****ПОИСК НАЧ. ПРИБЛ. ТОЧКИ ПЕРЕСЕЧЕНИЯ КООРД. ЛИНИИ С ПОВ-ТЬЮ
//****BEPCИЯ: 00 (22.02.97)
	double DP[2];
	double RS[6];
	int JJ[4]={4,6,3,5};
	double UV[3];
	double D=1;
    int IPR=__IntAbs(*LP);
	double Q=-1;
	if(IPR<1 || IPR>4){
		message_astra("ПРИЗНАК ЛИНИИ НЕ [1-4]!");
		return BAD;
	 }
	if(*NRP-1<IS1(5) || *NRP-1<IS1(6)){
		message_astra("РАБОЧЕЕ ПОЛЕ МАЛО!");
		return BAD;
	 }
	int L1=IS1(3);
	int L2=IS2(3);
	IS1(3)=11;
	IS2(3)=11;
//....ВЫДЕЛЕНИЕ КООРДИНАТНОЙ ЛИНИИ ПЕРВОЙ ПОВЕРХНОСТИ
	int J=1;
	if(IPR>=3) 
		J=2;
	TS1(J)=TS1(J+2);
	if(*LP>0) 
		TS1(3-J)=TS1(JJ(IPR));
	int K=1;
Lb1: if( INPH(&RP(1,K),IS1,TS1,EQV(1),A1))
	   return BAD;
	if(K!=1) goto Lb2;
	D=1;
	if(IS1(4+J)<=2) 
		D=0.2;
Lb2: if(TS1(J)==TS1(J+4)) goto Lb3;
	if( ASSHG(0, &TS1(J), TS1(J+2), TS1(J+4),D))
	   return BAD;
	K=K+1;
	if(K>*NRP-1){
		message_astra("РАБОЧЕЕ ПОЛЕ МАЛО!");
		return BAD;
	 }
	goto Lb1;
//....ПЕРЕБОР ВТОРОЙ ПОВЕРХНОСТИ
Lb3: DP(1)=1;
	DP(2)=1;
	Q=-1;
	IPR=1;
Lb4: if( ASSHG2(&IPR,DP,TS2))
	   return BAD;
	if(EQV(2)==0)
		if( INPW(RS,IS2,TS2,A2))
			return BAD;
	if(EQV(2)!=0) 
		if( INPH(RS,IS2,TS2,EQV(2),A2))
			return BAD;
	if(Q>=0) goto Lb5;
	Q=1E10;
	if(IS2(5)<=2)
		DP(1)=0.2;
	if(IS2(6)<=2)
		DP(2)=0.2;
Lb5: TS1(J)=TS1(J+2);
	for(int I=0; I<=K; I++){
		double R=fabs(RS(1)-RP(1,I))+fabs(RS(2)-RP(2,I))+fabs(RS(3)-RP(3,I));
		if(R>=Q) goto Lb6;
		Q=R;
		UV(1)=TS1(J);
		UV(2)=TS2(1);
		UV(3)=TS2(2);
Lb6:	if( ASSHG(0, &TS1(J),TS1(J+2),TS1(J+4),D))
			return BAD;
	}
	if(IPR!=3) goto Lb4;
	TS1(J)=UV(1);
	TS2(1)=UV(2);
	TS2(2)=UV(3);
	IS1(3)=L1;
	IS2(3)=L2;
	return OK;
}
//***************************************************************
//C
//***************************************************************
BOOL SCSF1(int IS1[8],int IS2[8],double TS1[6],double TS2[6],\
		   int* LG,double EQV[2],double E,double* A1, double* A2,int* NRP,double* RP)
{
//****ПОИСК 1-ОЙ ТОЧКИ ЛИНИИ ПЕРЕСЕЧЕНИЯ ПОВ-ЕЙ
//****BEPCИЯ: 01 (03.03.97)
    static double RS[30];
	if(*LG<0 || *LG>4){
		message_astra("НОМЕР ВАРИАНТА НЕ [1-4]!");
		return BAD;
	 }
	TS1(13)=1;
	TS1(14)=0.01;
	TS2(13)=1;
	TS2(14)=0.01;
	int K=1;
	double PT=1.0;
	if(*LG==0) goto Lb1;
//....ДЛЯ ЗАДАННОЙ ГРАНИЦЫ 1-ОЙ ПОВЕРХНОСТИ
	if( SCSF0(IS1,IS2,TS1,TS2,LG,EQV,A1,A2,NRP,RP))
		return BAD;
	K=int(*LG+1)/2+1;
	return SCSF3(RS, IS1, IS2, TS1, TS2, &K, EQV, E, &PT,A1,A2);
//....ОБЩИЙ СЛУЧАЙ
Lb1: for(int I=1; I<=4; I++){
		*LG=I;
		if( SCSF0(IS1,IS2,TS1,TS2,LG,EQV,A1,A2,NRP,RP))
			return BAD;
		K=(int(*LG+1)/2+1);
		int pr=SCSF3(RS,IS1,IS2,TS1,TS2, &K,EQV,E, &PT, A1,A2);
		if( pr==OK)
			return OK;
//		if(IRAS==0)
//			return OK;
	 }
	return BAD;////нету начального приближения
}

#undef RES

#define KT(x) KT[x-1]
#define K1(x) K1[x-1]
#define T(x) T[x-1]
#define IJ(x) IJ[x-1]
#define NU(x) NU[x-1]
#define RES(x) RES[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCSF2(double* RES,int* IS1, int* IS2, double* TS1, double* TS2,\
		   int* IPR,double* EQV, double E, double* A1, double* A2)
{
//****ПОСЛЕДОВАТЕЛЬНЫЙ РАСЧЕТ ТОЧЕК ЛИНИИ ПЕРЕСЕЧЕНИЯ ПОВ-ЕЙ
//****BEPCИЯ: 00 (24.02.97)
	static double RS[43];
    static double  R[12];
	static int KT[4]={4,6,3,5};
	static int K1[6]={1,2,5,6,9,10};
	static int IJ[4];
	static double T[12];
	int NU[48]={3,1,3,3,901,101,1002,407,903,102,-113,603,-114,502,
		1002,410,903,102,-115,903,-116,802,1001,506,-217,101,
		1001,809,-218,101,2000,0,
		1,4,801,507,902,604,-213,200,601,100,802,202,-414,201,
		2000,0};
	 if(*IPR<0 || *IPR>4){
		message_error_("НОМЕP ВАРИАНТА НЕ [0-4]");
		return BAD;
	}
	 int K=0;
	 int I=0;
	 double D=0;
	 double B=0;
	 int L=1;
	double R1=0;
	if(*IPR==0) goto Lb2;
//....ДЛЯ ПЕРВОЙ ТОЧКИ
	RS(43)=1;
	K=int((*IPR+1)/2);
	TS1(3-K)=TS1(KT(*IPR));
	*IPR=K;
	TS1(13)=1;
	TS1(14)=0.01;
	TS2(13)=1;
	TS2(14)=0.01;
	TS2(18)=1;
	TS2(19)=0.01;
	for(I=1;I<=6; I++){
		T(K1(I))=TS1(I);
		T(K1(I)+2)=TS2(I);
	}
	goto Lb3;
//....ДЛЯ ПОСЛЕДУЮЩИХ ТОЧЕК
Lb2: TS1(1)=TS1(9);
	TS1(2)=TS1(10);
	TS2(1)=TS2(9);
	TS2(2)=TS2(10);
Lb3: K=*IPR+1;
Lb4: if(SCSF3(RS,IS1,IS2,TS1,TS2, &K,EQV,E,&RS(43),A1,A2))
		 return BAD;
	 double Y[3];
	if(ASVOPM(&RS(37),NU, &RS(4), &RS(10), &RS(22),Y,Y,Y,Y,Y))
		return BAD;
	for(I=37;I<=38; I++){
		RS(I)=RS(I)/RS(41);
		RS(I+2)=RS(I+2)/RS(42);
	}
	D=0;
	for(I=37;I<=40; I++)
		D=D+RS(I)*RS(I);
   if(D==0){
		message_error_("НAПPABЛEHИE HE OПPEДEЛEHO");
		return BAD;
	}
	D=sqrt(D);
	for(I=37;I<=40; I++)
		RS(I)=RS(I)/D;
	for(I=1;I<=2; I++){
		T(I)=TS1(I);
		T(I+2)=TS2(I);
	}
	if(*IPR==0) goto Lb6;
	K=0;
	*IPR=0;
	if(ASM1(T,3,4,T, &RS(37), &RS(43)))
		return BAD;
	goto Lb4;
//....РАСЧЕТ ШАГА В ПОСЛЕДУЮЩУЮ ТОЧКУ
Lb6:	for(I=1;I<=2; I++){
		TS1(8+I)=TS1(I);
		TS2(8+I)=TS2(I);
		TS1(I)=TS1(I)+RS(36+I);
		TS2(I)=TS2(I)+RS(38+I);
		IJ(I)=IS1(I);
		IJ(2+I)=IS2(I);
		IS1(I)=2;
		IS2(I)=2;
	}
	for(I=1;I<=6; I++)
		R(I)=RS(I);
	int Ivar=0;
	if(SCSF3(&RS(7),IS1,IS2,TS1,TS2,&Ivar,EQV,E, &RS(43),A1,A2))
		return BAD;
	for(I=1;I<=2; I++){
		IS1(I)=IJ(I);
		IS2(I)=IJ(I+2);
		TS1(I)=TS1(8+I);
		TS2(I)=TS2(8+I);
	}
	B=1;
	for(I=1;I<=6; I++)
		R(6+I)=RS(6+I);
	for(I=1;I<=2; I++){
		if(I==1) goto Lb12;
		for(L=1;L<=3; L++)
			R(6+L)=RS(24+L);
Lb12:	if(ASVOPM(&RS(41),&NU(33),R,Y,Y,Y,Y,Y,Y,Y))
			return BAD;
		double B1=RS(41);
		double B2=RS(42);
		B1=26*E*sqrt(B1);
		B2=sqrt(B2);
		B2=B2*B2*B2;
		if(B1>=B2)
			continue;
		B1=sqrt(B1/B2);
		B1=sqrt(B1);
		if(B1<=0.1)
			B1=0.1;
		if(B1<=B)
			B=B1;
	}
	for(I=37;I<=40; I++)
		RS(I)=B*RS(I);
	if(ASM1(T,4,4,T, &RS(37), &B))
		return BAD;
	if(B<=0.2) goto Lb15;
	TS1(9)=T(1);
	TS1(10)=T(2);
	TS2(9)=T(3);
	TS2(10)=T(4);
	goto Lb21;
Lb15: TS1(1)=T(1);
	TS1(2)=T(2);
	TS2(1)=T(3);
	TS2(2)=T(4);
	Ivar=0;
	if( SCSF3(RS,IS1,IS2,TS1,TS2,&Ivar,EQV,E, &RS(43),A1,A2))
		return BAD;
	R1=0;
	for(I=1;I<=3; I++)
		R1=R1+(RS(I)-RS(18+I))*(RS(I)-RS(18+I));
	if(R1>E*E) goto Lb3;
	*IPR=5;
//....ЗАНЕСЕНИЕ ТОЧКИ В РЕЗУЛЬТАТ
Lb21: for(I=1;I<=6; I++)
		RES(I)=RS(I);
	return	OK;
}

#define F(x) F[x-1]
#define INT(x) INT[x-1]
#define M(x) M[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCSF3(double RS[30],int* IS1, int* IS2,double* TS1,double* TS2,\
		   int* IPR, double EQV[2], double E, double* PT, double* A1,double* A2)
{
//****PACЧET TOЧKИ ПEPECEЧEHИЯ КООРД. ЛИНИИ С ПOB-ТЬЮ
//****BEPCИЯ: 01 (03.03.97)
	static double F[17];
	static double R[3];
	static int M[3]={29,41,51};
	int INT[98]={3,3,1,1,901,407,-113,108,-114,508,-115,608,2000,0,
		3,1,3,1,901,405,-113,101,-114,106,-115,107,2000,0,
		1,8,1001,711,1002,701,-113,205,-114,206,2000,0,
		1,8,1001,711,1002,701,-113,205,2000,0,
		1,8,1001,711,1002,701,-114,206,2000,0,
		1,8,901,408,1002,111,-113,1002,-114,902,2000,0,
		3,-1,-1,2,601,604,602,705,-313,102,2000,0,
		3,4,1,-1,901,101,804,104,1401,708,605,109,2000,0};
	IRAS=0;
//....КОНТРОЛЬ УПРАВЛЯЮЩИХ ПАРАМЕТРОВ
	if(IPR==0){
		message_error_("IPR==NULL");
		return BAD;
	}
	if(*IPR<0 || *IPR>3){
		message_error_("НОМЕP ВАРИАНТА НЕ [0-3]");
		IRAS=1;
		return BAD;
	}
	if(E<=0.00001){
		message_error_("ЗАДАНА ТОЧНОСТЬ < 0.00001");
		IRAS=1;
		return BAD;
	}
//....ПОДГОТОВКА ИТЕРАЦИИ
	int L3=IS1(3);
	int L2=IS2(3);
	IS1(3)=22;
	IS2(3)=22;
	F(10)=0;
	F(11)=40;
	F(14)=3;
	double E1=E*E;
	TS1(18)=TS1(13);
	TS1(19)=TS1(14);
	TS2(18)=TS2(13);
	TS2(19)=TS2(14);
	int K=0;
	int I=0;
	int IP=0;
	int L1=61;
//....ИТЕРАЦИИ
Lb1: if(INPH(&RS(7),IS1,TS1,EQV(1),A1))
		goto Lb101;

	if(I!=0) goto Lb3;
	I=1;
Lb2: if( INPH(&RS(19),IS2,TS2,EQV(2),A2))
		goto Lb101;

	if(*IPR==0) goto Lb20;
	double Y[3];
Lb3: if(ASVOPM(R,&INT(14*I-13), &RS(7), &RS(19), &RS(28),Y,Y,Y,Y,Y))
		goto Lb101;

	if(I==2) goto Lb4;
	F(1)=R(1)*R(1);
	F(2)=2*R(1)*R(2);
	F(3)=2*R(1)*R(3);
	if(IP==1) goto Lb5;
	if(F(1)<=E1) goto Lb9;
	goto Lb5;
Lb4: F(1)=R(1);
	if(F(1)<=E1) goto Lb20;
	F(2)=-2*R(2);
	F(3)=-2*R(3);
Lb5: if(K==0) goto Lb6;
	F(17)=0;
	if(I==1) goto Lb7;
	if(ASVOPM(&F(17),&INT(73),&F(6),&F(7),&RS(12*I-2),Y,Y,Y,Y,Y))
		goto Lb101;

	F(17)=0.3*sqrt(F(1))*F(17);
	goto Lb8;
Lb6: L1=61;
	if(I!=2)
		L1=M(*IPR);
	F(9-*IPR)=0;
	if(ASVOPM(&F(6),&INT(L1),&RS(7),Y,Y,Y,Y,Y,Y,Y))
		goto Lb101;

	if(I!=2) goto Lb7;
	F(6)=-F(6);
	goto Lb8;
Lb7: if( ASM6W(F,TS1,&K,2))
		 goto Lb11;
	if(K==0) goto Lb9;
	goto Lb1;
Lb8: if(ASM6W(F,TS2,&K,2))
		 goto Lb11;
	if(K==0) goto Lb9;
	goto Lb2;
//....CЕМАФОР
Lb9: IP=0;
Lb10: I=3-I;
	K=0;
	goto Lb3;
Lb11: if(IP!=0)
		goto Lb100;
	IP=1;
	goto Lb10;
//....ОФОРМЛЕНИЕ РЕЗУЛЬТАТА
Lb20: if( ASVOPM(RS,&INT(85),&RS(7),&RS(28),PT,Y,Y,Y,Y,Y))
		  goto Lb101;
	IS1(3)=L3;
	IS2(3)=L2;
	IRAS=0;
	return OK;
Lb101: IRAS=1;
Lb100:	IS1(3)=L3;
	IS2(3)=L2;
	return BAD;
}

