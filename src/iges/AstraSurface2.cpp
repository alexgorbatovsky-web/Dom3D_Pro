////////////	реализация функций Астры на C
/////////	начало 18.07.2000 конец 21.07 Ура.
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

extern BOOL INPM(double RES[24],int  ISP[8],double  UV[6],double* A);

#define DK(x) DK[x-1]
#define D(x) D[x-1]
#define ISP(x) ISP[x-1]
#define UV(x) UV[x-1]
#define TS(x) TS[x-1]
#define TSR(x) TSR[x-1]
#define RT(x) RT[x-1]

extern BOOL ASM6WM(double F[17], double TS[22], int* K1, int K);

BOOL ASSHG2(int* IPR, double D[2], double UV[6]);
BOOL ASM6W(double F[17], double TS[22], int* K1, int K);
BOOL ASM1(double TSR[4],int K, int L, double TS[12], double D[4], double* B);
int ASM2(double F[5],double RT[8], int* K);



//C****************************************************************
BOOL INVLTM(int ISP[8],double TS[6], double B[3], double DK[2], double* A)
{
//C*****ВЫБОР НАЧ. ПРИБЛИЖЕНИЯ ДЛЯ ТОЧКИ
//C*****BEPCИЯ: 00 (31.01.97)
	double RES[3];
	double D[2];
	double UV[6];
	if(DK(1)<1 || DK(1)>100){
	    message_error_("ЧИСЛО РАЗБИЕНИЙ НЕ [1-100]!");
	    return BAD;
	    }
	if(DK(2)<1 || DK(2)>100){
	    message_error_("ЧИСЛО РАЗБИЕНИЙ НЕ [1-100]!");
	    return BAD;
	    }
	D(1)=1/DK(1);
	D(2)=1/DK(2);
	if(ISP(7)<42) 
		D(2)=D(2)*0.25;
	int LL=ISP(3);
	ISP(3)=11;
	int IP=1;
	for(int I=3; I<=6; I++)
		UV(I)=TS(I);
	double H=1000000;
Lb2: if( ASSHG2(&IP,D,UV)){
		ISP(3)=LL;
		return BAD;
	 }
	 if( INPM(RES,ISP,UV,A)){
		ISP(3)=LL;
		return BAD;
	 }
	 double R;
	 if( VOPB(&R,6,B,RES)){
		ISP(3)=LL;
		return BAD;
	 }
	if(R>=H) goto Lb3;
	H=R;
	TS(1)=UV(1);
	TS(2)=UV(2);
Lb3: if(IP!=3) goto Lb2;
	ISP(3)=LL;
	return OK;
}
//C****************************************************************
BOOL ASSHG2(int* IPR, double D[2], double UV[6])
{
//C****************************************************************
//C*****ПОСЛЕДОВАТЕЛЬНЫЙ РАСЧЕТ УЗЛОВ СЕТКИ
//C*****BEPCИЯ: 05 (11.03.97)

	double T;
	int I=1;
	if(*IPR==2) goto Lb2;
	if(*IPR!=1){
	    message_error_("ПРИЗНАК СОСТОЯНИЯ НЕ [1-2]!");
	    return BAD;
	    }
	for(I=1; I<=2; I++){
		UV(I)=UV(I+2);
		if(UV(I+2)<=UV(I+4)) goto Lb1;
		UV(I+2)=UV(I+4);
		UV(I+4)=UV(I);
Lb1: UV(I)=UV(I+2);
	}
	*IPR=2;
	return OK;
//.....ПРОДОЛЖЕНИЕ
Lb2: int L=1;
	T=UV(1);
	double RD=D(1);
	if(T<UV(5)) goto Lb3;
	L=2;
	UV(1)=UV(3);
	T=UV(2);
	RD=D(2);
Lb3: if(RD<=0){
	    message_error_("ШАГ НЕ ПОЛОЖИТЕЛЕН!");
	    return BAD;
	    }
	I=(int)T+1024;
	T=T+RD;
	int J=int(T+0.1*RD+1024);
	if(I!=J) 
		T=I-1023;
	if(T<UV(L+4)) goto Lb4;
	T=UV(L+4);
Lb4: UV(L)=T;
	if(UV(1)>=UV(5) && UV(2)>=UV(6))
		*IPR=3;
	return OK;
}

