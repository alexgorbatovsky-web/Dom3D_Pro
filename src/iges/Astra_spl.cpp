////////////	реализация функций Астры на C
/////////	начало 19.06.2000
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

#define A(x) A[x-1]
#define AU(x) AU[x-1]
#define B(x,y) B[x-1+(y-1)*15]
#define FG(x) FG[x-1]
#define IB(x) IB[x-1]
#define IU(x) IU[x-1]
#define ISK(x) ISK[x-1]
#define LR(x) LR[x-1]
#define KU(x) KU[x-1]
#define RES(x,y) RES[x-1+(y-1)*3]
#define RS(x) RS[x-1]
#define T(x) T[x-1]


int VTK(double* RES, int KU[], double* AU,double T);
int WF(double* FG, double T,int L, int IX);
int VTKM(double RES[39], int KU[4],double* AU, double T);
int WFM(double* FG, double T,int L, int IX);
int STAFMM(int IPR, int N, int LR[4], double D, double* A );
extern  BOOL ASSHG(int IPR, double* T, double TN, double TK, double DT);
extern BOOL ASM6WM(double F[17], double TS[22], int* K1, int K);

//****************************************************************
//   Расчет коэффициентов сплайна
//****************************************************************
int STAFM2(int IPR, int N, int LR[4], double D, double* A )
{
//*****
//*****: 01 (25.02.92)
	double B[75];
	double* IB=&B[0];
	int KU[4]={7,0,1,12};
	double RS[14];
	int IU[37]={2,1,1,904,405,-216,0,2000,0,
				2,1,1,101,405,800,404,600,1,1304,5,2000,0,
				2,2,2,801,507,1302,406,-113,101,-114,102,-415,406,2000,0};
	double AL=1;

	if(IPR<0 || IPR>1){
		message_error_("Признак параметризации не 0-1!");
		return BAD;
	}
	if(N<2){
		message_error_("Число узлов сплайн-кривой <2!");
		return BAD;
	}
	if(N>1024){
		message_error_("Число узлов сплайн-кривой >1024!");
		return BAD;
	}
	if(LR(1)<1 || LR(1)>26){
		message_error_("ОШИБКА В ИНДЕКСАХ\nLR[0]<1 || LR[0]>26!");
		return BAD;
	}
	if(LR(2)<LR(1)+3 || LR(2)>29){
		message_error_("ОШИБКА В ИНДЕКСАХ\nLR(2)<LR(1)+3 || LR(2)>29!");
		return BAD;
	}
	if(LR(3)<LR(2)+3 || LR(3)>32){
		message_error_("ОШИБКА В ИНДЕКСАХ\nLR(3)<LR(2)+3 || LR(3)>32!");
		return BAD;
	}
	if(LR(4)<7){
		message_error_("ОШИБКА В ИНДЕКСАХ\nLR(4)<7!");
		return BAD;
	}
/*	if(LR(4)>6000){
		message_error_("ОШИБКА В ИНДЕКСАХ\n LR(4)>6000!");
		return BAD;
	}*/
	int I=1;
      for(I=1; I<=75; I++)
		IB(I)=0;
    int JT=0;
    int  JL=1;
	int J;
    double BE=1;
	double U=1;
	double S1=1;
	double S2=1;
	double S=1;
    double  PRM=A(LR(3));
    double H=B(6,1);
//C1.....
Lb10: for(I=1; I<=15; I++)
		IB(I)=0;
      JT=JT+1;
      if(JT>N) goto Lb20;
      J=(JT-1)*LR(4);
      if(IPR>0)
		  B(14,1)=A(LR(3)+J);
      for(I=7; I<=9; I++){
		  B(I,1)=A(LR(1)+J);
		  B(I+3,1)=A(LR(2)+J);
		 J=J+1;
	  }
      IB(1)=JT;
      if(JT==1) goto Lb16;
      if(IPR>0 && B(14,1)<=B(14,2)){
		message_error_("Параметр не растет!");
		return BAD;
	}
	double Y[3];
      if( ASVOPM(&B(3,1),IU,&B(7,1),&B(7,2),Y,Y,Y,Y,Y,Y))
		  return BAD;
     H=B(6,1);
      if(H!=0) goto Lb15;
      if(D<=0){
		message_error_("Узлы сплайна совпадают!\nD<=0 STAFM2");
		return BAD;
	}
      if(JL<=2 || JT==N){
		message_error_("JL<=2 || JT==N");
		return BAD;
	}
      JL=1;
      goto Lb16;
Lb15: if( VOPA(&B(3,1),3,&B(3,1)))
		  return BAD;
      if( VOPB(&B(15,2),3,&B(3,1),&B(3,2)))
		  return BAD;
Lb16: IB(2)=JL;
      JL=JL+1;
//C2.....
Lb20: if(IB(32)<=0) goto Lb40;
      if(fabs(B(10,3))+fabs(B(11,3))+fabs(B(12,3))!=0) goto Lb5;
      if(IB(32)==1) goto Lb30;
      if(IB(32)!=2) goto Lb21;
      if(IB(17)!=3) goto Lb23;
      if(IB(2)!=4) goto Lb25;
      goto Lb28;
Lb21: if(IB(32)!=3) goto Lb22;
      if(IB(17)!=4) goto Lb23;
      if(IB(2)!=5) goto Lb19;
      goto Lb9;
Lb22: if(IB(17)<=1) goto Lb23;
      if(IB(2)<=1) goto Lb19;
      goto Lb9;
//C3.....
Lb23: if(fabs(B(10,4))+fabs(B(11,4))+fabs(B(12,4))!=0) goto Lb24;
      B(10,3)=B(3,3);
      B(11,3)=B(4,3);
      B(12,3)=B(5,3);
      goto Lb5;
Lb24: if( ASVOPM(&B(10,3),&IU(10),&B(3,3),&B(10,4),Y,Y,Y,Y,Y,Y))
		  return BAD;
      goto Lb4;
//C4....
Lb25: AL=1;
      BE=1;
      goto Lb8;
//C5.....
Lb28: AL=1;
      BE=B(15,2)/(B(15,2)+0.005);
      goto Lb8;
//C.....
Lb19: BE=1;
      AL=B(15,4)/(B(15,4)+0.005);
      goto Lb8;
//C.....
Lb9: AL=B(15,4)/(B(15,4)+0.005);
      BE=B(15,2)/(B(15,2)+0.005);
Lb8: if(AL+BE>=0.8) goto Lb7;
      AL=1;
      BE=1;
Lb7: AL=AL*B(6,3);
      BE=BE*B(6,2);
      for(I=3; I<=5; I++)
		B(I+7,3)=AL*B(I,2)+BE*B(I,3);
Lb5: if( VOPA(&B(10,3), 3, &B(10,3)))
		 return BAD;
//C.....
      if(IB(47)!=1) goto Lb30;
Lb4: if(fabs(B(10,4))+fabs(B(11,4))+fabs(B(12,4))!=0) goto Lb30;
      if( ASVOPM(&B(10,4),&IU(10),&B(3,3),&B(10,3),Y,Y,Y,Y,Y,Y))
		 return BAD;

//C6.....
Lb30: if(IB(31)==0) goto Lb40;
      B(13,3)=0;
      if(IB(31)==1) goto Lb50;
      if(IB(32)>1) goto Lb31;
      B(13,3)=B(13,4);
      if(D>0)
		  B(13,3)=B(13,3)+D;
      goto Lb40;
Lb31: if( ASVOPM(RS,&IU(23),&B(7,3),&B(7,4),Y,Y,Y,Y,Y,Y))
		 return BAD;
      U=16+RS(2)*RS(2)-RS(1);
      B(13,3)=B(13,4)+6*RS(3)/(sqrt(U)+RS(2));
      if(D<=0) goto Lb40;
      if(IB(17)!=1) goto Lb32;
      U=1;
      AL=-1;
      J=3;
      goto Lb33;
Lb32: if(IB(47)!=1) goto Lb40;
      if(IB(46)==1) goto Lb40;
      U=0;
      AL=1;
      J=4;
Lb33: for(I=1; I<=6; I++){
		  RS(I)=B(I+6,4);
		  RS(I+7)=B(I+6,3);
	  }
      RS(7)=B(13,4);
      RS(14)=B(13,3);
      U=fabs(U-D/(RS(14)-RS(7)));
      if( VTKM(&B(7,J),KU,RS,U))
		 return BAD;
      if( VOPA(&B(10,J),3, &B(10,J)))
		 return BAD;
      B(13,J)=B(13,J)+AL*D;
//C7.....
Lb40: if(IB(46)<=1) goto Lb50;
      if(IPR>0) goto Lb41;
      B(14,4)=B(13,4);
      goto Lb50;
Lb41: if(IB(47)<=1) goto Lb50;
	 if(IB(32)<=1) goto Lb45;
//C8.....
	 S1=(B(13,3)-B(13,4))/(B(14,3)-B(14,4));
      S2=(B(13,4)-B(13,5))/(B(14,4)-B(14,5));
      S=S1*S2*(S1+S2)/(S1*S1+S2*S2);
      goto Lb46;
//C9.....
Lb45: if(IB(47)==2)
		  S=(B(13,4)-B(13,5))/(B(14,4)-B(14,5));
      if(IB(47)>2)
		  S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,5);
