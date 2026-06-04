////////////	реализация функций Астры на C
/////////	начало 24.07.2000 конец  25.07.
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

#define F(x) F[x-1]
#define TS(x) TS[x-1]
#define R(x) R[x-1]
#define RS(x) RS[x-1]
#define IS(x) IS[x-1]
#define KIN(x) KIN[x-1]
#define M(x) M[x-1]
#define D(x) D[x-1]

extern BOOL INPM(double RES[24],int  ISP[8],double  UV[6],double* A);
extern BOOL ASM1(double TSR[4],int K, int L, double TS[12], double D[4], double* B);
extern int ASM2(double F[5],double RT[8], int* K);
extern BOOL ASSHG2(int* IPR, double D[2], double UV[6]);
extern BOOL ASM6W(double F[17], double TS[22], int* K1, int K);

BOOL INPH(double RS[30],int IS[8], double TS[22], double EQV, double* A);
BOOL ASM6WM(double F[17], double TS[22], int* K1, int K);
BOOL SCPL1(int* IPR, int IS[8], double TS[6], double EQV, double PL[6], double* A);
BOOL SCPL2(double RES[7],int IS[8],double TS[22],int* IPR,double P[6],double E,double EQV,double* A);
BOOL SCPL3(double RS[15],int IS[8],double  TS[22],int IPR,double P[6],double E,double EQV,double* PT,double* A);

BOOL ASLM(double RES[12], int IS[30], double TS[22], double PR[6],double EQV, double E, double* A)
{
//****PACЧET TOЧKИ ПEPECEЧEHИЯ ПPЯMOЙ C ПOBEPXHOCTЬЮ
//****BEPCИЯ: 00 (19.02.97)
	static double RS[12];
	static double F[17];
	int KIN[36]={2,3,2,901,407,1001,800,-113,0,1000,805,-114,1,1000,806,
				-115,1,-116,808,2000,0,
				2,4,2,900,408,1000,9,1801,700,-113,6,-114,105,2000,0};
	double Y[3]={7777777777., 1,1};
	if(E<0.0001){
		message_error_("ЗАДАНА ПОГРЕШНОСТЬ < 0.0001!");
		return BAD;
	} 
	int K1=0;
	F(11)=10;
	F(14)=6;
	F(10)=0;
	F(17)=.15;
	TS(13)=1;
	TS(14)=0.01;
	int L=IS(3);
	IS(3)=22;
Lb1: if( INPH(RS,IS,TS,EQV,A)) goto Lb100;
	if( ASVOPM(F,KIN,RS,PR,Y,Y,Y,Y,Y,Y)) goto Lb100;
	if(F(1)<=E*E*F(4)) goto Lb4;
	F(2)=2*F(2);
	F(3)=2*F(3);
	if(K1!=0) goto Lb3;
Lb2: if( ASVOPM(&F(6), &KIN(22),RS,PR,Y,Y,Y,Y,Y,Y)) goto Lb100;
	F(6)=-F(6);
Lb3: if( ASM6WM(F,TS, &K1, 2)) goto Lb100;
	if(K1==0) goto Lb2;
	goto Lb1;
Lb4: IS(3)=L;
	if( INPH(RES,IS,TS,EQV,A)) goto Lb100;
	return OK;
Lb100:	IS(3)=L;
	return BAD;
}
//***************************************************************
//C
//***************************************************************
BOOL INPH(double RS[30],int IS[8], double TS[6], double EQV, double* A)
{
//****BЫЧИCЛEHИE ЗHAЧEHИЙ ЭKBИДИCTAHTЫ ПOBEPXHOCTИ
//****BEPCИЯ: 01 (19.02.97)
	double R[30];
	double Y[3]={7777777777., 1,1};
	int M[16]={1,9,19,29,41,52,81,110,6,9,9,12,6,9,9,12};
	int KIN[152]={1,3,1405,506,1904,400,2000,0,
				1,3,1406,506,1904,400,1905,500,2000,0,
				1,3,1406,506,1904,400,1905,600,2000,0,
				1,3,1407,506,1904,400,1905,500,1906,600,2000,0,
				2,3,-1,1405,506,600,7,804,4,2000,0,
				2,6,-1,1403,507,501,507,1002,607,1000,509,800,200,700,1,
				1800,300,600,10,805,5,600,310,804,400,1906,300,2000,0,
				2,6,-1,1403,507,501,507,1002,907,1000,508,800,200,700,1,
				1800,300,600,10,805,7,600,310,804,400,1906,300,2000,0,
				2,6,-1,1403,507,501,507,1002,607,1000,509,800,200,700,1,
				1800,300,600,10,805,5,1002,907,1000,508,800,200,700,1,
				1800,300,600,10,806,7,600,310,804,400,1907,300,2000,0};
	int L=IS(3);
	int I=0;
	int J=0;
	int KK;
	if(L==11 || L==111) I=1;
	if(L==21 || L==121) I=2;
	if(L==12 || L==112) I=3;
	if(L==22 || L==122) I=4;
	if(I==0){
		message_error_("OШИБKA B ПPИЗHAKE PEЗУЛЬTATA!");
		IS(3)=L;
		return BAD;
	} 
	if(EQV!=0) 
		I=I+4;
	int K=0;
	if(L>100) 
		K=100;
	IS(3)=22+K;
	if(I>=6) 
		IS(3)=-33-K;
	K=M(I);
	if( INPM(R,IS,TS,A)){
		IS(3)=L;
		return BAD;
	}
	if( ASVOPM(RS, &KIN(K),R, &EQV,Y,Y,Y,Y,Y,Y)){
		IS(3)=L;
		return BAD;
	}
	if(I<6) goto Lb1;
	J=4;
	if(I==7) 
		J=10;
	if(R(J)*RS(4)+R(J+1)*RS(5)+R(J+2)*RS(6)<=0){
		IS(3)=L;
		message_error_("ОСОБАЯ  ТОЧКА  ЭКВИДИСТАНТЫ ПОВ-ТИ!");
		return BAD;
	} 
	if(I==8 && R(10)*RS(7)+R(11)*RS(8)+R(12)*RS(9)<=0){
		IS(3)=L;
		message_error_("ОСОБАЯ  ТОЧКА  ЭКВИДИСТАНТЫ ПОВ-ТИ!");
		return BAD;
	} 
Lb1: if(L<100) goto Lb3;
	K=9;
	if(I>=6) 
		K=18;
	KK=M(8+I);
	for( J=1; J<=6; J++)
		RS(KK+J)=R(K+J);
Lb3: IS(3)=L;
	return OK;
}


