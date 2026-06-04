/****************************************************************************
				ageom.c++
****************************************************************************/

#include "ageom.h"

#include "Triangle.h"
//#include "service.h"


//1 дюйм=25.3995 mm
//1 yard=0.9144 m
//1 kg=2.2046 pounds


double dist_POINT(double p1[],double p2[])
{
	double ax=p2[0]-p1[0];
	double ay=p2[1]-p1[1];
	double az=p2[2]-p1[2];

	return qxyz(ax,ay,az);
}

double dist_POINT_LINE(double p0[],double p1[],double v[])
{

	double q1=v[1]*(p1[2]-p0[2])-v[2]*(p1[1]-p0[1]);
	double q2=v[2]*(p1[0]-p0[0])-v[0]*(p1[2]-p0[2]);
	double q3=v[0]*(p1[1]-p0[1])-v[1]*(p1[0]-p0[0]);
	double q=qxyz(v[0],v[1],v[2]);
	if(q<DDELTA)
		return dist_POINT(p0,p1);
	return qxyz(q1,q2,q3)/q;
}

void line_ortho_line(double a1,double b1,double x,double y,double *a2,double *b2,double *c2)
{

    *a2=b1;
    *b2=-a1;
    *c2=a1*y-b1*x;
}

short cross2_line_2D(double a1,double b1,double c1,double a2,double b2,double c2,double *x,double *y)
{
    if(fabs(a1)<DELTA && fabs(b1)<DELTA && fabs(c1)<DELTA)
		return BAD;
    if(fabs(a2)<DELTA && fabs(b2)<DELTA && fabs(c2)<DELTA)
		return BAD;

    double d=a1*b2-a2*b1;
    if(fabs(d)<DDELTA)
		return BAD;
    *x=(b1*c2-b2*c1)/d;
    *y=(c1*a2-c2*a1)/d;
    return OK_AG;
}

short XY_XY_ABC(double x1,double y1,double x2,double y2,double *a,double *b,double *c)
{
double e;

    *a=y1-y2;
    *b=x2-x1;
    *c=x1*y2-y1*x2;
    e=sqrt(pow(*a,2)+pow(*b,2));
    if(e<DDELTA){
		*a=*b=*c=0;
		return BAD;
		}
    *a=*a/e;
    *b=*b/e;
    *c=*c/e;
    return OK_AG;
}

double get_dist_XY_line2D(double x0,double y0,double x1,double y1,double x2,double y2)
{
double a1,b1,c1,a2,b2,c2;
double xc,yc;
double delta1,delta2;

    if(XY_XY_ABC(x1,y1,x2,y2,&a1,&b1,&c1))
		return dist_XY_XY(x1,y1,x0,y0);
    line_ortho_line(a1,b1,x0,y0,&a2,&b2,&c2);
    delta1=x1*a2+y1*b2+c2;
    delta2=x2*a2+y2*b2+c2;
    if((delta1<0&&delta2<0)||(delta1>0&&delta2>0))
		return MIN(dist_XY_XY(x1,y1,x0,y0),dist_XY_XY(x2,y2,x0,y0));
    cross2_line_2D(a1,b1,c1,a2,b2,c2,&xc,&yc);
    return dist_XY_XY(xc,yc,x0,y0);
}

short POINT_in_RECT(double x,double y,double x1,double y1,double x2,double y2)
{
double X_min=MIN(x1,x2);
double X_max=MAX(x1,x2);
double Y_min=MIN(y1,y2);
double Y_max=MAX(y1,y2);


    if(x>=X_min && x<=X_max &&  y>=Y_min && y<=Y_max)
		return 1;
    return 0;

}

short XY_in_P1P2(double x0,double y0,double x1,double y1,double x2,double y2,double dopusk)
{
double a1,b1,c1,a2,b2,c2;
double delta1,delta2;

    if(XY_XY_ABC(x1,y1,x2,y2,&a1,&b1,&c1)){
		if(dist_XY_XY(x1,y1,x0,y0)<=dopusk)
			return 1;	//In
		else
			return 0;	//Out
		}
    line_ortho_line(a1,b1,x0,y0,&a2,&b2,&c2);
    delta1=x1*a2+y1*b2+c2;
    delta2=x2*a2+y2*b2+c2;

    if((delta1>0&&delta2<0)||(delta1<0&&delta2>0)||fabs(delta1)<dopusk||fabs(delta2)<dopusk)
		return 1;	//IN
    return 0;	//OUT
}