Lb46: if( VOPB(&B(10,4),13,&B(10,4),&S))
		 return BAD;
      B(15,4)=S;
//C10....
      if(IB(62)!=1) goto Lb50;
      S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,4);
      if( VOPB(&B(10,5),13,&B(10,5),&S))
		 return BAD;
//C11.....
Lb50: if(IB(61)==0) goto Lb54;
      J=(int)(IB(61)-1)*LR(4);
      if(IPR==0)
		  A(LR(3)+J)=B(14,5)+PRM;
	 for(I=7; I<=9; I++){
		  A(J+LR(1))=B(I,5);
		  A(J+LR(2))=B(3+I,5);
		  J++;
	  }
Lb54: if(IB(61)==N)
	  return OK;
		  I=60;
Lb55: IB(I+15)=IB(I);
      I=I-1;
      if(I>0) goto Lb55;
      goto Lb10;

	  return OK;
}
	
//****************************************************************
//
//****************************************************************
int VTK(double* RES, int KU[], double* AU,double T)
{
//*****BЫЧИCЛEHИE TOЧKИ HA УЧACTKE KPИBOЙ
//*****BEPCИЯ: 10 (25.09.90)
      double FG[4];
      int J1=1;

      int K=KU(1);
      int IPA=1;
      int IR=__IntAbs(KU(4));
      if(IR>100) goto Lb24;
      IPA=0;
Lb24: IR=IR-100*IPA;
      int IK=IR/10;
      int IL=IR-10*IK;
      double P1=AU(K);
      double P2=AU(2*K);
      double DP=fabs(P2-P1);
      if(IL>1 || IL>4){
		  message_error_("HEBEPEH ПPИЗHАK PEЗУЛЬТАТА\nVTK");
		  return BAD;
	  }
      if(IK>1 || IK*6>=K){
		  message_error_("HEBEPEH ПPИЗHАK PEЗУЛЬТАТА\nVTK");
		  return BAD;
	  }
      int L=1;
      int J=1;
      int IRD=IL;
      IR=1;
      int LP=1;
	  int I=1;
Lb11: if( WF(FG,T,L,KU(3)))
	   return BAD;
Lb12: double C=DP*FG(3);
      double D=DP*FG(4);
      if(P2==P1) goto Lb2;
      J1=J+2;
      for(I=J; I<=J1; I++){
		if(AU(I)!=AU(I+K))
			goto Lb16;
	  }
Lb2: C=0;
      D=0;
      if(L!=2) goto Lb16;
      D=T*DP;
      C=DP-D;
Lb16: J1=J+K;
      int J2=J;
      for(I=1; I<=3; I++){
		  RES(I,IR)=AU(J2)*FG(1)+AU(J1)*FG(2)+AU(J2+3)*C+AU(J1+3)*D;
		  J1=J1+1;
		  J2=J2+1;
	  }
      int IRN=2;
	  int JK=6*IK+1;
      if(LP==2) goto Lb22;
      IR=IR+IRD;
      if(KU(4)>0) goto Lb23;
      IRD=1;
Lb23: J=J+6;
      if(J>6*IK+1) goto Lb12;
      JK=6*IK+1;
      IRN=2;
      if(KU(4)>0) goto Lb7;
      JK=7;
Lb7: J=1;
      L=L+1;
      if(L>IL) goto Lb20;
      IR=IRN;
      LP=2;
      goto Lb11;
Lb22: IR=IR+IL;
      J=J+6;
      if(J>JK) goto Lb12;
      IRN++;
      goto Lb7;
Lb20: if(IPA==0) goto Lb25;
      IR=1;
      if(KU(4)>0) goto Lb26;
      IR=IL;
Lb26: K=(IL+(IK-1)*IR)+1;
      RES(3,K)=P2;
      RES(2,K)=P1;
      RES(1,K)=P1+(P2-P1)*T;
Lb25: return OK;
}

