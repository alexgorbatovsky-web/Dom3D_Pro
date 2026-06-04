////////////	реализация функций Астры на C
/////////	начало 18.10.2000
/////////////////////////////////////////////////////////
#include "defines.h"
#include "AstraVect.h"

extern BOOL ASSHG(int IPR, double* T, double TN, double TK, double DT);
extern BOOL VTKB(double* RES,int IPR, int IU, int IEX, int N, int ITBS, int K,\
		  int IP, double S, double* A);

BOOL VTPB(double* RES,int IPR, int IU,int IV,int IUV,int IEU,int IEV,\
		  int LU, int K, int N, int M, double UV[], double* A);



//****************************************************************
//
//****************************************************************

#define IS(x) IS[x-1]
#define B(x,y) B[x-1+(y-1)*14]
#define A(x,y,z) A[x-1+6*(y-1)+6*K*(z-1)]
#define RES(x,y) RES[x-1+(y-1)*3]
#define UV(x) UV[x-1]
#define IT(x) IT[x-1]

BOOL IG128K(int* KB,int* LB, double B[],int N,int M, int K, int L,\
			double A[], int IT[], int IS[], double E)
{
//*****КОНВЕРСИЯ NURBS-ПОВЕРХНОСТИ В ФОРМАТ КПС С ЗАДАННОЙ ТОЧНОСТЬЮ
//*****BEPCИЯ: 00 (22.07.99)
//      DIMENSION B(14,KB),A(6,K,L),,
	double U,V;
	double RES[18];
	double UV[2];
//....контроль данных
	if(*KB < 4){
		message_error_("РАЗМЕР  ПОЛЯ РЕЗУЛЬТАТА < 4!");
		return BAD;
	}
	if(N < 1 || N > 50){
		message_error_("СТЕПЕНЬ NURBS N НЕ 1-50!");
		return BAD;
	}
	if(M < 1 || M > 50){
		message_error_("СТЕПЕНЬ NURBS M НЕ 1-50!");
		return BAD;
	}
	if(K < 2*N+2 || L < 2*M+2){
		message_error_("K/L НЕ СООТВЕТСТВУЕТ N/M!");
		return BAD;
	}
	if(E < 0.0001){
		message_error_("ЗАДАНА ТОЧНОСТЬ < 0.0001!");
		return BAD;
	}
	int KN=K-N-1;
	int KM=L-M-1;
	int J=1;
	int I=1;
//....контроль весов
	for(J=1; J<=KM; J++){
		for(I=1;I<=KN; I++){
			if(A(4,I,J) <= 0){
				message_error_("ВЕС НЕ ПОЛОЖИТЕЛЕН!");
				return BAD;
			}
		}
    }
//....создание массива ссылок и контроль кратности узлов по U
      J=1;
      int II=0;
      for(I=N+1;I<=K-N; I++){
         II=II+1;
		 if(A(5,I,1) > A(5,I+1,1)){
			message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
			return BAD;
		}
		 if(A(5,I,1) != A(5,I+1,1) || I==K-N){
			 if(N-II < 1 && II > 1){
				message_error_("НАЛИЧИЕ ИЗЛОМА!");
				return BAD;
			}
			J=J+1;
			IT(J)=I-N;
			II=0;
		 }
      }
      IT(1)=J-1;
      if(IT(1) < 2){
			message_error_("ВЫРОЖДЕНИЕ ПОВ-ТИ!");
			return BAD;
	  }
      int KU=IT(1);
//....создание массива ссылок и контроль кратности узлов по V
      J=1;
      II=0;
      for( I=M+1;I<=L-M; I++){
         II=II+1;
         if(A(6,1,I) > A(6,1,I+1)){
				message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
				return BAD;
			}
         if(A(6,1,I) != A(6,1,I+1) || I==L-M){
			 if(M-II < 1 && II > 1){
					message_error_("НАЛИЧИЕ ИЗЛОМА!");
					return BAD;
				}
            J=J+1;
            IS(J)=I-M;
            II=0;
         }
      }
      IS(1)=J-1;
      if(IS(1) < 2){
			message_error_("ВЫРОЖДЕНИЕ ПОВ-ТИ!");
			return BAD;
		}
      int KV=IS(1);
//....разбиение сегментов
	for( J=2;J<=KV; J++){
		double Q=0;
		int LS=IS(J);
		UV(2)=0;
		for( I=2;I<=KU; I++){
			 int LT=IT(I)-IT(I)/1000*1000;
		//.......поиск max(R''''(u) на очередном сегменте
Lb1:		 double F=0;
			 U=0;
			 for(int ii=1; ii<=11; ii++){
				UV(1)=U;
				if( VTPB(RES,0,5,1,0,1,1,K,6,N+1,M+1,UV,&A(1,LT,LS)))
					return BAD;
				double P=0;
				if( VOPA(&P,2, &RES(1,5)))
					return BAD;
				if(P > F)
					F=P;
				if( ASSHG(0,&U,0.0,1.0,0.1))
					return BAD;
			 }
			 if(J==KV && UV(2)==0){
				UV(2)=1;
				goto Lb1;
			 }
		//.......подсчет числа разбиений очередного сегмента по U
			 double DT=sqrt(F/(E*192));
			 DT=sqrt(DT);
			 II=(int)DT;
			 if(II==0 || DT-II > 0.1)
				 II=II+1;
			 II=II*1000;
			 if(II > IT(I))
				 IT(I)=II+LT;
		//.......поиск max(R''''(v) на очередном сегменте
			 UV(1)=0;
Lb2:		U=0;
			 for(int ii=1; ii<=11; ii++){
				UV(2)=U;
				if( VTPB(RES,0,1,5,0,1,1,K,6,N+1,M+1,UV, &A(1,LT,LS)))
					return BAD;
				double P=0;
				if( VOPA(&P,2, &RES(1,5)))
					return BAD;
				if(P > Q)
					Q=P;
				if( ASSHG(0,&U,0.0,1.0,0.1))
					return BAD;
			 }
			 if(I==KU && UV(1)==0){
				UV(1)=1;
				goto Lb2;
			 }
		}
//.......подсчет числа разбиений очередного сегмента по V
		double DT=sqrt(Q/(E*192));
		DT=sqrt(DT);
		II=(int)DT;
		if(II==0 || DT-II > 0.1)
			II=II+1;
		II=II*1000;
		if(II > IS(J))
			IS(J)=II+LS;
	}
//....расчет размерностей результата
      int LT=1;
      for( I=2;I<=KU;I++)
         LT=LT+IT(I)/1000;
      int LS=1;
      for( I=2;I<=KV; I++)
         LS=LS+IS(I)/1000;
      if(*KB < LT*LS){
		message_error_("ПОЛЕ РЕЗУЛЬТАТА МАЛО!");
		return BAD;
	  }
      *KB=LT;
      *LB=LS;
//....расчет точек и занесение в массив результата
      int JRES=0;
      for( J=2;J<=KV; J++){
         KM=IS(J)/1000;
         LS=IS(J)-KM*1000;
         double DS=1./KM+0.001;
         if(DS > 1)
			 DS=1;
         if(J==KV)
			 KM=KM+1;
         V=0;
         for(int JJ=1; JJ<=KM; JJ++){
			 for( I=2; I<=KU; I++){
               KN=IT(I)/1000;
               LT=IT(I)-KN*1000;
               double DT=1./KN+0.001;
               if(DT > 1)
				   DT=1;
               if(I==KU)
				   KN=KN+1;
//.............расчет значений
               U=0;
               for(int ii=1; ii<=KN; ii++){
                  UV(1)=U;
                  UV(2)=V;
                  if( VTPB(RES,1,2,2,1,1,1,K,6,N+1,M+1,UV, &A(1,LT,LS)))
					  return BAD;
                  double Q=RES(3,5)-RES(2,5);
                  if(Q <= 0){
					message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
					return BAD;
				  }
                  double P=RES(3,6)-RES(2,6);
                  if(P <= 0){
					message_error_("ПАРАМЕТР НЕ ВОЗРАСТАЕТ!");
					return BAD;
				  }
                  JRES=JRES+1;
                  for(int KK=1;KK<=3; KK++){
                     B(KK,JRES)=RES(KK,1);
                     B(KK+3,JRES)=RES(KK,2)/Q;
                     B(KK+6,JRES)=RES(KK,3)/P;
                     B(KK+9,JRES)=RES(KK,4)/(Q*P);
                  }
                  B(13,JRES)=RES(1,5);
                  B(14,JRES)=RES(1,6);
                  if( ASSHG(0,&U,0.0,1.0,DT))
					  return BAD;
               }
			 }
            if( ASSHG(0,&V,0.0,1.0,DS))
				return BAD;
         }
      }
	return OK;
}