double XYZ_XYZ_LMN(double p1[],double p2[],double n[])
{
double ax,ay,az;
double q;

    ax=p2[0]-p1[0];	
    ay=p2[1]-p1[1];
    az=p2[2]-p1[2];
    q=qxyz(ax,ay,az);
    if(q < DDELTA){
		n[0]=0;
		n[1]=0;
		n[2]=0;
		return q;
		}
    n[0]=ax/q;
    n[1]=ay/q;
    n[2]=az/q;
    return q;
}

short POINTs_SC(CPoint3d* p0,CPoint3d* px,CPoint3d* py,CVector *vx,CVector *vy,CVector *vz)
{

    long double ax=px->x-p0->x;
    long double ay=px->y-p0->y;
    long double az=px->z-p0->z;

    long double bx=py->x-p0->x;
    long double by=py->y-p0->y;
    long double bz=py->z-p0->z;

    long double cx=ay*bz-az*by;
    long double cy=az*bx-ax*bz;
    long double cz=ax*by-ay*bx;

    bx=cy*az-cz*ay;
    by=cz*ax-cx*az;
    bz=cx*ay-cy*ax;
    long double qa=qxyz(ax,ay,az);
    long double qb=qxyz(bx,by,bz);
    long double qc=qxyz(cx,cy,cz);
    if(qa<DDELTA || qb<DDELTA || qc<DDELTA)
            return BAD;

    vx->l=ax/qa;
    vx->m=ay/qa;
    vx->n=az/qa;
    vy->l=bx/qb;
    vy->m=by/qb;
    vy->n=bz/qb;
    vz->l=cx/qc;
    vz->m=cy/qc;
    vz->n=cz/qc;
    return OK_AG;
}

double get_square(CPoint3d* p1,CPoint3d* p2,CPoint3d* p3)
{
double a,b,c;
double s;

    b=p1->y*(p2->z-p3->z)-p2->y*(p1->z-p3->z)+p3->y*(p1->z-p2->z);
    c=p1->z*(p2->x-p3->x)-p2->z*(p1->x-p3->x)+p3->z*(p1->x-p2->x);
    a=p1->x*(p2->y-p3->y)-p2->x*(p1->y-p3->y)+p3->x*(p1->y-p2->y);
    s=(pow(a,2)+pow(b,2)+pow(c,2))/4;
    return sqrt(s);
}

BOOL point_triangle_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3)
{// Это функция имени Шепеля

double S=get_square(p1,p2,p3);
    if(S<DELTA)	//	очень маленький треугольник
		return BAD;
double    s1=get_square(p1,p2,pc);
double    s2=get_square(pc,p2,p3);
double    s3=get_square(p1,pc,p3);

    if(fabs(S-(s1+s2+s3)) < (S*DELTA))
		return OK_AG;  //point IN
    return BAD;
}

BOOL crossing_line_triangle(CPoint3d* p0, CVector* v,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,CPoint3d* pc)
{// Это Наконечный Юра нашел в сети
// находим пересечение луча с треугольником
	CTriangle tr;
	tr.vtx[0]=*p1;
	tr.vtx[1]=*p2;
 	tr.vtx[2]=*p3;
	tr.calcTriangle(tr);
	CPoint3d ray_orig=*p0;
	CVector ray_dir=*v;
	Hit hit;
	bool pr=tr.intersect( tr, ray_orig, ray_dir,  hit);
	if(pr==false)
		return FALSE;

	pc->x=hit.m_point.x;
	pc->y=hit.m_point.y;
	pc->z=hit.m_point.z;

	return TRUE;
}

/*
double RayTri(CPoint3d &RayOrig1, CVector &RayDir, CPoint3d &t0, CPoint3d &t1, CPoint3d &t2, float& u, float& v) {
	float k0 = RayDir.DotP(RayOrig1);
	CVector RayOrig = RayOrig1 - RayDir*k0;
	CVector e1 (&t0, &t1);
	CVector e2 (&t0, &t2);
	CVector p(&RayDir, &e2);

	float d = e1.Dot(p);
	if (d == 0)return 1e15;
	CVector to (&t0, &RayOrig);
	float id = 1.0 / d;
	Vector3D q;
	q.cross(to, e1);
	if (__abs(d) > 0){
		u = to.dot(p) / d;
		if (u < -0.01 || u > 1.01) return FLT_MAX;
		v = RayDir.dot(q) / d;
		if (v < -0.01 || u + v > 1.01) return FLT_MAX;
	}
	else return false;
	return e2.dot(q) * id - k0;
}
*/
double grad_min_sek_angle(double grad,int min,int sek)
{
double angle;
double fmin=min;
double fsek=sek;

    if(grad<0)
		angle=grad-fmin/60.0-fsek/3600.0;
    else
		angle=grad+fmin/60.0+fsek/3600.0;
    return angle*PI/180.0;
}