//****************************************************************
//
//****************************************************************
int WF(double* FG, double T,int L, int IX)
{
//*****BЫЧИCЛEHИE БAЗИCHЫX ФУHKЦИЙ
//*****BEPCИЯ: 08 (25.09.90)
	double Z=1-T;
	double Z1=Z*Z;
	int IP=1;
     int K=IX+IP+4*L-6;
      if(IX==4) goto Lb18;
      if(T<0) goto Lb21;
      if(Z<0) goto Lb20;
Lb18: switch (L){
	  case 1:
		goto Lb1;
	  case 2:
		goto Lb2;
	  case 3:
		goto Lb3;
	  case 4:
		goto Lb4;
	  }
	  message_error_("HEДOПУCTИMOE ЗHAЧEHИE ПAPAMETPA L\nWF");
	  return BAD;
Lb20: Z=-Z;
      IP=3;
Lb21: if(IX==2) goto Lb23;
      if(IX!=3){
		  message_error_("ЭKCTPAПOЛЯЦИЯ ЗAПPEЩEHA\nWF");
		  return BAD;
	  }
Lb23: FG(1)=0.;
      FG(2)=0.;
      FG(3)=0.;
      FG(4)=0.;
      K=IX+IP+4*L-6;
 //			1,2, 3, 4,5, 6, 7, 8,9,10,11,12,13,14,15,16
//	  GO TO(6,11,9,14,7,12,10,15,8,13, 8,16, 8, 8, 8,8),K
	 switch (K){
	  case 1:
		goto Lb6;
	  case 2:
		goto Lb11;
	  case 3:
		goto Lb9;
	  case 4:
		goto Lb14;
	  case 5:
		goto Lb7;
	  case 6:
		goto Lb12;
	  case 7:
		goto Lb10;
	  case 8:
		goto Lb15;
	  case 9:
		return OK;
	  case 10:
		goto Lb13;
	  case 11:
		return OK;
	  case 12:
		goto Lb16;
	  case 13:
	  case 14:
	  case 15:
	  case 16:
		return OK;
	  }
//C.....ИHTEPПOЛЯЦИЯ
Lb1: FG(1)=Z1*(2*T+1);
      FG(2)=1-FG(1);
      FG(3)=Z1*T;
      FG(4)=T-FG(3)-FG(2);
      return OK;
Lb2: FG(2)=6*T*Z;
      FG(1)=-FG(2);
      FG(3)=Z*(1-3*T);
      FG(4)=1-FG(3)-FG(2);
      return OK;
Lb3: FG(2)=6*(1-2*T);
      FG(1)=-FG(2);
      FG(4)=6*T-2;
      FG(3)=FG(1)-FG(4);
      return OK;
Lb4: FG(1)=12.;
      FG(2)=-12.;
      FG(3)=6.;
      FG(4)=6.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KACATEЛЬHOЙ (T<0)
Lb6: FG(1)=1.;
      FG(3)=T;
      return OK;
Lb7: FG(3)=1.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KACATEЛЬHOЙ (T>1)
Lb9: FG(2)=1.;
      FG(4)=Z;
      return OK;
Lb10: FG(4)=1.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KBAДP. ПAPAБOЛE (T<0)
Lb11: Z1=T*T;
      FG(2)=Z1;
      FG(1)=1-FG(2);
      FG(4)=0;
      FG(3)=T-Z1;
      return OK;
Lb12: FG(2)=2*T;
      FG(1)=-FG(2);
      FG(4)=0;
      FG(3)=1-FG(2);
      return OK;
Lb13: FG(2)=2;
      FG(1)=-2;
      FG(3)=-2;
      FG(4)=0;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KBAДP. ПAPAБOЛE (T>1)
Lb14: Z1=Z*Z;
      FG(1)=Z1;
      FG(2)=1-FG(1);
      FG(3)=0;
      FG(4)=Z+Z1;
      return OK;
Lb15: FG(1)=2*Z;
      FG(2)=-FG(1);
      FG(3)=0;
      FG(4)=1+FG(1);
      return OK;
Lb16: FG(1)=2;
      FG(2)=-2;
      FG(3)=0;
      FG(4)=2;
	return OK;
}