#define F(x) F[x-1]
#define RS(x) RS[x-1]
#define IS(x) IS[x-1]
#define KIN(x) KIN[x-1]

//C****************************************************************
//C
//C****************************************************************
BOOL ASTPVM(int IS[8], double TS[22], double R[3], double E, double* A)
	{
//C*****РАСЧЕТ TOЧKИ ПOBEPXHOCTИ, БЛИЖAЙШЕЙ K ЗAДAHHOЙ
//C*****BEPCИЯ: 00 (31.01.97)
	double RS[9];
	double F[17];
	double Y[3];
	int KIN[46]={2,3,1,901,407,1402,506,-114,105,-115,106,-113,101,
			-516,102,2000,0,
			2,3,1,901,704,1402,506,1003,602,-113,103,1003,102,
			-114,503,2000,0,
			3,2,1,1,601,406,602,507,-313,102,2000,0};

	if(E<=0.0001){
	    message_error_("ЗАДАНА ПОГРЕШНОСТЬ <0.0001!");
	    return BAD;
	}
	int L1=IS(3);
	IS(3)=22;
	int K=0;
	F(10)=0;
	F(11)=10;///////число итераций
	F(14)=7;
	TS(13)=1;
	TS(14)=0.01;
Lb1: if( INPM(RS,IS,TS,A)) goto Lb100;
	if( ASVOPM(F,KIN,RS,R,Y,Y,Y,Y,Y,Y)) goto Lb100;
	if(F(4)<=E) goto Lb4;
	F(2)=2*F(2);
	F(3)=2*F(3);
	if(K==0) goto Lb2;
	if( ASVOPM(&F(17), &KIN(35), &RS(4), &F(6), &F(7),Y,Y,Y,Y,Y)) goto Lb100;
	F(17)=0.3*F(17)*F(4);
	goto Lb3;
Lb2: if( ASVOPM(&F(6), &KIN(18),RS,R,Y,Y,Y,Y,Y,Y)) goto Lb100;
Lb3: if( ASM6W(F,TS, &K,2)) goto Lb100;
	if(K==0) goto Lb2;
	goto Lb1;
Lb4: IS(3)=L1;
	return OK;
Lb100: IS(3)=L1;
	return BAD;
}



//C****************************************************************
//
//C****************************************************************
BOOL ASM6W(double F[17], double TS[22], int* K1, int K)
{
//C*****MИHИMИЗAЦИЯ B HAПPABЛEHИИ
//C*****BEPCИЯ: 09 (16.07.97)
	static double D[4];
	static double T;
	double R=0;
	double A=0;
	int IRAS=0;
	int I=1;
	if(*K1!=0) goto Lb5;
	if(F(10)!=0) goto Lb1;
	if(F(11)<1 || F(11)>50){
	    message_error_("ЧИСЛО ИТЕРАЦИЙ НЕ [1-50]!");
	    return BAD;
	}
	TS(18)=TS(13);
	TS(19)=TS(14);
Lb1: A=0;
	R=0;
	for (I=1; I<=K; I++)
		R=R+F(I+1)*F(I+5);
	if(R<0) goto Lb19;
	for ( I=1; I<=K; I++)
		F(I+5)=-F(I+5);
Lb19: if( ASM1(TS, 1, K, TS, &F(6), &A))
	    return BAD;
	R=0;
	for (I=1; I<=K; I++)
		R=R+F(5+I)*F(5+I);
	R=sqrt(R);
	if(R>0) goto Lb3;
	if(A<0){
	    message_astra("BЫXOД B УГЛ. TOЧKУ OБЛACTИ!");
	    return BAD;
	}
	message_astra("HAПPABЛEHИE HE OПPEДEЛEHO!");
	return BAD;
Lb3: for (I=1; I<=K; I++)
		F(5+I)=F(5+I)/R;
	TS(15)=0;
	TS(17)=10*TS(13);
	TS(16)=-TS(17);
Lb5: F(13)=0;
	F(12)=F(1);
	for (I=1; I<=K; I++)
		F(13)=F(13)+F(1+I)*F(5+I);
	T=TS(15);
	if(*K1<=2) goto Lb7;
	if(F(12)>F(15)) goto Lb7;
	if(fabs(F(13))<=F(17)) goto Lb10;
Lb7: IRAS= ASM2(&F(12),&TS(15), K1);
	if(IRAS!=0)
		goto Lb12;
	for (I=1; I<=K; I++)
		D(I)=(TS(15)-T)*F(5+I);
	if( ASM1(TS,2,K,TS,D, &A))
		return BAD;
	TS(22)=TS(22)*A;
	TS(15)=T+TS(22);
	if(TS(15)<TS(20))
		TS(15)=TS(20);
	if(TS(15)>TS(21)) 
		TS(15)=TS(21);
	return OK;
Lb12: if(IRAS<=2) goto Lb10;
	if(IRAS!=5)
		return BAD;
Lb10: *K1=0;
	F(10)=F(10)+1;
	if(F(10)>F(11)){
		message_astra("Выход по числу итераций!\nASM6W");
		return BAD;
	}
	R=fabs(TS(15));
	if(TS(19)>R && R!=0) 
		TS(19)=R;
	return OK;

}

