////////////	реализация функций Астры на C
/////////	начало 18.10.2000
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

extern BOOL ASSHG(int IPR, double* T, double TN, double TK, double DT);


#define RES(x,y) RES[x-1+(y-1)*4]
#define H(x,y) H[x-1+(y-1)*4]
#define B(x,y) B[x-1+(y-1)*4]
#define A(x,y) A[x-1+(y-1)*K]
 
//      CALL VTKB(RES,     0,      5,        1,     N+1,    1,      5,      5,        U,     A(1,IS))
BOOL VTKB(double* RES,int IPR, int IU, int IEX, int N, int ITBS, int K,\
		  int IP, double S, double* A)
{
//C*****РАСЧЕТ ЗНАЧЕНИЙ NURBS-КРИВОЙ НА ЗАДАННОМ УЧАСТКЕ
//C*****BEPCИЯ: 01 (27.01.99)
//	DIMENSION RES(4,6),A(K,*),
	double B[404];//B(4,101)
	double H[20];//H(4,5)
//.....контроль данных
	if(IU < 1 || IU > 5){
		message_error_("ЧИСЛО ПРОИЗВОДНЫХ НЕ 1-5!");
		return BAD;
	}
	if(IEX < 1 || IEX > 4){
		message_error_("ПРИЗНАК ЭКСТРАПОЛЯЦИИ НЕ 1-4!");
		return BAD;
	}
	if(N < 2 || N > 51){
		message_error_("ПОРЯДОК СПЛАЙНА НЕ 2-51!");
		return BAD;
	}
	if(ITBS < -1 || ITBS > 1){
		message_error_("ПРИЗНАК ТИПА В-СПЛАЙНА НЕ -1,0,1!");
		return BAD;
	}
	if(K < 4){
		message_error_("РАЗМЕРНОСТЬ ДАННЫХ K < 4!");
		return BAD;
	}
	int KPR=__IntAbs(ITBS)+3;
	if(IP < KPR+1 || IP > K){
		message_error_("ОШИБКА В ИНДЕСЕ ПАРАМЕТРА!");
		return BAD;
	}
//.....контроль экстраполяции
	double T=S;
	int ID=IU;
	if(S < 0 || S > 1){
		if(IEX==1){
			message_error_("ЭКСТРАПОЛЯЦИЯ ЗАПРЕЩЕНА!");
			return BAD;
		}
	   if(S < 0)
		   T=0;
	   if(S > 0)
		   T=1;
	   ID=IEX;
	}
//....подсчет внутреннего параметра
	double DT=A(IP,N+1)-A(IP,N);
	if(DT <= 0){
		message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
		return BAD;
	}
	double P=A(IP,N)*(1-T)+A(IP,N+1)*T;
	int JJ=0;
//....расчет точек и производных В-сплайна
	for(JJ=1; JJ<=5; JJ++){
		for(int M=1; M<=4; M++)
			H(M,JJ)=0;
	}
	double Q=1;
	if(ID > N) 
		ID=N;
	double V,W;
	for(JJ=1; JJ<=ID; JJ++){
         if(JJ > 1)  
			 Q=Q*(N-JJ+1)*DT;
		 for(int J=2; J<=N; J++){
			for(int I=J; I<=N; I++){
			int L=I-J+1;
			double D=A(IP,L+N)-A(IP,I);
			if(D <= 0){
				message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
				return BAD;
			}
			if(J <= JJ){
				V=1/D;
				W=-V;
			}
			else{
				V=(P-A(IP,I))/D;
				W=1-V;
				if(V < 0 || V > 1){
					message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
					return BAD;
				}
			}
			for(int M=1; M<=KPR; M++){
				if(J==2){
				  if(ITBS==1 && M <= 3)
					 B(M,L)=A(M,L)*A(4,L)*W+A(M,L+1)*A(4,L+1)*V;
				  else
					 B(M,L)=A(M,L)*W+A(M,L+1)*V;
				}
			   else
				  B(M,L)=B(M,L)*W+B(M,L+1)*V;
			   if(J==N) 
				   H(M,JJ)=B(M,1)*Q;
			   }
			}
		 }
	}
//....экстраполяция
	if(S < 0 || S > 1){
         T=S-T;
         for(int I=1; I<=KPR; I++){
			 for(int J=1; J<=IU; J++){
				switch (J){
					case 1:
					   RES(I,1)=H(I,1)+T*(H(I,2)+T*(H(I,3)*0.5+H(I,4)*T/6));
					   break;
					case 2:
						RES(I,2)=H(I,2)+T*(H(I,3)+H(I,4)*T*0.5);
					   break;
					case 3:
						RES(I,3)=H(I,3)+H(I,4)*T;
					   break;
					case 4:
						RES(I,4)=H(I,4);
						break;
					case 5:
						RES(I,5)=H(I,5);
						break;
				}
			 }
		 }
	}
	else{
         for(int I=1;I<=KPR; I++)
            for(int J=1; J<=IU; J++)
               RES(I,J)=H(I,J);
	}
//....возвращение внутреннего параметра
	if(IPR==1){
         int I=IU+1;
         RES(1,I)=A(IP,N)*(1-S)+A(IP,N+1)*S;
         RES(2,I)=A(IP,N);
         RES(3,I)=A(IP,N+1);
	}
	return OK;
}