#undef A
#undef B

//#define IS(x) IS[x-1]
#define B(x,y) B[x-1+(y-1)*5]
#define A(x,y) A[x-1+(y-1)*K]
#define Q(x,y) Q[x-1+(y-1)*4]
#define H(x,y) H[x-1+(y-1)*4]


//****************************************************************
//
//****************************************************************
BOOL VTPB(double* RES,int IPR, int IU,int IV,int IUV,int IEU,int IEV,\
		  int LU, int K, int N, int M, double UV[], double* A)
{
//*****РАСЧЕТ ЗНАЧЕНИЙ NURBS-ПОВЕРХНОСТИ НА ЗАДАННОЙ КЛЕТКЕ
//*****BEPCИЯ: 02 (23.06.99)
//      DIMENSION RES(3,12),A(K,*),UV(2),Q(4,14),B(5,101),H(4,2)
	double B[505];
	double H[8];
	double Q[56];
//....контроль данных
	if(IU < 1 || IU > 5){
		message_error_("ЧИСЛО ПРОИЗВОДНЫХ НЕ 1-5!");
		return BAD;
	}
	if(IV < 1 || IV > 5){
		message_error_("ЧИСЛО ПРОИЗВОДНЫХ НЕ 1-5!");
		return BAD;
	}
	if(IUV < 0 || IUV > 1){
		message_error_("!ПРИЗНАК СМЕШ. ПРОИЗВОДНОЙ НЕ 0-1");
		return BAD;
	}
	if(IUV==1 && (IU==1 || IV==1)){
		message_error_("DU=1 ИЛИ DV=1 ПРИ DUV=1!");
		return BAD;
	}
	if(N < 2 || N > 51){
		message_error_("ПОРЯДОК СПЛАЙНА НЕ 2-51!");
		return BAD;
	}
	if(M < 2 || M > 51){
		message_error_("ПОРЯДОК СПЛАЙНА НЕ 2-51!");
		return BAD;
	}
	if(K < 5 || K > 6){
		message_error_("РАЗМЕРНОСТЬ ДАННЫХ K НЕ 5-6!");
		return BAD;
	}
	int JN=2*N-1;
	if(LU < JN){
		message_error_("LU < 2*N-1!");
		return BAD;
	}
	int JM=2*M-1;
	int J=1;
//....расчет производных по U ---------------------------------------
	for(J=1; J<=JN; J++){
         if(J <= N)
            if( VTKB(&B(1,J),0,1,IEV,M,K-5,LU*K,K,UV(2), &A(1,J)))
				return BAD;
         B(5,J)=A(K-1,J);
	}
	if( VTKB(Q,0,IU,IEU,N,-K+5,5,5,UV(1),B))
		return BAD;
//....возврат результата
      double W=Q(4,1);
      if(W <= 0 && K==6){
		message_error_("ЗНАЧЕНИЕ ВЕСА НЕ > 0!");
		return BAD;
	  }
      for(J=1; J<=IU; J++){
		  for(int I=1; I<=3; I++){
//....случай рационального  В-сплайна
			  if(K==6) 
				  switch (J){
					case 1:
						RES(I,J)=Q(I,1)/W;
						break;
					case 2:
						RES(I,J)=(Q(I,2)-RES(I,1)*Q(4,2))/W;
 						break;
					case 3:
					   RES(I,J)=(Q(I,3)-RES(I,1)*Q(4,3)-2*RES(I,2)*Q(4,2))/W;
					   break;
					case 4:
						RES(I,J)=(Q(I,4)-RES(I,1)*Q(4,4)-3*(RES(I,2)*Q(4,3)+RES(I,3)*Q(4,2)))/W;
						break;
					case 5:
						RES(I,J)=(Q(I,5)-RES(I,1)*Q(4,5)-4*RES(I,2)*Q(4,4)-6*RES(I,3)*Q(4,3)-4*RES(I,4)*Q(4,2))/W;
						break;
			  }
///////....случай нерационального  В-сплайна
			  else
				 RES(I,J)=Q(I,J);
		  }
	  }
//....расчет производных по V  --------------------------------------
	if(IV > 1){
		for(J=1; J<=JM; J++){
			if(J <= M)
				if( VTKB(&B(1,J),0,1,IEU,N,K-5,K,K-1,UV(1), &A(1,(J-1)*LU+1)))
					return BAD;
			B(5,J)=A(K,(J-1)*LU+1);
		}
		if( VTKB(&Q(1,6),0,IV,IEV,M,-K+5,5,5,UV(2),B))
			return BAD;
//....возврат результата
		for( J=1;J<=IV-1; J++){
			  int L=IU+J;
			  for(int I=1;I<=3; I++){
	//////....случай рационального  В-сплайна
				  if(K==6)
					  switch (J){
						case 1:
							RES(I,L)=(Q(I,7)-RES(I,1)*Q(4,7))/W;
							break;
						case 2:
							RES(I,L)=(Q(I,8)-RES(I,1)*Q(4,8)-2*RES(I,L-1)*Q(4,7))/W;
							break;
						case 3:
							RES(I,L)=(Q(I,9)-RES(I,1)*Q(4,9)-3*(RES(I,L-1)*Q(4,8)+RES(I,L-2)*Q(4,7)))/W;
							break;
						case 4:
							RES(I,L)=(Q(I,10)-RES(I,1)*Q(4,10)-4*RES(I,L-1)*Q(4,9)-6*RES(I,L-2)*Q(4,8)+4*RES(I,L-3)*Q(4,7))/W;
							break;
					  }
	/////....случай нерационального  В-сплайна
				  else
					 RES(I,L)=Q(I,J+6);
			  }
		}
	}
//....расчет смешанной производной DUV ------------------------------
//char buf[80];
//Step("расчет смешанной производной DUV");

	if(IUV==1){
		for( J=1; J<=JM; J++){
			 if( VTKB(H,0,2,IEU,N,K-5,K,K-1,UV(1), &A(1,(J-1)*LU+1)))
				 return BAD;
			 for(int I=1; I<=4; I++)
				B(I,J)=H(I,2);
			 B(5,J)=A(K,(J-1)*LU+1);
		}
		if( VTKB(&Q(1,11),0,2,IEV,M,-K+5,5,5,UV(2),B))
			return BAD;
//....возврат результата
		int L=IU+IV;
		for(int I=1; I<=3; I++){
//....случай рационального  В-сплайна
			if(K==6)
				RES(I,L)=(Q(I,12)-RES(I,1)*Q(4,12)-RES(I,2)*Q(4,7)-RES(I,IU+1)*Q(4,2))/W;
			//....случай нерационального  В-сплайна
			else
				RES(I,L)=Q(I,12);

		
		}
//sprintf(buf,"x,y,z=%12.9f %12.9f %12.9f",RES(1,L),RES(2,L),RES(3,L));
//Step(buf);
	}
//....возвращение значений внутренних параметров
	if(IPR==1){
		int I=IU+IV+IUV;
		RES(1,I)=A(K-1,N)*(1-UV(1))+A(K-1,N+1)*UV(1);
		RES(2,I)=A(K-1,N);
		RES(3,I)=A(K-1,N+1);
		I=I+1;
		J=LU*(M-1)+1;
		RES(1,I)=A(K,J)*(1-UV(2))+A(K,J+LU)*UV(2);
		RES(2,I)=A(K,J);
		RES(3,I)=A(K,J+LU);
	}
	return OK;
}