//C****************************************************************
//C
//C****************************************************************
BOOL ASM1(double TSR[4],int K, int L, double TS[12], double D[4], double* B)
{
//C*****OPИEHTAЦИЯ BEKTOPA OTHOCИTEЛЬHO CEГMEHTA
//C*****BEPCИЯ: 09 (31.01.97)
	static double R, B1, R1;
	static double P1,P2;
	int I,J, I1;
	int L1, L2;
	if(K!=1) 
		*B=1;
	switch (K){
	case 1:
		goto Lb1;
	case 2:
		goto Lb10;
	case 3:
		goto Lb1;
	case 4:
		goto Lb10;
	}
	message_error_("HОМЕР ВАРИАНТА НЕ [1-4]!\nASM1");
	return BAD;
//.....ВАРИАНТЫ 1,3 
Lb1: for(J=1; J<=2; J++){
		 for(I=1; I<=L; I++){
			L1=I+L;
			L2=L1+L;
			if(TS(I)==TS(L1)) goto Lb2;
			if(TS(I)!=TS(L2))
				continue;
			L2=L1;
			L1=L1+L;
	Lb2:	if((TS(L1)-TS(L2))*D(I)<=0)
				continue;
			if(K==3) goto Lb3;
			D(I)=0;
			goto Lb5;
	Lb3:	if(*B<0){
#ifdef _DEBUG
				message_error_("BEKTOP ПPOXOДИT BHE CEГMEHTA!\nASM1");
#endif
				return BAD;
			 }
			for(I1=1; I1<=L; I1++)
				D(I1)=-D(I1);
	Lb5:	*B=-1;
		 }

		if(K==1)
			return OK;
		if(*B>0)
			return OK;
	 }
	return OK;
//.....ВАРИАНТЫ 2,4 
Lb10: B1=1;
	I1=0;
	for(I=1; I<=L; I++){
		L2=I+L;
		L1=L2+L;
		if((TS(L2)-TS(L1))*D(I)<0)
			L2=L1;
		R=TS(L2);
		P1=fabs(R-TS(I));
		P2=fabs(D(I))**B;
		if(P2<P1) goto Lb11;
		if(P2==0)
			continue;
		*B=*B*P1/P2;
		R1=R;
		I1=I;
		continue;
Lb11:	if(K==2)
			continue;
		if(*B<1)
			continue;
		P1=0.66*P1;
		P2=P2*B1;
		if(P2<=P1)
			continue;
		B1=B1*P1/P2;
	}
	double C=*B*B1;
	for(J=1; J<=L; J++)
		TSR(J)=TS(J)+D(J)*C;
	if(I1!=0) 
		TSR(I1)=R1;
	return OK;
}