#define B(x,y) B[x-1+(y-1)*7]
#define A(x,y) A[x-1+(y-1)*5]
#define IT(x) IT[x-1]

//****************************************************************
//
//****************************************************************

/*           NB - integer - максимальный размер поля результата. После
C*      исполнения модуля содержит число узлов полученного кубического 
C*      сплайна;
C*           B(7,NB) - поле под массив коэфициентов сплайн-кривой;
C*           N - степень NURBS-кривой (0 < N < 51);
C*           L - размер полной сетки параметра NURBS-кривой;
C*           A(5,L) - массив коэффициентов и значений параметров NURBS-кривой.
C*          E - заданная точность приближения, Е > 0.0001;
C*           IT(L) - рабочий массив; 
*/

BOOL IG126K(int* NB,double* B,int N,int L,double*  A,double E, int* IT)
{
//*****КОНВЕРТАЦИЯ NURBS-КРИВОЙ В КПС С ЗАДАННОЙ ТОЧНОСТЬЮ 
//*****BEPCИЯ: 00 (22.07.99)
      double TS=0;
	  double RES[24];
//....контроль данных
	if(*NB < 2){
		message_error_("РАЗМЕР  ПОЛЯ РЕЗУЛЬТАТА < 2!");
		return BAD;
	}
	if(L < 2*N+2){
		message_error_("L НЕ СООТВЕТСТВУЕТ N!");
		return BAD;
	}
	if(N < 1 || N > 50){
		message_error_("СТЕПЕНЬ NURBSа НЕ 1-50!");
		return BAD;
	}
	if(E < 0.0001){
		message_error_("ЗАДАНА ТОЧНОСТЬ < 0.0001!");
		return BAD;
	}
	int M=L-N-1;
	int I=0;
//....контроль весов
	for(I=1; I<=M; I++)
		if(A(4,I) <= 0){
			message_error_("ВЕС НЕ ПОЛОЖИТЕЛЕН!");
			return BAD;
		}
//....создание массива ссылок и контроль кратности узлов
	int J=0;
	int K=0;
	for( I=N+1; I<=L-N; I++){
		K=K+1;
		if(A(5,I) > A(5,I+1)){
			message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
			return BAD;
		}
		if(A(5,I) != A(5,I+1) || I==L-N){
		   if(N-K < 1 && K > 1){
				message_error_("КРИВАЯ С ИЗЛОМОМ!");
				return BAD;
			}
			J=J+1;
			IT(J)=I-N;
			K=0;
		}
	}
	int KU=J;
	if(KU < 2){
		message_error_("ВЫРОЖДЕНИЕ КРИВОЙ!");
		return BAD;
	}
//....цикл по числу сегментов
	J=0;
	for( I=1; I<=KU-1; I++){
		int IS=IT(I);
		TS=0;
		//.......поиск max(R''''(u) на очередном сегменте
		double Q=0;
		for(int II=1; II<=11; II++){
			double U=TS;
			if( VTKB(RES,0,5,1,N+1,1,5,5,U,&A(1,IS)))
				return BAD;
			double W=RES(4,1);
			if(W <= 0){
				message_error_("ВЕС НЕ ПОЛОЖИТЕЛЕН!");
				return BAD;
			}
			for(int K=1; K<=3; K++){
				RES(K,1)=RES(K,1)/W;
				RES(K,2)=(RES(K,2)-RES(K,1)*RES(4,2))/W;
				RES(K,3)=(RES(K,3)-RES(K,1)*RES(4,3)-2*RES(K,2)*RES(4,2))/W;
				RES(K,4)=(RES(K,4)-RES(K,1)*RES(4,4)-3*(RES(K,2)*RES(4,3)+RES(K,3)*RES(4,2)))/W;
				RES(K,5)=(RES(K,5)-RES(K,1)*RES(4,5)-2*(2*RES(K,2)*RES(4,4)+3*RES(K,3)*RES(4,3)+2*RES(K,4)*RES(4,2)))/W;
			}
			if( VOPA(&W,2,&RES(1,5)))
				return BAD;
			if(W > Q)
				Q=W;
			if( ASSHG(0,&TS,0,1.0,0.1))
				return BAD;
		}
		//.......подсчет шага на очередном сегменте
		double DT=sqrt(Q/(E*384));
		DT=sqrt(DT);
		M=(int)DT;
		if(M==0 || DT-M > 0.1)
			M=M+1;
		DT=1./M+0.001;
		if(DT > 1)
			DT=1;
		if(I==KU-1)
			M=M+1;
		TS=0;
		//.......расчет точек результата
		for(int II=1; II<=M; II++){
			J=J+1;
			if(J > *NB){
				message_error_("ПОЛЕ РЕЗУЛЬТАТА ИСЧЕРПАНО!");
				return BAD;
			}
			double U=TS;
			if( VTKB(RES,1,2,1,N+1,1,5,5,U,&A(1,IS)))
				return BAD;
			double W=RES(4,1);
			if(W <= 0){
				message_error_("ВЫРОЖДЕНИЕ КРИВОЙ!");
				return BAD;
			}
			Q=(RES(3,3)-RES(2,3))*W;
			if(Q <= 0){
				message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
				return BAD;
			}
			for(int KK=1;KK<=3; KK++){
				B(KK,J)=RES(KK,1)/W;
				B(KK+3,J)=(RES(KK,2)-B(KK,J)*RES(4,2))/Q;
			}
			B(7,J)=RES(1,3);
			if( ASSHG(1,&TS,0,1.0,DT))
				return BAD;
		}
	}////////end цикла по числу сегментов
	*NB=J;
	return OK;
}