//***************************************************************
//C
//***************************************************************
BOOL ASM6WM(double F[17], double TS[22], int* K1, int K)
{
//****MИHИMИЗAЦИЯ B HAПPABЛEHИИ
//****BEPCИЯ: 02 (25.07.97)
	static double D[4];
	static double T;
	double A=0;
	double R=0;
	int I=1;
	int IRAS=0;
	if(*K1!=0) goto Lb5;
	if(F(10)!=0) goto Lb1;
	if(F(11)<1 || F(11)>50){
		message_error_("ПРЕДЕЛ ЧИСЛА ИТЕРАЦИЙ НЕ [1-50]!\nASM6WM");
		return BAD;
	} 
	TS(18)=TS(13);
	TS(19)=TS(14);
Lb1: A=0;
	R=0;
	for (I=1; I<=K; I++)
		R=R+F(I+1)*F(I+5);
	if(R<0) goto Lb19;
	for (I=1; I<=K; I++)
		F(I+5)=-F(I+5);
Lb19:	if( ASM1(TS, 1, K, TS, &F(6), &A))
			return BAD;
	R=0;
	for (I=1; I<=K; I++)
		R=R+F(5+I)*F(5+I);
	R=sqrt(R);
	if(R>0) goto Lb3;
	if(A<0){
		message_astra("BЫXOД B УГЛ. TOЧKУ OБЛACTИ!\nASM6WM");
		return BAD;
	} 
	message_astra("HAПPABЛEHИE HE OПPEДEЛEHO!\nASM6WM");
	return BAD;
Lb3:	for (I=1; I<=K; I++)
			F(5+I)=F(5+I)/R;
	TS(15)=0;
	TS(17)=10*TS(13);
	TS(16)=-TS(17);
Lb5: F(13)=0;
	F(12)=F(1);
	R=0;
	for (I=1; I<=K; I++){
		R=R+F(1+I)*F(1+I);
		F(13)=F(13)+F(1+I)*F(5+I);
	}
	T=TS(15);
	if(*K1<=2) goto Lb7;
	if(F(12)>F(15)) goto Lb7;
	if(fabs(F(13))<=F(17)*sqrt(R)) goto Lb10;
Lb7: IRAS= ASM2(&F(12), &TS(15), K1);
	if(IRAS!=0)
		goto Lb12;
	for (I=1; I<=K; I++)
		D(I)=(TS(15)-T)*F(5+I);
	if( ASM1(TS, 2, K, TS, D, &A))
		return BAD;
	TS(22)=TS(22)*A;
	TS(15)=T+TS(22);
	if(TS(15)<TS(20)) 
		TS(15)=TS(20);
	if(TS(15)>TS(21)) 
		TS(15)=TS(21);
	return OK;
Lb12: if(IRAS<=2) goto Lb10;
	  if(IRAS!=5){
		message_error_("IRAS!=5!\nASM6WM");
		return BAD;
	  }
Lb10: *K1=0;
	F(10)=F(10)+1;
	if(F(10)>F(11)){
		message_astra("BЫXOД ПO ЧИCЛУ ИTEPAЦИЙ!\nASM6WM");
		return BAD;
	} 
	R=fabs(TS(15));
	if(TS(19)>R && R!=0) 
		TS(19)=R;

	return OK;
}
	
