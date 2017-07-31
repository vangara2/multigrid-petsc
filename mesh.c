#include "mesh.h"

#define ERROR_MSG(message) (fprintf(stderr,"Error:%s:%d: %s\n",__FILE__,__LINE__,(message)))
#define ERROR_RETURN(message) {ERROR_MSG(message);return ierr;}
#define CHKERR_PRNT(message) {if(ierr==1) {ERROR_MSG(message);}}
#define CHKERR_RETURN(message) {if(ierr==1) {ERROR_RETURN(message);}}
#define PI 3.14159265358979323846

#define METRICS(i,j,k) (metrics->data[metrics->nk*((i)*metrics->nj+(j))+(k)])
#define isGRIDtoGLOBAL(i,j) (IsGridToGlobal.data[((i)*IsGridToGlobal.nj+(j))])
#define isGLOBALtoGRID(i,j) (IsGlobalToGrid.data[((i)*IsGlobalToGrid.nj+(j))])

int UniformMesh(double ***pcoord, int *n, double *bounds, double *h, int dimension) {
	
	int ierr = 0;
	
	//Assign memory
	ierr = malloc2dY(pcoord,dimension,n); CHKERR_RETURN("malloc failed");
	//Compute uniform grid in each dimension
	for(int i=0;i<dimension;i++){
		if (n[i]<2) {ierr=1; ERROR_RETURN("Need at least 2 points in each direction");}
		(*pcoord)[i][0] = bounds[i*2]; //Lower bound
		(*pcoord)[i][n[i]-1] = bounds[i*2+1]; //Upper bound
		
		h[i] = ((*pcoord)[i][n[i]-1]-(*pcoord)[i][0])/(n[i]-1); //Spacing
		for(int j=1;j<n[i]-1;j++){
			(*pcoord)[i][j] = (*pcoord)[i][j-1] + h[i];
		}
	}

	return ierr;
}

int NonUniformMeshY(double ***pcoord, int *n, double *bounds, double *h, int dimension, double (*Transform)(double *bounds, double length, double xi) ) {
	
	int	ierr = 0;
	double	length, d[dimension];
	
	//Assign memory
	ierr = malloc2dY(pcoord,dimension,n); CHKERR_RETURN("malloc failed");
	//Compute uniform grid in each dimension
	for(int i=0;i<1;i++){
		if (n[i]<2) {ierr=1; ERROR_RETURN("Need at least 2 points in each direction");}
		(*pcoord)[i][0] = bounds[i*2]; //Lower bound
		(*pcoord)[i][n[i]-1] = bounds[i*2+1]; //Upper bound
		
		d[i] = ((*pcoord)[i][n[i]-1]-(*pcoord)[i][0])/(n[i]-1); //Spacing
		for(int j=1;j<n[i]-1;j++){
			(*pcoord)[i][j] = (*pcoord)[i][j-1] + d[i];
		}
	}

	for(int i=1;i<2;i++){
		if (n[i]<2) {ierr=1; ERROR_RETURN("Need at least 2 points in each direction");}
		(*pcoord)[i][0] = bounds[i*2]; //Lower bound
		(*pcoord)[i][n[i]-1] = bounds[i*2+1]; //Upper bound
		
		length = ((*pcoord)[i][n[i]-1]-(*pcoord)[i][0]);
		d[i] = 0.0;
		for(int j=1;j<n[i]-1;j++){
			(*pcoord)[i][j] = (*Transform)(&(bounds[i*2]), length, j/(double)(n[i]-1));
			d[i] = fmax(d[i],fabs((*pcoord)[i][j]-(*pcoord)[i][j-1])); 
		}
		d[i] = fmax(d[i],fabs((*pcoord)[i][n[i]-2]-(*pcoord)[i][n[i]-1])); 
	}

	for(int i=2;i<dimension;i++){
		if (n[i]<2) {ierr=1; ERROR_RETURN("Need at least 2 points in each direction");}
		(*pcoord)[i][0] = bounds[i*2]; //Lower bound
		(*pcoord)[i][n[i]-1] = bounds[i*2+1]; //Upper bound
		
		d[i] = ((*pcoord)[i][n[i]-1]-(*pcoord)[i][0])/(n[i]-1); //Spacing
		for(int j=1;j<n[i]-1;j++){
			(*pcoord)[i][j] = (*pcoord)[i][j-1] + d[i];
		}
	}
	
	*h = 0.0;
	for (int i=0;i<dimension;i++){
		*h = *h + d[i]*d[i];
	}
	*h = sqrt(*h);

	return ierr;
}

int MetricCoefficients2D(Array2d *metrics, double **coord, ArrayInt2d IsGlobalToGrid, int *range, double *bounds, int dimension, void (*MetricCoefficientsFunc)(double *metricsAtPoint, double *bounds, double *lengths, double x, double y)) {
	//This is a metric coefficients computing shell
	//Note: Metric coefficients at each point excluding BC points are computed
	//
	//metrics.indices[(n[0]-2)*(n[1]-2)][5]
	//metrics.data[(n[0]-2)*(n[1]-2)][5]
	//range[0:1] = start to end global index of unknowns in this process
	//
	//bounds[0] - Lower bound of "x"
	//bounds[1] - Upper bound of "x"
	//bounds[2] - Lower bound of "y"
	//bounds[3] - Upper bound of "y"
	//
	//(*MetricCoefficientsFunc) compute and return metric coefficients at (x,y)
	
	double	lengths[dimension];
	int	igrid, jgrid;
	int	ierr = 0;

//	int	procs, rank;
//	
//	MPI_Comm_size(PETSC_COMM_WORLD, &procs);
//	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

//	ierr = malloc3d(metrics, n[1]-2, n[0]-2, 5); CHKERR_RETURN("malloc failed");
	metrics->ni = range[1]-range[0];
	metrics->nj = 5;
//	metrics->indices = malloc(metrics->ni*metrics->nj*sizeof(int));if (metrics->indices==NULL) ERROR_MSG("malloc failed");
	metrics->data = malloc(metrics->ni*metrics->nj*sizeof(double));if (metrics->data==NULL) ERROR_MSG("malloc failed");

	for(int i=0;i<dimension;i++) {
		lengths[i] = bounds[i*2+1] - bounds[i*2];
	}
	
	for(int i=range[0];i<range[1];i++) {
		igrid = isGLOBALtoGRID(i,0);
		jgrid = isGLOBALtoGRID(i,1);
		(*MetricCoefficientsFunc)(((metrics->data)+((i-range[0])*metrics->nj)),bounds,lengths,coord[0][jgrid+1],coord[1][igrid+1]);
	}
//	for(int i=0;i<metrics->ni;i++) {
////	for(int i=1;i<2;i++) {
//		for(int j=0;j<metrics->nj;j++) {
//			(*MetricCoefficientsFunc)(((metrics->data)+(metrics->nk*(i*metrics->nj+j))),bounds,lengths,coord[0][j+1],coord[1][i+1]);
//		}
//	}

//	for (int i=0;i<metrics->ni;i++) {
//		for (int j=0;j<metrics->nj;j++) {
//			PetscSynchronizedPrintf(PETSC_COMM_WORLD,"rank = %d: metrics[%d][%d]: %f %f %f %f %f\n",rank,i,j,METRICS(i,j,0),METRICS(i,j,1),METRICS(i,j,2),METRICS(i,j,3),METRICS(i,j,4));
//		}
//	}
	return ierr;
}
