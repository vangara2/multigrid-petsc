/*


*/


#ifndef _ARRAY_H_
#define _ARRAY_H_ 

#include <stdlib.h>
// Structures

typedef struct {
	int start;
	int end;
} IsRange;

typedef struct {
	int ni;
	int nj;
	double* data;
} Array2d;

typedef struct {
	int ni;
	int nj;
	int nk;
	double* data;
} Array3d;

typedef struct {
	int ni;
	int* data;
} ArrayInt1d;

typedef struct {
	int ni;
	int nj;
	int* data;
} ArrayInt2d;

typedef struct {
	int ni;
	int nj;
	int nk;
	int* data;
} ArrayInt3d;

typedef struct {
	int	ni;
	int	nj;
	int*	indices;
	double*	data;
} Metrics2d;

// Function prototypes

void CreateArrayInt2d(int ni, int nj, ArrayInt2d *a);
void DeleteArrayInt2d(ArrayInt2d *a); 
void SetArrayInt2d(ArrayInt2d *a, int v);

void CreateArray2d(int ni, int nj, Array2d *a);
void DeleteArray2d(Array2d *a);
void SetArray2d(ArrayInt2d *a, double v);

extern int malloc2d(double ***a, int n, int m);
extern int malloc3d(double ****a, int p, int q, int r);
extern int malloc2dY(double ***a, int n, int *m);

extern void free2dArray(double ***a);
extern void free3dArray(double ****a);

#endif