//***************************************************************
//C
//***************************************************************
BOOL ASSHG(int IPR, double* T, double TN, double TK, double DT)
{
//****ШАГ ПО ПАРАМЕТРУ
//****BEPCИЯ: 07 (11. 3.97)
	double R;
	int I=1;
	int J=1;
	 if((*T-TN)*(TK-*T)<0){
		message_error_("ТЕКУЩЕЕ Т ВНЕ [Тн, Тк]!\nASSHG");
		return BAD;
	} 
	 if(DT<=0){
		message_error_("ШАГ ВНЕ ИНТЕРВАЛА [0,1]!\nASSHG");
		return BAD;
	} 
	int ICURS=1;
	if(TK<TN)  
		ICURS=-1;
	R=*T+DT*ICURS;
	if(IPR==0) goto Lb4;
	if(IPR!=1 && IPR!=2){
		message_error_("ТИП ШАГА НЕ (0,1,2)!\nASSHG");
		return BAD;
	} 
//....ШАГ С ПРОХОЖД. ЦЕЛОЧИСЛ. ЗНАЧ.
	if(DT>1){
		message_error_("ШАГ ВНЕ ИНТЕРВАЛА [0,1]!\nASSHG");
		return BAD;
	} 
	I=(int)*T;
	for(J=1; J<=2; J++){
		if((I-*T)*(R-I)>0) goto Lb2;
		I=I+ICURS;
	}
	I=(int)R;
	if(I==R) goto Lb3;
	goto Lb4;
Lb2: R=I;
Lb3: if(IPR==1)
		 R=R+1.E-10*ICURS;
Lb4: if((TK-R)*ICURS<0)
		R=TK;
	*T=R;
	return OK;
}
#define DK(x) DK[x-1]
#define ISP(x) ISP[x-1]
#define UV(x) UV[x-1]
#define PR(x) PR[x-1]

//***************************************************************
//C
//***************************************************************
BOOL INVLLM(int ISP[8], double TS[6], double PR[6], double DK[2], double EQV, double* A)
{
//****ВЫБОР НАЧ. ПРИБЛИЖЕНИЯ ДЛЯ ПРЯМОЙ пересек-ся с плоскостью
//****BEPCИЯ: 01 (15.02.97)
	double RS[6];
	double D[2];
	double UV[6];
	double R;
	int I=1;
	for( I=1; I<=2; I++){
		if(DK(I)<1 || DK(I)>100){
			message_error_("ЧИСЛО РАЗБИЕНИЙ НЕ [1-100]!\nINVLLM");
			return BAD;
		} 
		D(I)=1/DK(I);
	}
	if(ISP(8)<42) 
		D(2)=D(2)*0.25;
	int LL=ISP(3);
	ISP(3)=11;
	int IP=1;
	for(I=3; I<=6; I++)
		UV(I)=TS(I);
	double Q=1000000;
Lb3: if( ASSHG2(&IP,D,UV))
		 return BAD;
	if( INPH(RS,ISP,UV,EQV,A))
		 return BAD;
	if( VOPC(&R,11, RS, PR, &PR(4)))
		 return BAD;
	if(R>=Q) goto Lb4;
	Q=R;
	TS(1)=UV(1);
	TS(2)=UV(2);
Lb4: if(IP!=3) goto Lb3;
	ISP(3)=LL;
	return OK;
}