//****************************************************************
//*****BЫЧИCЛEHИE TOЧEK KPИBOЙ, РАСПОЛОЖЕННОЙ В ОПЕРАТИВНОЙ ПАМЯТИ
//*****BEPCИЯ: 00 (25.01.97)
//
//****************************************************************
int INKM(double RES[39], int ISK[7], double T[3], double* A)
{
	//RES-Массив результата
	//ISK-управляющие параметры кривой={1,12,7,2,1,30,0}
	//ISK[0]-признак Экстраполяции
	//ISK[3]-число узлов кривой
	//T[0]-значение параметра искомой точки
	//T[1]-T[2] интервал кривой (T[1]=1, T[2]=число узлов)
	//A-массив коэффициентов сплайна
	int KU[4];
	int LR[4]={1,4,7,7};

	int L=ISK(4);
      if(L<2){
		  message_error_("ЧИСЛО ТОЧЕК КРИВОЙ <2");
		  return BAD;
	  }

//C.....РАСЧЕТ КОЭФФИЦИЕНТОВ СПЛАЙНА если надо
      if(ISK(7)==1)
		  if( STAFMM(0,L,LR,-1.,A))
			  return BAD;
      ISK(7)=0;
//C.....УCTAHOBKA B HAЧAЛЬHOE COCTOЯHИE T(2),T(3)
     if(T(2)!=0 || T(3)!=0) goto Lb2;
      T(2)=1;
      T(3)=L;
//C.....OПPEДEЛEHИE HOMEPA УЧАСТКА И PACЧET OTHOC. ПAPAMETPОВ
Lb2: if((T(1)-T(2))*(T(3)-T(1))<0 && ISK(1)<2){
		  message_error_("ЭKCTPAПOЛЯЦИЯ ЗAПPEЩEHA\nINKM");
		  return BAD;
	  }
      int IJ=(int)T(1);
      if(IJ<1)
		  IJ=1;
      if(IJ>=L)
		  IJ=L-1;
      double TS=T(1)-IJ;
//C.....ВЫЧИСЛЕНИЕ ТОЧКИ
      KU(1)=ISK(3);
      KU(2)=0;
      KU(3)=ISK(1);
      KU(4)=ISK(2);
     return VTKM(RES, KU, &A((IJ-1)*ISK(3)+1),TS);
}


