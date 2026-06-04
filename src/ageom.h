/********************************AGeom.h*****************************/
#ifndef _LIBAGEOM_H
#define _LIBAGEOM_H

#include <cmath>                    // for sqrt() and pow()
#include "Vector.h"
#include "Point3d.h"

enum {
RIGHT_DIR,
LEFT_DIR,
MIDDLE_DIR
};

#define GRAD_TO_RAD(grad) (grad)*(PI/180.0)
#define RAD_TO_GRAD(rad) (rad)*(180.0/PI)


#define Num_el(arr)	((unsigned int) (sizeof(arr) / sizeof(arr[0])))


#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (x) : (y))

extern inline long double qxy(long double qx,long double qy){	return sqrt(pow(qx,2)+pow(qy,2));}

extern inline long double qxyz(long double qx,long double qy,long double qz){ return sqrt(pow(qx,2)+pow(qy,2)+pow(qz,2));}

extern double	dist_POINT(double p1[],double p2[]);
extern inline double dist_POINT(CPoint3d* p1,CPoint3d* p2)
{
double ax=p2->x-p1->x;
double ay=p2->y-p1->y;
double az=p2->z-p1->z;
    return qxyz(ax,ay,az);
}

extern inline double dist_XY_XY(double x1,double y1,double x2,double y2)
{
double ax=x2-x1;
double ay=y2-y1;
    return sqrt(pow(ax,2)+pow(ay,2));
}

inline bool Equals(const float x, const float y, const float Eps) {
    return fabs(x - y) <= Eps;
}

extern double	dist_POINT_LINE(double p0[],double p1[],double v[]);
extern double	get_dist_XY_line2D(double x0,double y0,double x1,double y1,double x2,double y2);

extern short cross2_line_2D(double a1,double b1,double c1,double a2,double b2,double c2,double *x,double *y);
extern void line_ortho_line(double a1,double b1,double x,double y,double *a2,double *b2,double *c2);
extern short XY_XY_ABC(double x1,double y1,double x2,double y2,double *a,double *b,double *c);

extern double	XYZ_XYZ_LMN(double p1[],double p2[],double n[]);
extern short POINT_in_RECT(double x,double y,double x1,double y1,double x2,double y2);
extern short XY_in_P1P2(double x0,double y0,double x1,double y1,double x2,double y2,double dopusk=DDELTA);

extern short POINTs_SC(CPoint3d* p0,CPoint3d* px,CPoint3d* py,CVector *vx,CVector *vy,CVector *vz);

extern BOOL crossing_line_triangle(CPoint3d* p0, CVector* v,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,CPoint3d* pc);
extern double get_square(CPoint3d* p1,CPoint3d* p2,CPoint3d* p3);
extern BOOL point_triangle_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3);
extern double grad_min_sek_angle(double grad,int min,int sek);
extern void swap(int* val1, int* val2);
extern void XY_ab_r_X0Y0(double x, double y, double a, double b, double r, double*x0, double*y0);
extern void XY_XY_R_XY(double x1, double y1, double x2, double y2, double r, double*x, double*y);
extern BOOL mood_coord2Dma(double x0, double y0, double x1, double y1, double xm, double zm, double*xa, double*ya);
extern BOOL IsValEven(int val);/////////Четное число или нет
extern double dist_two_line(double p1[],double p2[],double p3[],double p4[]);
extern short cross_2dline_vector(double x1, double y1, double x2, double y2,double a,double b,double c, double*xc, double*yc);
extern short left_or_right(double x1, double y1, double x2, double y2, double x, double y);
extern short point_RCT_2D_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,CPoint3d* p4);
extern short point_triangle2d_out(CPoint3d* pc,CPoint3d* p1,CPoint3d* p2,CPoint3d* p3,double delta);
extern void rotate_point_angle(double x_org, double y_org,double alfa, double* x, double* y);




#endif /* _LIBAGEOM_H */