#define B(x,y) B[x-1+(y-1)*7]
#define ST(x,y) ST[x-1+(y-1)*2]
#define T(x) T[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCPL(int* NRES, double* B, double* ST, int IPR, int IS[8], double T[22], double PL[6], double EQV, double E, double* A)
{
//****РАЧЕТ ПЛОСКОГО СЕЧЕНИЯ ЗАДАННОГО СЕГМЕНТА ПОВЕРХНОСТИ 
//****BEPCИЯ: 00 (15.02.97)
	double R[3];
	int KIN[15]={2,2,2,801,507,1302,406,-113,101,-114,102,-415,406,2000,0};
	double Y[3];
	double Q=0;
	 if(*NRES<7){
			message_error_("ПОЛЕ РЕЗУЛЬТАТА МАЛО!");
			return BAD;
		} 
	int IP=IPR;
	if( SCPL1(&IP,IS,T,EQV,PL,A))
		return BAD;
	int J=1;
Lb1: if( SCPL2(&B(1,J),IS,T, &IP,PL,E,EQV,A))
		return BAD;
	B(7,J)=0;
	ST(1,J)=T(1);
	ST(2,J)=T(2);
	if(J==1) goto Lb2;
//....подсчет параметра
	if( ASVOPM(R,KIN,&B(1,J),&B(1,J-1),Y,Y,Y,Y,Y,Y))
		return BAD;
	Q=16+R(2)*R(2)-R(1);
	B(7,J)=B(7,J-1)+6*R(3)/(sqrt(Q)+R(2));
//....конец цикла по числу точек сечения 
Lb2: J=J+1;
	if(J>*NRES-5){
			message_error_("ПOЛE PEЗУЛЬTATA ИCЧEPПAHO!");
			return BAD;
		} 
	if(IP!=5) goto Lb1;
	*NRES=J-1;
	return OK;
}

#define TK(x,y) TK[x-1+(y-1)*4]
#define PL(x) PL[x-1]