//****************************************************************
//
//*****BЫЧИCЛEHИE TOЧKИ HA УЧACTKE KPИBOЙ
//*****BEPCИЯ: 00 (25.01.97)
//****************************************************************
int VTKM(double RES[39], int KU[4],double* AU, double T)
{
      double FG[4];
      int K=KU(1);
      int IPA=1;
      int IR=__IntAbs(KU(4));
      if(IR>100) goto Lb1;
      IPA=0;
Lb1: IR=IR-100*IPA;
      int IK=IR/10;
      int IL=IR-10*IK;
      double P1=AU(K);
      double P2=AU(2*K);
      double DP=fabs(P2-P1);
      if(IL<1 || IL>4){
		  message_error_("1-я ОШИБКА В ПРИЗНАКЕ РЕЗУЛЬТАТА\nVTKM");
		  return BAD;
	  }
      if(IK<1 || IK*6>=K){
		  message_error_("2-я ОШИБКА В ПРИЗНАКЕ РЕЗУЛЬТАТА\nVTKM");
		  return BAD;
	  }
      int L=1;
      int J=1;
      int IRD=IL;
      IR=1;
      int LP=1;
	  int I=1;
      int J1=1;
      int J2=1;
	  int JK=1;
	  int IRN=2;
Lb2: if( WFM(FG,T,L,KU(3)))
		 return BAD;
Lb3: double C=DP*FG(3);
     double D=DP*FG(4);
      if(P2==P1) goto Lb5;
      J1=J+2;
	for(I=J; I<=J1; I++)
      if(AU(I)!=AU(I+K)) goto Lb6;

Lb5: C=0;
      D=0;
      if(L!=2) goto Lb6;
      D=T*DP;
      C=DP-D;
Lb6: J1=J+K;
      J2=J;
	for(I=1; I<=3; I++, J1++, J2++)
		RES(I,IR)=AU(J2)*FG(1)+AU(J1)*FG(2)+AU(J2+3)*C+AU(J1+3)*D;

      if(LP==2) goto Lb10;
      IR=IR+IRD;
      if(KU(4)<0) goto Lb8;
      IRD=1;
Lb8: J=J+6;
      if(J<6*IK+1) goto Lb3;
      JK=6*IK+1;
      IRN=2;
      if(KU(4)<0) goto Lb9;
      JK=7;
Lb9: J=1;
      L=L+1;
      if(L>IL) goto Lb11;
      IR=IRN;
      LP=2;
      goto Lb2;
Lb10: IR=IR+IL;
      J=J+6;
      if(J<JK) goto Lb3;
      IRN=IRN+1;
      goto Lb9;
Lb11: if(IPA==0) goto Lb99;
      IR=1;
      if(KU(4)>0) goto Lb12;
      IR=IL;
Lb12: K=(IL+(IK-1)*IR)+1;
      RES(3,K)=P2;
      RES(2,K)=P1;
      RES(1,K)=P1+(P2-P1)*T;
Lb99: return OK;

}


//C****************************************************************
//C
//C****************************************************************
int WFM(double* FG, double T,int L, int IX)
{
//C*****BЫЧИCЛEHИE БAЗИCHЫX ФУHKЦИЙ
//C*****BEPCИЯ: 00 (25.01.97)
      double Z=1-T;
	  double Z1=Z*Z;
	  int IP=1;
     int K=IX+IP+4*L-6;
      if(IX==4) goto Lb18;
      if(T<0) goto Lb17;
      if(Z<0) goto Lb20;
Lb18: Z1=Z*Z;
	 switch (L){
	  case 1:
		goto Lb1;
	  case 2:
		goto Lb2;
	  case 3:
		goto Lb3;
	  case 4:
		goto Lb4;
	  }
	  message_error_("ПРИЗНАК ПРОИВОДНОЙ НЕ 1-4\nWFM");
	  return BAD;
Lb17: IP=1;
      goto Lb21;
Lb20: Z=-Z;
      IP=3;
Lb21: if(IX==2) goto Lb23;
      if(IX!=3){
		  message_error_("ЭKCTPAПOЛЯЦИЯ ЗAПPEЩEHA\nWF");
		  return BAD;
	  }
Lb23: FG(1)=0.;
      FG(2)=0.;
      FG(3)=0.;
      FG(4)=0.;
      K=IX+IP+4*L-6;
 //			  1,2, 3, 4,5, 6, 7, 8, 9,10,11,12,13,14,15,16
//      GO TO(6,11,9,14,7,12,10,15,99,13,99,16,99,99,99,99),K
	 switch (K){
	  case 1:
		goto Lb6;
	  case 2:
		goto Lb11;
	  case 3:
		goto Lb9;
	  case 4:
		goto Lb14;
	  case 5:
		goto Lb7;
	  case 6:
		goto Lb12;
	  case 7:
		goto Lb10;
	  case 8:
		goto Lb15;
	  case 9:
		return OK;
	  case 10:
		goto Lb13;
	  case 11:
		return OK;
	  case 12:
		goto Lb16;
	  case 13:
	  case 14:
	  case 15:
	  case 16:
		return OK;
	  }

//C.....ИHTEPПOЛЯЦИЯ
Lb1: FG(1)=Z1*(2*T+1);
      FG(2)=1-FG(1);
      FG(3)=Z1*T;
      FG(4)=T-FG(3)-FG(2);
      return OK;
Lb2: FG(2)=6*T*Z;
      FG(1)=-FG(2);
      FG(3)=Z*(1-3*T);
      FG(4)=1-FG(3)-FG(2);
      return OK;
Lb3: FG(2)=6*(1-2*T);
      FG(1)=-FG(2);
      FG(4)=6*T-2;
      FG(3)=FG(1)-FG(4);
      return OK;
Lb4: FG(1)=12.;
      FG(2)=-12.;
      FG(3)=6.;
      FG(4)=6.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KACATEЛЬHOЙ (T<0)
Lb6: FG(1)=1.;
      FG(3)=T;
      return OK;
Lb7: FG(3)=1.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KACATEЛЬHOЙ (T>1)
Lb9: FG(2)=1.;
      FG(4)=Z;
      return OK;
Lb10: FG(4)=1.;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KBAДP. ПAPAБOЛE (T<0)
Lb11: Z1=T*T;
      FG(2)=Z1;
      FG(1)=1-FG(2);
      FG(4)=0;
      FG(3)=T-Z1;
      return OK;
Lb12: FG(2)=2*T;
      FG(1)=-FG(2);
      FG(4)=0;
      FG(3)=1-FG(2);
      return OK;
Lb13: FG(2)=2;
      FG(1)=-2;
      FG(3)=-2;
      FG(4)=0;
      return OK;
//C.....ЭKCTPAПOЛЯЦИЯ ПO KBAДP. ПAPAБOЛE (T>1)
Lb14: Z1=Z*Z;
      FG(1)=Z1;
      FG(2)=1-FG(1);
      FG(3)=0;
      FG(4)=Z+Z1;
      return OK;
Lb15: FG(1)=2*Z;
      FG(2)=-FG(1);
      FG(3)=0;
      FG(4)=1+FG(1);
      return OK;
Lb16: FG(1)=2;
      FG(2)=-2;
      FG(3)=0;
      FG(4)=2;
	return OK;
}