void swap(int* val1, int* val2)
{
int tmp=*val2;
	*val2=*val1;
	*val1=tmp;
}

void XY_ab_r_X0Y0(double x, double y, double a, double b, double r, double*x0, double*y0)
{

    *x0=x+a*r;
    *y0=y+b*r;
}

BOOL mood_coord2Dma(double x0, double y0, double x1, double y1, double xm, double zm, double*xa, double*ya)
{
CPoint3d p0(x0, y0, 0);
CPoint3d p1(x1, y1, 0);
CPoint3d p2(x0, y0, 100);
CVector c1,c2,c3;
	if(POINTs_SC(&p0,&p1,&p2,&c1,&c2,&c3))
		return BAD;
CPoint3d pa(xm, 0, zm);
	pa.mod_coord_ma(&p0,&c1,&c2,&c3);
	*xa=pa.x;
	*ya=pa.y;
	return OK_AG;
}

void XY_XY_R_XY(double x1, double y1, double x2, double y2, double r, double*x, double*y)
{
CPoint3d p1(x1, y1, 0);
CPoint3d p2(x2, y2, 0);
CVector vect(&p1, &p2);
	p1.Shift(&vect, r, &p2);
	*x=p2.x;
	*y=p2.y;

}

BOOL IsValEven(int val)
{
	if(val==0)
		return TRUE;
	double dval=val/2.0;
	if((dval-int(dval))==0)
		return TRUE;
	return FALSE;
}

double dist_two_line(double p1[],double p2[],double p3[],double p4[])
{
double s1,s2,s3;
double q;
CVector l1((CPoint3d*)p1, (CPoint3d*)p2);
CVector l2((CPoint3d*)p3, (CPoint3d*)p4);

    s1=l1.m*l2.n-l1.n*l2.m;
    s2=l1.l*l2.n-l1.n*l2.l;
    s3=l1.l*l2.m-l1.m*l2.l;
    q=qxyz(s1,s2,s3);
    if(q==0)
      return   dist_POINT_LINE(p1,p3,&l2.l);		
    return fabs( ((p1[0]-p3[0])*s1-(p1[1]-p3[1])*s2+(p1[2]-p3[2])*s3)/q);

}

short cross_2dline_vector(double x1, double y1, double x2, double y2,double a,double b,double c, double*xc, double*yc)
{
	double a1,b1,c1;
    if(XY_XY_ABC(x1,y1,x2,y2,&a1,&b1,&c1))
	    return BAD;

	double delta1=x1*a+y1*b+c;
	double delta2=x2*a+y2*b+c;
	double dopusk=DDELTA;
    if(fabs(delta1)<dopusk || fabs(delta2)<dopusk)
		return BAD;
    if(cross2_line_2D(a1,b1,c1,a,b,c,xc,yc))
		return BAD;
    if((delta1>0&&delta2<0)||(delta1<0&&delta2>0))
		return OK_AG;
    return BAD;
}

short left_or_right(double x1, double y1, double x2, double y2, double x, double y)
{
double a,b,c;

    if(XY_XY_ABC(x1,y1,x2,y2,&a,&b,&c))
		return MIDDLE_DIR;
double delta=x*a+y*b+c;
    if(delta<-DDELTA)
		return RIGHT_DIR;
    if(delta>DDELTA)
		return LEFT_DIR;

    return MIDDLE_DIR;
}