//***************************************************************
//C
//***************************************************************	
BOOL SCPL1(int* IPR, int IS[8], double TS[6], double EQV, double PL[6], double* A)
{
//****ПOИCK НАЧ. ПРИБЛИЖЕНИЯ ДЛЯ 1-ОЙ ТОЧКИ ПЛОСКОСКОГО СЕЧЕНИЯ
//****BEPCИЯ: 01 (10.12.99)
	double RS[6];
	double TK[8];
	int II=__IntAbs(*IPR);
	if(II<1 || II>5){
		message_error_("ПРИЗНАК ОРИЕНТАЦИИ НЕ [1-5]!");
		return BAD;
	} 
	IS(3)=11;
	double SH=1;
	int NP=1;
	int JJ=0;
	int K=1;
	double D;
	double Q;
	double G;
Lb10: TS(1)=TS(3);
	TS(2)=TS(4);
	NP=1;
	JJ=0;
//....проход по точкам линии V=Vн
	K=1;
Lb1: goto Lb20;
Lb2: if(TS(1)==TS(5)) goto Lb3;
	if( ASSHG(1,&TS(1),TS(3),TS(5),SH))
		return BAD;
	goto Lb1;
//....проход по точкам линии U=Uк
Lb3: D=SH;
	if(IS(7)<42) 
		D=D*0.2;
	K=4;
Lb4: if( ASSHG(1,&TS(2),TS(4),TS(6),D))
		return BAD;
	goto Lb20;
Lb5: if(TS(2)!=TS(6)) goto Lb4;
//....проход по точкам линии V=Vк
	K=2;
Lb6: if( ASSHG(1,&TS(1),TS(5),TS(3),SH))
		return BAD;
	goto Lb20;
Lb7: if(TS(1)!=TS(3)) goto Lb6;
//....проход по точкам линии U=Uн
	D=SH;
	if(IS(7)<42) 
		D=D*0.2;
	K=3;
Lb8: if( ASSHG(1,&TS(2),TS(6),TS(4),D))
		return BAD;
	goto Lb20;
Lb9: if(TS(2)!=TS(4)) goto Lb8;
	 if(SH<1){
		message_astra("HET ПEPECEЧEHИЯ С ГРАНИЦЕЙ!");
		return BAD;
	} 
	SH=0.1;
	goto Lb10;
//....общий блок анализа условия пересечения
Lb20: if( INPH(RS,IS,TS,EQV,A))
		return BAD;
	Q=(RS(1)-PL(1))*PL(4)+(RS(2)-PL(2))*PL(5)+(RS(3)-PL(3))*PL(6);
	if(JJ==0){
	   G=Q;
	   JJ=1;
	}
	if(Q*G<=0){
	   TK(1,NP)=TS(1);
	   TK(2,NP)=TS(2);
	   if(II<=3) 
		   TK(3,NP)=RS(II);
	   TK(4,NP)=K;  
	   NP=NP+1;
	   if(NP>2) goto Lb30;
	   G=Q;
	}
	switch (K){
	case 1:
		goto Lb2;
	case 2:
		goto Lb7;
	case 3:
		goto Lb9;
	case 4:
		goto Lb5;
	}
//....возвращение результата
Lb30: K=1;
	if(II<=3){
	   if(*IPR>0 && TK(3,1)>TK(3,2))
		   K=2;
	   if(*IPR<0 && TK(3,1)<TK(3,2))
		   K=2;
	}
	if(II>3) {
	   int I=II-3;
	   if(*IPR>0 && TK(I,1)>TK(I,2)) K=2;
	   if(*IPR<0 && TK(I,1)<TK(I,2)) K=2;
	}
	TS(1)=TK(1,K);
	TS(2)=TK(2,K);
	*IPR=(int)TK(4,K);
	return OK;
}
#define KT(x) KT[x-1]
#define IJ(x) IJ[x-1]
#define RES(x) RES[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCPL2(double RES[7],int IS[8],double TS[22],int* IPR,double P[6],double E,double EQV,double* A)
{
//****РАСЧЕТ ОЧЕРЕДНОЙ ТОЧКИ ПЛОСКОГО СЕЧЕНИЯ ПOBEPXHOCTИ
//****BEPCИЯ: 00 (19.02.97)
	static double RS[29];
	int KT[4]={4,6,3,5};
	static int IJ[2];
	int KIN[43]={1,5,901,101,1002,506,903,102,-104,803,-105,702,
			1104,100,2000,0,
			1,5,803,508,902,704,-215,200,603,300,802,202,-416,203,
			2000,0,2,1,2,901,405,1102,600,-113,102,2000,0};
	double Y[3]={7777777777.,1,1};
	double B,B1,B2;
	int K=1;
	int LPNT=0;
	if(*IPR<0 || *IPR>4){
		message_error_("НОМЕР ВАРИАНТА НЕ [0-4]!");
		return BAD;
	} 
	if(*IPR==0) goto Lb2;
//....В ПЕРВОЙ ТОЧКЕ СЕЧЕНИЯ
	RS(29)=1;
	TS(7)=1;
	TS(8)=0.01;
	K=(*IPR+1)/2;
	TS(3-K)=TS(KT(*IPR));
	*IPR=K;
	LPNT=0;
	goto Lb3;
//....ПОСЛЕДУЮЩИЕ ТОЧКИ
Lb2: TS(1)=TS(9);
	TS(2)=TS(10);
	LPNT=1;
Lb3: K=*IPR+1;
Lb4: if( SCPL3(RS,IS,TS,K,P,E,EQV,&RS(29),A))
		 return BAD;
	if( ASVOPM(&RS(25),KIN,RS,Y,Y,Y,Y,Y,Y,Y))
		 return BAD;
	if(*IPR==0) goto Lb5;
	if( ASM1(TS,3,2,TS,&RS(25),&RS(29)))
		 return BAD;
	*IPR=0;
	K=0;
	goto Lb4;
Lb5: TS(9)=TS(1);
	TS(10)=TS(2);
	IJ(1)=IS(1);
	IJ(2)=IS(2);
	TS(1)=TS(1)+RS(25);
	TS(2)=TS(2)+RS(26);
	IS(1)=2;
	IS(2)=2;
	if( SCPL3(&RS(10),IS,TS,0,P,E,EQV,&RS(29),A))
		 return BAD;
	TS(1)=TS(9);
	TS(2)=TS(10);
	IS(1)=IJ(1);
	IS(2)=IJ(2);
	if( ASVOPM(&RS(25),&KIN(17),RS,Y,Y,Y,Y,Y,Y,Y))
		 return BAD;
	B1=26*E*sqrt(RS(27));
	B2=sqrt(RS(28));
	B2=B2*B2*B2;
	if(B1>=B2) goto Lb6;
	B=sqrt(B1/B2);
	B=sqrt(B);
	if(B<=0.1)
		B=0.1;
	RS(25)=RS(25)*B;
	RS(26)=RS(26)*B;
Lb6: if( ASM1(&TS(9),4,2,TS,&RS(25), &B))
		 return BAD;
	if(B>0.1 || LPNT==0) goto Lb7;
	TS(1)=TS(9);
	TS(2)=TS(10);
	if( SCPL3(RS,IS,TS,0,P,E,EQV,&RS(29),A))
		 return BAD;
	if( ASVOPM(&RS(28),&KIN(33),RS,P,Y,Y,Y,Y,Y,Y))
		 return BAD;
	if(fabs(RS(28))>E) goto Lb3;
	*IPR=5;
Lb7: for(int I=1; I<=6; I++)
		RES(I)=RS(I);
	return OK;
}