//
//****************************************************************
//
//****************************************************************
int STAFMM(int IPR, int N, int LR[4], double D, double* A )
{
//*****РАСЧЕТ КОЭФФИЦ. СПЛАЙНОВОЙ КРИВОЙ
//*****ВЕРСИЯ: 01 (25.07.97)
	double B[75];
	double* IB=&B[0];
	int KU[4]={7,0,1,12};
	double RS[14];
	double AL=1;
	double BE=1;
	double U;
	double H;
	int J;
	double S1, S2, S;
	int IU[37]={2,1,1,904,405,-216,0,2000,0,
				2,1,1,101,405,800,404,600,1,1304,5,2000,0,
				2,2,2,801,507,1302,406,-113,101,-114,102,-415,406,2000,0};

//.....КОНТРОЛЬ ПАРАМЕТРОВ
  if(IPR<0 || IPR>1){
		message_error_("Признак параметризации не 0-1!");
		return BAD;
	}
	if(N<2){
		message_error_("Число узлов сплайн-кривой <2!");
		return BAD;
	}
	if(N>1024){
		message_error_("Число узлов сплайн-кривой >1024!");
		return BAD;
	}
	if(LR(1)<1 || LR(1)>26){
		message_error_("LR(1)<1 || LR(1)>26!");
		return BAD;
	}
	if(LR(2)<LR(1)+3 || LR(2)>29){
		message_error_("LR(2)<LR(1)+3 || LR(2)>29!");
		return BAD;
	}
	if(LR(3)<LR(2)+3 || LR(3)>32){
		message_error_("LR(3)<LR(2)+3 || LR(3)>32!");
		return BAD;
	}
	if(LR(4)<7 || LR(4)>6000){
		message_error_("LR(4)<7 || LR(4)>6000!");
		return BAD;
	}
	int I=1;
	for(I=1; I<=75; I++)
		IB(I)=0;

      int JT=0;
      int JL=1;
      double PRM=A(LR(3));
//////////.....СЕКЦИЯ 1
Lb10: for(I=1; I<=15; I++)
         IB(I)=0;
      JT++;
      if(JT>N) goto Lb20;
      J=(JT-1)*LR(4);
      if(IPR>0) 
		  B(14,1)=A(LR(3)+J);
      for( I=7; I<=9; I++){
         B(I,1)=A(LR(1)+J);
         B(I+3,1)=A(LR(2)+J);
         J=J+1;
	  }
      IB(1)=JT;
      if(JT==1) goto Lb11;
      if(IPR>0 && B(14,1)<=B(14,2)){
		  	message_error_("IPR>0 && B(14,1)<=B(14,2)!");
			 return BAD;
		 }

	double Y[3];
      if( ASVOPM(&B(3,1),IU, &B(7,1), &B(7,2),Y,Y,Y,Y,Y,Y))
		  return BAD;

      H=B(6,1);
      if(H!=0){
         if(VOPA(&B(3,1),3, &B(3,1)))
			 return BAD;
         if(VOPB(&B(15,2),3, &B(3,1), &B(3,2)))
			 return BAD;
	  }
      else{
		  if(D<=0){
		 	message_error_("D<=0!");
			 return BAD;
		 }

         if(JL<=2 || JT==N){
			 message_error_("JL<=2 || JT==N!");
			 return BAD;
		 }
         JL=1;
	  }
Lb11: IB(2)=JL;
	JL++;
/////.....СЕКЦИЯ 2
Lb20: if(IB(32)<=0) goto Lb40;
      if(fabs(B(10,3))+fabs(B(11,3))+fabs(B(12,3))!=0) goto Lb5;
      if(IB(32)==1) goto Lb30;
      if(IB(32)!=2) goto Lb21;
      if(IB(17)!=3) goto Lb23;
      if(IB(2)!=4) goto Lb25;
      goto Lb28;
Lb21: if(IB(32)!=3) goto Lb22;
      if(IB(17)!=4) goto Lb23;
      if(IB(2)!=5) goto Lb19;
      goto Lb9;
Lb22: if(IB(17)<=1) goto Lb23;
      if(IB(2)<=1) goto Lb19;
      goto Lb9;
//.....по 2-ум точкам в последней
Lb23: if(fabs(B(10,4))+fabs(B(11,4))+fabs(B(12,4))!=0) goto Lb24;
      B(10,3)=B(3,3);
      B(11,3)=B(4,3);
      B(12,3)=B(5,3);
      goto Lb5;
Lb24: if(ASVOPM(&B(10,3), &IU(10), &B(3,3), &B(10,4),Y,Y,Y,Y,Y,Y))
		return BAD;
      goto Lb4;
//.....по 3-ем точкам в средней
Lb25: AL=1;
      BE=1;
      goto Lb8;
//.....по 4-ем точкам во второй
Lb28: AL=1;
      BE=B(15,2)/(B(15,2)+0.005);
      goto Lb8;
//.....по 4-ем точкам в третьей
Lb19: BE=1;
      AL=B(15,4)/(B(15,4)+0.005);
      goto Lb8;
//.....по 5-ти точкам
Lb9: AL=B(15,4)/(B(15,4)+0.005);
      BE=B(15,2)/(B(15,2)+0.005);
Lb8: if(AL+BE>=0.8) goto Lb7;
      AL=1;
      BE=1;
Lb7: AL=AL*B(6,3);
      BE=BE*B(6,2);
      for(I=3; I<=5; I++)
         B(I+7,3)=AL*B(I,2)+BE*B(I,3);

Lb5:	if(VOPA(&B(10,3), 3, &B(10,3)))
			return BAD;
//.....в 1-ой точке
      if(IB(47)!=1) goto Lb30;
Lb4: if(fabs(B(10,4))+fabs(B(11,4))+fabs(B(12,4))!=0) goto Lb30;
      if(ASVOPM(&B(10,4), &IU(10), &B(3,3), &B(10,3),Y,Y,Y,Y,Y,Y))
		  return BAD;
/////.....СЕКЦИЯ 3
Lb30: if(IB(31)==0) goto Lb40;
      B(13,3)=0;
      if(IB(31)==1) goto Lb50;
      if(IB(32)==1)
         B(13,3)=B(13,4)+D;
         goto Lb40;
	if(ASVOPM(RS, &IU(23), &B(7,3), &B(7,4),Y,Y,Y,Y,Y,Y))
		  return BAD;
      U=16+RS(2)*RS(2)-RS(1);
      B(13,3)=B(13,4)+6*RS(3)/(sqrt(U)+RS(2));
      if(D<=0) goto Lb40;
      if(IB(17)==1 || (IB(47)==1 && IB(46)!=1)){
		  for( I=1; I<=7; I++){
            RS(I)=B(I+6,4);
            RS(I+7)=B(I+6,3);
		  }
		  if(IB(47)==1 && IB(46)!=1){
            U=fabs(D/(RS(14)-RS(7)));
            if(VTKM(&B(7,4),KU,RS,U))
				return BAD;
            if(VOPA(&B(10,4),3, &B(10,4)))
				return BAD;
            B(13,4)=B(13,4)+D;
		  }
		  if(IB(17)==1){
            U=fabs(1-D/(RS(14)-RS(7)));
            if( VTKM(&B(7,3),KU,RS,U))
				return BAD;
            if( VOPA(&B(10,3),3,&B(10,3)))
				return BAD;
            B(13,3)=B(13,3)-D;
		  }
	  }
 
//.....СЕКЦИЯ 4
Lb40: if(IB(46)<=1) goto Lb50;
      if(IPR>0) goto Lb41;
      B(14,4)=B(13,4);
      goto Lb50;
Lb41: if(IB(47)<=1) goto Lb50;
	if(IB(32)<=1) goto Lb45;
//.....расчет S' по 3-ем точкам
	S1=(B(13,3)-B(13,4))/(B(14,3)-B(14,4));
       S2=(B(13,4)-B(13,5))/(B(14,4)-B(14,5));
       S=S1*S2*(S1+S2)/(S1*S1+S2*S2);
      goto Lb46;
//.....расчет S' по 2-ум точкам в последней
Lb45: if(IB(47)==2) S=(B(13,4)-B(13,5))/(B(14,4)-B(14,5));
      if(IB(47)>2) S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,5);