short point_triangle2d_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,double delta)
{
double a1,b1,c1;
double a2,b2,c2;
double a3,b3,c3;


    if(XY_XY_ABC(p1->x,p1->y,p2->x,p2->y,&a1,&b1,&c1))
		return BAD;

    if(XY_XY_ABC(p2->x,p2->y,p3->x,p3->y,&a2,&b2,&c2))
		return BAD;

    if(XY_XY_ABC(p3->x,p3->y,p1->x,p1->y,&a3,&b3,&c3))
		return BAD;

double d1=a1*pc->x+b1*pc->y+c1;
double d2=a2*pc->x+b2*pc->y+c2;
double d3=a3*pc->x+b3*pc->y+c3;

    if(d1>0 && d2>0 && d3>0)
		return OK_AG;

    if(d1<0 && d2<0 && d3<0)
		return OK_AG;

    if(fabs(d1)<delta && XY_in_P1P2(pc->x,pc->y,p1->x,p1->y,p2->x,p2->y,delta/100.0))
	    return OK_AG;

    if(fabs(d2)<delta && XY_in_P1P2(pc->x,pc->y,p2->x,p2->y,p3->x,p3->y,delta/100.0))
	    return OK_AG;

    if(fabs(d3)<delta && XY_in_P1P2(pc->x,pc->y,p3->x,p3->y,p1->x,p1->y,delta/100.0))
	    return OK_AG;

    return BAD;
}

short point_RCT_2D_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,CPoint3d* p4)
{
    if(!point_triangle2d_out(pc,p1,p2,p3,DELTA))
		return 0;
    if(!point_triangle2d_out(pc,p1,p3,p4,DELTA))
		return 0;
    return 1;

}

void rotate_point_angle(double x_org, double y_org,double alfa, double* x, double* y)
{
double dx,dy;

    dx=*x-x_org;
    dy=*y-y_org;
    *x=x_org+dx*cos(alfa)-dy*sin(alfa);
    *y=y_org+dx*sin(alfa)+dy*cos(alfa);

}

#ifdef EXAMPLE_3D

Проверка пересечения луча с треугольником.

--------------------------------------------------------------------------------

 

    Для проверки столкновений и ray tracing'a необходимо определить, пересекается ли луч с 
	каким-либо полигоном сцены. Существует много разных способов решения этой задачи.
	Я расскажу вам о двух алгоритмах, по моему мнению, наиболее простых и эффективных.

 

Определение точки пересечения прямой с плоскостью.

 

    Допустим у вас есть отрезок, начинающийся в точке StartPoint и кончающийся в точке EndPoint.
Также у вас есть плоскость Ax + By + Cz + D = 0, где N={A;B;C}, как вы помните, вектор нормали,
а D - расстояние от начала координат до плоскости. Так как же определить точку пересечения 
плоскости с прямой? Это оказывается довольно просто, если немного вспомнить математику.

    Параметрическое уравнение прямой, проходящей через StartPoint и EndPoint, имеет вид:  

    

    PointOnRay = StartPoint + t*RayDir

 

RayDir - вектор, совпадающий с направлением прямой и равный EndPoint - StartPoint.
 При каком-то значении t прямая пересечет плоскость, если, конечно, она ей не параллельна.
  Найдем t.

    Пусть прямая пересекает плоскость в точке I (intersection point), тогда 

 

    I = StartPoint + t*RayDir    (1)

 

С другой стороны уравнение плоскости можно записать в следующем виде: N dot I = -D 
(dot - скалярное произведение векторов). Подставим (1) в это уравнение. 

 

    N dot (StartPoint + t*RayDir) = -D  <=> N dot StartPoint + t* (N dot RayDir) = -D 

 

Отсюда получаем: 

 

    t = -(D + N dot StartPoint)/(N dot RayDir)    (2)

 

Теперь, подставив (2) в уравнение (1), получим точку пересечения луча с плоскостью. 
Теперь немного проанализируем то, что мы получили.

Если N dot RayDir равно 0, то это значит, что прямая параллельна плоскости и никогда ее не 
пересечет. 

Если t < 0 или t > 1, то это значит, что точка пересечения лежит до StartPoint или за 
EndPoint соответственно. 

А вот и исходник прямо из моего 3d engine'а:


#define EPSILON 0.000001

int R_TraceRay(CVector start, CVector end)
{
    CVector dir, intersection;
    double cosang, dist, lamda;

    dir = end - start;

    for (int i=0; i<Level.numsurfaces; i++)
    {
        CSurface *s = &Level.surfaces[i];

        cosang = DotProduct(dir, s->normal);
        if (cosang > -EPSILON && cosang < EPSILON)
            continue; // Determine if ray paralle to a plane.

        dist = DotProduct(start, s->normal);

        lamda = (-s->dist-dist)/cosang;
        if (lamda < 0 || lamda > 1)
            continue;

        intersection = start + dir*lamda;

 

        // ...

        // Проверка, лежит ли точка внутри треугольника или нет.

        // ...

    }


    return 1; // no collision

}

Сложно? Вроде нет, но это только начало ;-)

 