//C****************************************************************
//C
//C****************************************************************
int ASM2(double F[5],double RT[8], int* K)
{
//C*****MИHИMИЗAЦИЯ ФУHKЦИИ OДHOЙ ПEPEMEHHOЙ
//C*****BEPCИЯ: 15 (31.01.97)
	static double P,T;
	double DF, DT;
	double A, B, C;	
	int I=1, J=1;
	 if(*K>F(3)){
		message_astra("BЫXOД ПO ЧИCЛУ ИTEPAЦИЙ!\nASM2");

		char buf[120];
		sprintf(buf, "K=%d", *K );
		message_to_file(buf);
		return 5;
	 }
	if(*K!=0) goto Lb1;
	I=2;
	if(RT(2)>RT(3)) 
		I=3;
	RT(6)=RT(I);
	RT(7)=RT(5-I);
Lb1: T=RT(1);
	if(T<RT(6) || T>RT(7)){
		message_error_("HEДOПУCTИMOE ЗHAЧEHИE APГУMEHTA!\nASM2");
		return 3;
	 }
	if(*K!=0) goto Lb3;
//.....УCTAHOBKA ПAPAMETPOB B HAЧAЛЬHOE COCTOЯHИE
	if(F(3)<2 || F(3)>50){
		message_error_("1 ОШИБКА В УПРАВЛЯЮЩЕМ ПАРАМЕТРЕ!\nASM2");
		return 4;
	 }
	if(RT(4)<=0 || RT(5)<=0){
		message_error_("2 ОШИБКА В УПРАВЛЯЮЩЕМ ПАРАМЕТРЕ!\nASM2");
		return 4;
	 }
Lb2: DT=RT(5);
	F(4)=10E35;
	goto Lb7;
Lb3: DF=F(1)-F(4);
	 if((F(2)*F(5))==0){
		message_astra("TOЧKA ЛOKAЛЬHOГO ЭKCTPEMУMA\nASM2");
		return 1;
	 }
	if((F(2)*F(5))<0)
		goto Lb6;
	if(F(1)>=F(4)) goto Lb2;
//.....BЫЧИCЛEHИE ШAГA ПУTEM ЭKCTPAПOЛЯЦИИ
	A=2*DF+(5*F(2)-F(5))*RT(8);
	B=F(5)-F(2);
	if(A*B==0) goto Lb2;
	DT=fabs(A/B*0.1666);
	goto Lb7;
//.....ВЫЧИСЛЕНИЕ ШАГА ПУТЕМ ИНТЕРПОЛЯЦИИ ПО КУБИЧЕСКОЙ ПАРАБОЛЕ
Lb6:	DT=F(2)/(F(2)-F(5));
	P=F(2)*RT(8);
	A=(F(5)+F(2)+F(2))*RT(8)-3*DF;
	B=3*((F(5)+F(2))*RT(8)-DF-DF);
	C=fabs(A+sqrt(fabs(A*A-B*P)));
	if(C>fabs(P)) 
		DT=P/C;
	DT=fabs(RT(8)*DT);
//.....ВЫЧИСЛЕНИЕ ОЧЕРЕДНОЙ ТОЧКИ
Lb7: if(DT>RT(4)) 
		DT=RT(4);
	I=7;
	if(F(2)>0) 
		I=6;
	J=2*I-13;
	P=T+DT*J;
	if((P-RT(I))*J<=0) goto Lb8;
	P=RT(I);
Lb8: if(F(1)>=F(4)) goto Lb9;
	F(4)=F(1);
	F(5)=F(2);
	RT(8)=P-T;
	RT(13-I)=T;
	goto Lb10;
Lb9: RT(8)=P-RT(I);
Lb10: if(RT(8)==0){
		message_astra("BЫXOД HA ГPAHИЦУ ИHTEPBAЛA!\nASM2");
		return 2;
	 }	  
	  RT(1)=P;
	*K=*K+1;
	return OK;

}