Lb46: if( VOPB(&B(10,4),13, &B(10,4), &S ))
		  return BAD;
      B(15,4)=S;
//.....расчет S' по 2-ум точкам в первой
      if(IB(62)!=1) goto Lb50;
      S=2*(B(13,4)-B(13,5))/(B(14,4)-B(14,5))-B(15,4);
      if( VOPB(&B(10,5),13,&B(10,5), &S))
		  return BAD;
//.....СЕКЦИЯ 5
Lb50: if(IB(61)==0) goto Lb54;
      J=(int)(IB(61)-1)*LR(4);
      if(IPR==0) A(LR(3)+J)=B(14,5)+PRM;
      for( I=7; I<+9; I++){
         A(J+LR(1))=B(I,5);
         A(J+LR(2))=B(3+I,5);
         J=J+1;
      }
Lb54: if(IB(61)==N)
		  return OK;
      for( I=60; I>=1; I--)
         IB(I+15)=IB(I);
      goto Lb10;
	  return OK;
}

#define R1(x) R1[x-1]
#define R2(x) R1[x-1]
#define IS1(x) IS1[x-1]
#define IS2(x) IS2[x-1]
#define S(x) S[x-1]
#define kIN(x) kIN[x-1]
#define TS(x) TS[x-1]
#define F(x) F[x-1]