#define P(x) P[x-1]
#define NU(x) NU[x-1]

//***************************************************************
//C
//***************************************************************
BOOL SCPL3(double RS[15],int IS[8],double  TS[22],int IPR,double P[6],double E,double EQV,double* PT,double* A)
{
//****PACЧET TOЧKИ ПЛОСКОГО СЕЧЕНИЯ ПOBEPXHOCTИ
//****BEPCИЯ: 00 (19.02.97)
	static double F[17];
	static double R[3];
	int M[3]={1,14,25};
	int NU[48]={2,3,2,901,407,-113,108,-114,508,-115,608,2000,0,
			2,2,2,901,406,-113,107,-114,507,2000,0,
			2,3,2,901,407,-113,108,-115,608,2000,0,
			2,3,1,1001,706,1002,601,-113,204,-114,205,2000,0};
	double Y[3]={7777777777.,1,1};
	double B;
	int I,J,K;
	if(E<1E-4){
		message_error_("ЗAДAHA ПOГPEШHOCTЬ < 0.0001!");
		return BAD;
	} 
	if(*PT*(*PT)!=1){
		message_error_("ПРИЗНАК ОРИЕНТАЦИИ КРИВОЙ НЕ 1!");
		return BAD;
	} 
	if(IPR>3 || IPR<0){
		message_error_("НОМЕР ВАРИАНТА НЕ [0-3]!");
		return BAD;
	} 
	int L=IS(3);
	IS(3)=22;
	if(IPR==0) goto Lb2;
//....УСТАНОВКА НАЧ. ЗНАЧЕНИЙ
	K=0;
	F(10)=0;
	F(11)=20;
	F(14)=6;
	F(17)=0;
	TS(13)=1;
	TS(14)=0.01;
	J=M(IPR);
	B=(P(4)*P(4)+P(5)*P(5)+P(6)*P(6))*E*E;
	for(I=1; I<=3; I++)
		R(I)=0;
//....ИТЕРАЦИЯ
Lb2: if( INPH(RS,IS,TS,EQV,A))
		 return BAD;
	if(IPR==0) goto Lb4;
	if( ASVOPM(R, &NU(J),RS,P,Y,Y,Y,Y,Y,Y))
		 return BAD;
	F(1)=R(1)*R(1);
	if(F(1)<=B) goto Lb4;
	if(K!=0) goto Lb3;
	if( ASVOPM(&F(6),&NU(36),&RS(4),&P(4),Y,Y,Y,Y,Y,Y))
		 return BAD;
	if(IPR!=1) 
		F(9-IPR)=0;
Lb3: F(2)=2*R(1)*R(2);
	F(3)=2*R(1)*R(3);
	if( ASM6W(F,TS,&K,2)){
		IS(3)=L;
		return BAD;
	}
	goto Lb2;
//....ОФОРМЛЕНИЕ РЕЗУЛЬТАТА
Lb4: for(I=7; I<=9; I++){
		RS(6+I)=RS(I);
		RS(I)=RS(3+I);
		RS(3+I)=RS(I-3);
		 }
	if( VOPC(&RS(4),13,&P(4),&RS(7), PT))
		 return BAD;
	IS(3)=L;
	return OK;
}