#undef A
#undef B

#define A(x,y,z) A[x-1+14*(y-1)+14*(*N)*(z-1)]
#define B(x,y,z) B[x-1+3*(y-1)+3*(2*(*N))*(z-1)]
#define S(x) S[x-1]
#define T(x) T[x-1]
#define W(x,y) W[x-1+(y-1)*2*(*N)]

//****************************************************************
//
//****************************************************************
BOOL IG128(int* N, int* M, double* A, double* T, double* S, double* W, double* B)
{
//*****ПРЕДСТАВЛЕНИЕ ПОВ-ТИ В ФОРМЕ NURBSа
//*****BEPCИЯ: 00 (03.03.97)
//      DIMENSION A(14,N,M),T(2*N+4),S(2*M+4),W(2*N,2*M),B(3,2*N,2*M)
	int I=0;
//.....ЗАНЕСЕНИЕ ЗНАЧЕНИЙ ПАРАМЕТРА T
	for(I=1; I<=2*(*N)+4; I++){
		int II=(I-1)/2;
		if(II < 1)
			II=1;
		if(II > (*N))
			II=(*N);
		T(I)=A(13,II,1);
	}
//....ЗАНЕСЕНИЕ ЗНАЧЕНИЙ ПАРАМЕТРА S
	for( I=1; I<=2*(*M)+4; I++){
		int II=(I-1)/2;
		if(II < 1)
			II=1;
		if(II > (*M))
			II=(*M);
		S(I)=A(14,1,II);
	}
	int J=0;
//....ЗАНЕСЕНИЕ ЗНАЧЕНИЙ ВЕСОВ
      for( J=1; J<=2*(*M); J++)
		for( I=1; I<=2*(*N); I++)
			W(I,J)=1;
//....ВЫВОД КОЭФФИЦИЕНТОВ
      int KM=1;
      int KL=1;
	for(int JM=1; JM<=2*(*M); JM++){
		J=(JM+1)/2;
		int K=J+2*KM-3;
		if(K < 1)
			K=1;
		if(K > (*M))
			K=(*M);
		double DS=A(14,1,K)-A(14,1,J);
		KM=3-KM;
		for(int IL=1; IL<=2*(*N); IL++){
			I=(IL+1)/2;
			K=I+2*KL-3;
			if(K < 1)
				K=1;
			if(K > (*N))
				K=(*N);
			double DT=A(13,K,1)-A(13,I,1);
			KL=3-KL;
			for(K=1; K<=3; K++)
				B(K,IL,JM)=A(K,I,J)+A(K+3,I,J)*DT/3+(A(K+6,I,J)+A(K+9,I,J)*DT/3)*DS/3;
		}
	  }
	return OK;

}