//SUBROUTINE ASCCM(IS1,IS2,T,S,E,A1,A2,*,NNN)
int ASCCM(int IS1[7], int IS2[7],double T[3], double S[3], double E, double* A1, double* A2 )
{
//*****ПEPECEЧEHИE KPИBЫX
//*****BEPCИЯ: 00 (12.11.98)
//IS1(7) - массив управляющих параметров 1-ой кривой;
//IS2(7) - массив управляющих параметров 2-ой кривой;
//T(3)=(T,Tн,Tк) - параметр 1-ой кривой и пределы его изменения;
//S(3)=(S,Sн,Sк) - параметр 2-ой кривой и пределы его изменения; 
//E - абсолютная допустимая погрешность при поиске решения;
//A1 - массив коэффициентов 1-ой кривой;
//A2 - массив коэффициентов 2-ой кривой;
	double R1[9];
	double R2[9];
	double RS[12];
	double F[17];
	double TS[22];

	int kIN[65]={4,1,1,1,1,-420,504,1301,504,900,706,1404,1,1002,1,903,604, 
		-116,1,-117,203,903,704,-118,1,-119,203,2000,0, 
		1,4,901,406,-113,101,-114,105,-115,107,1102,500,1103,700,
		-116,102,-117,103,2000,0,
		1,4,901,406,1000,507,1000,7,-113,1,1500,507,-114,1,2000,0};
	double Y[3]={7777777777, 0,0};
	if(E<0.0001){
		message_error_("ЗАДАНА ПОГРЕШНОСТЬ < 0.0001");
		return BAD;
	}
	int L1=IS1(2);
	int L2=IS2(2);
	double P=100001;
//C.....ПOИCK HAЧAЛЬHOГO ПPИБЛИЖEHИЯ
	IS1(2)=11;
	IS2(2)=11;
	T(1)=T(2);
	if(INKM(R1,IS1,T,A1))
		return BAD;
	if(ASSHG(0,&T(1),T(2),T(3),0.5))
		return BAD;

	if(INKM(&R1(4),IS1,T,A1))
		return BAD;
Lb1: TS(1)=T(1);
	if(ASSHG(0, &T(1),T(2),T(3),0.5))
		return BAD;
	if(INKM(&R1(7),IS1,T,A1))
		return BAD;

	S(1)=S(2);
	if(INKM(R2,IS2,S,A2))
		return BAD;
	if(ASSHG(0, &S(1),S(2),S(3),0.5))
		return BAD;
	if(INKM(&R2(4),IS2,S,A2))
		return BAD;
Lb2: TS(2)=S(1);
	if(ASSHG(0, &S(1),S(2),S(3),0.5))
		return BAD;
	if(INKM(&R2(7),IS2,S,A2))
		return BAD;
	if(ASVOPM(RS,kIN,R1, &R1(7),R2, &R2(7),Y,Y,Y,Y))
		return BAD;
	double D=RS(5)-RS(7);
	if(D==0) goto Lb3;
	TS(5)=RS(5)/D;
	if(TS(5)<-0.000001 || TS(5)>1.000001) goto Lb3;
	TS(6)=(RS(4)+(RS(6)-RS(4))*TS(5))/RS(8);
	if(TS(6)<-0.000001 || TS(6)>1.000001) goto Lb3;
	if(VOPC(&D,9,R1,R2,RS))
		return BAD;
	if(fabs(D)> P) goto Lb3;
	P=fabs(D);
	TS(3)=TS(1);
	TS(4)=TS(2);
Lb3:int I=1;
    for(I=1;I<=6;I++)
		R2(I)=R2(I+3);
	if(S(1)!=S(3)) goto Lb2;
	for(I=1;I<=6;I++)
		R1(I)=R1(I+3);
	int K1=0;
	if(T(1)!=T(3)) goto Lb1;
	if(P>100000) goto Lb102;
//C.....PACЧET TOЧKИ ПEPECEЧEHИЯ
	T(1)=TS(3);
	S(1)=TS(4);
	F(10)=0;
	F(11)=10;
	F(14)=6;
	F(17)=0.15;
	TS(13)=1;
	TS(14)=0.01;
	IS1(2)=12;
	IS2(2)=12;
	for(I=1;I<=3;I++){
		TS(2*I-1)=T(I);
		TS(2*I)=S(I);
	}
Lb4: if(INKM(RS,IS1,T,A1))
		return BAD;
	if(INKM(&RS(7),IS2,S,A2))
		return BAD;
	if(ASVOPM(R1, &kIN(30),RS,Y,Y,Y,Y,Y,Y,Y))
		return BAD;
	if(R1(1)>E*E){
		D=sqrt(R1(1))*E;
		if(fabs(R1(4))>D || fabs(R1(5))>D){
			F(1)=R1(1);
			F(2)=2*R1(2);
			F(3)=-2*R1(3);
			if(K1==0)
				if(ASVOPM(&F(6), &kIN(50),RS,Y,Y,Y,Y,Y,Y,Y))
					return BAD;
			if(ASM6WM(F,TS, &K1,2))
				return BAD;
			T(1)=TS(1);
			S(1)=TS(2);
			goto Lb4;
		}
	}
	IS1(2)=L1;
	IS2(2)=L2;
	return OK;
//C.....ОБРАБОТКА АВАРИЙНЫХ СИТУАЦИЙ
Lb102: IS1(2)=L1;
	IS2(2)=L2;
	message_error_("КРИВЫЕ НЕ ПЕРЕСЕКАЮТСЯ");

	return BAD;
}