Определение, лежит ли точка внутри треугольник или нет.

 

    У нас есть точка пересечения плоскости с лучом. Теперь надо определить, лежит ли эта точка 
	внутри трекгольника (полигона) или нет.

 

"Barycentric intersection" алгоритм.

 

    Это довольно быстрый алгоритм для определения, лежит ли точка внутри треугольника или снаружи.
	К сожалению, его нельзя применять для многоугольников. Но я думаю, что это и не надо, т.к.
	полигон всегда можно разбить на треугольники.

    Я не буду приводить весь вывод, т.к. это сплошная высшая алгебра, и большинство (такие как я)
	просто запутаются и ничего не поймут ;-)

    Параметрическое уравнение плоскости имеет вид:

 

    P = (1-u-v)*A + u*B + v*C    (3)

 

A, B и C - три точки, задающие плоскость. (Не путать с координатами вектора нормали!). Если мы
 будем изменять u и v от –Ґ до +Ґ, то будем всегда получать точки P, лежащие в плоскости ABC.

Если все коэффициенты і 0, то точка лежит внутри треугольника ABC, т.е. 1-u-v і 0, u і 0 и v і 0. 
Таким образом, надо выразить из (3) u и v, подставить вместо P точку пересечения прямой с 
треугольником и проверить следующие условия: 0 Ј u Ј 1 и 0 Ј v Ј (1–u). Если это выполняется,
 то точка пересечения лежит внутри треугольника ABC.

 

Алгоритм N2.

 

    Этот алгоритм тоже довольно хороший и его можно применять для полигонов. Я думаю, он даже 
	более быстрый, чем barycentric алгоритм (при соответствующей оптимизации), особенно если его 
	  применять для многоугольников.

    Пусть у нас есть на плоскости (!) треугольник ABC. Как определить, лежит ли точка M(x,y)
	 внутри треугольника или вне его? Будем считать, что треугольник задан векторами AB, BC и CA.
	  Если точка M лежит правее вектора AB, правее BC и CA, то она находится внутри треугольника.
	  Для определения, где лежит точка M относительно вектора AB, воспользуемся следующей формулой:

    j = (y - y0) (x1 - x0) - (x - x0) (y1 - y0)

 

(x0,y0), (x1,y1) - координаты точек А и B соответственно. Если j больше 0, то точка М лежит слева
 от вектора AB, если меньше 0, то справа от AB, а если j равно 0, то на AB. Таким образом если для 
	  всех сторон треугольника (полигона) выполняется неравенство j  Ј 0, то точка M лежит внутри
			  треугольника (полигона). Этот метод работает довольно быстро, т.к. часто условие не
			   выполняется уже при проверки первой стороны, и дальнейшие вычисления можно не выполнять.

    Вы скажите, причем тут треугольник на плоскости, если мы работаем в 3D? Все очень просто.
	Если мы спроецируем треугольник вместе с точкой его пересечения с лучом на плоскость Oxy,
	Oxz или Ozy, то ничего не изменится! Если точка лежала внутри треугольника (полигона), то 
	она так и будет там лежать. Проецирование выполняется элементарно: просто отбрасывается одна 
	 из координат x, y или z. Остается только выбрать плоскость проецирования. Если проецировать 
	 все треугольники на одну плоскость, то часть из них вырождается в линии.

    Для выбора плоскости проецирования надо определить абсолютное значение какой координаты 
	(x, y или z) вектора нормали является максимальным. Сделать это можно так:

 

if (  fabs(normal.x) >= fabs(normal.y)
    && fabs(normal.x) >= fabs(normal.z) )
{
    projection_plane = YZ;

}

Аналогично сравниваем координаты нормали y и z.

    Важно учесть, что в зависимости от знака normal.x (здесь я предполагаю, что fabs(normal.x)
	максимально, и проецируем на Oyz), условие на j будет меняться. Т.е. если normal.x < 0, то 
	точка будет лежать внутри треугольника при условии, что j для всех сторон больше или равно 
	  нулю. Другими словами, при проецировании может меняться порядок вершин. Если normal.x < 0,
	  и вершины расположены "по часовой стрелке", то при проецировании они будут расположены
	  "против часовой стрелки".

    Вот и все! Эти алгоритмы проверены и отлично работают, и если у вас что-то будет не получаться,
	то либо я где-то ошибся, либо вы что-то не поняли :-) Если у вас есть какие-нибудь дополнения
	   и/или исправления, то пишите мне.


#endif