#define A(x,y) A[x-1+(y-1)*7]
#define B(x,y) B[x-1+(y-1)*3]
#define W(x) W[x-1]
#define T(x) T[x-1]

//****************************************************************

//****************************************************************
BOOL IG126(int* N,double* A,double* T,double* W,double* B)
{
//****ПРЕОБРАЗОВАНИЕ КРИВОЙ В ФОРМУ NURBSа
//****BEPCИЯ: 00 (01.03.97)
//      DIMENSION A(7,N),T(2*N+4),W(2*N),B(3,2*N)
      if(*N < 2 || *N > 5000){
		  message_error_("ЧИСЛО ТОЧЕК КРИВОЙ НЕ 2-5000");
		  return BAD;
	  }
//....ЗАНЕСЕНИЕ ЗНАЧЕНИЙ ПАРАМЕТРА В МАССИВ Т
	int I=0;
	for(I=1;I<=2*(*N)+4;I++){
		int II=(int)(I-1)/2;
		if(II < 1)
			II=1;
		if(II > *N)
			II=*N;
		T(I)=A(7,II);
	  }
//....ЗАНЕСЕНИЕ ЗНАЧЕНИЙ ВЕСОВ В МАССИВ W
	for(I=1; I<=2*(*N);I++)
		W(I)=1;
//....ЗАНЕСЕНИЕ КОЭФФИЦИЕНТОВ В-СПЛАЙНА В МАССИВ B
	int II=1;
	for(I=1; I<=*N;I++)
		for(int J=1; J<=2;J++){
			int K=I+2*J-3;
			if(K < 1)
				K=1;
			if(K > *N)
				K=*N;
			double DT=A(7,K)-A(7,I);
			for(K=1;K<=3;K++)
				B(K,II)=A(K,I)+A(K+3,I)*DT/3;
			II=II+1;
		}
      return OK;
}

