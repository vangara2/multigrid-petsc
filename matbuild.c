#include "matbuild.h"

#define METRICS(i,j) (metrics.data[(i)*metrics.nj+(j)])
//#define METRICS(i,j,k) (metrics.data[metrics.nk*((i)*metrics.nj+(j))+(k)])
#define PMETRICS(i) ((metrics.data)+((i)*metrics.nj))
#define F(i,j) (f.data[((i)*f.nj+(j))])
#define U(i,j) (u.data[((i)*u.nj+(j))])

static int ipow(int base, int exp) {

	int result = 1;
	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}
	return result;
}

void GridId(Indices *indices) {
	// Computes the gridId range and number grids in each level
	
	int q, count;
	
	count = 0;
	q = indices->totalGrids/indices->levels;
	for (int l=0;l<indices->levels;l++) {
		indices->level[l].grids = q;
		indices->level[l].gridId = malloc(indices->level[l].grids*sizeof(int));
		for (int g=0;g<indices->level[l].grids;g++) {
			indices->level[l].gridId[g] = count;
			count += 1;
		}
	}
}

void CreateIndexMaps(Mesh *mesh, Indices *indices) {
	// Allocate memory to global-to-grid and grid-to-global maps
	
	int	temp, g;
	int	totaln, n0, n1, fn0, fn1;
	int	factor;
	const	int	M = 3; // i,j,g
	
	factor = indices->coarseningFactor;
	fn0 = mesh->n[0];
	fn1 = mesh->n[1];
	for (int l=0;l<indices->levels;l++) {
		totaln = 0;
		for (int lg=0;lg<indices->level[l].grids;lg++) {
			g = indices->level[l].gridId[lg];
			temp = ipow(factor,g);
			n0 = (fn0-1)/temp - 1;
			n1 = (fn1-1)/temp - 1;
			totaln = totaln + n0*n1; 
			CreateArrayInt2d(n1, n0,&(indices->level[l].grid[lg]));
		}
		CreateArrayInt2d(totaln, M, &(indices->level[l].global));
	}
}

void DestroyIndexMaps(Indices *indices) {
	// Free the memory of global-to-grid and grid-to-global maps	
	
	for (int l=0;l<indices->levels;l++) {
		for (int g=0;g<indices->level[l].grids;g++) {
			DeleteArrayInt2d(&(indices->level[l].grid[g]));
//			free((*IsGridToGlobal)[l].grid[g].data);
		}
		DeleteArrayInt2d(&(indices->level[l].global));
	}
}

void SetUpIndices(Mesh *mesh, Indices *indices) {
	// Get the GridId range, then allocate memory
	
	//indices->coarseningFactor = 2;	
	indices->level = malloc(indices->levels*sizeof(Level));
	GridId(indices);	
	for (int i=0;i<indices->levels;i++) {
		indices->level[i].grid = malloc(indices->level[i].grids*sizeof(ArrayInt2d));
		indices->level[i].h = malloc(indices->level[i].grids*sizeof(double[2]));
	}
	CreateIndexMaps(mesh, indices);
	for (int l=0;l<indices->levels;l++) {
		for (int lg=0;lg<indices->level[l].grids;lg++) {
			indices->level[l].h[lg][0] = 1.0/(indices->level[l].grid[lg].ni+1);
			indices->level[l].h[lg][1] = 1.0/(indices->level[l].grid[lg].nj+1);
		}
	}
}

void DestroyIndices(Indices *indices) {
	// Free the memory allocated to indices
	
	DestroyIndexMaps(indices);
	for (int l=0;l<indices->levels;l++) {
		free(indices->level[l].grid);
		free(indices->level[l].h);
		free(indices->level[l].gridId);
	}
	free(indices->level);
}

void mapping(Indices *indices) {
	// Maps global indices to grid unknowns and vice-versa
	// 
	// IsGridToGlobal[i][j] = globalIndex
	// IsGlobalToGrid[globalIndex][0/1] = i/j
	// range[level].start = Starting global index in level
	// range[level].end = 1+(ending global index) in level
	
	int	count;
	int	g;
	ArrayInt2d	a, b;
	
	for (int l=0;l<indices->levels;l++) {
		count = 0;
		a = indices->level[l].global;
		for (int lg=0;lg<indices->level[l].grids;lg++) {
			g = indices->level[l].gridId[lg];
			b = indices->level[l].grid[lg];
			for (int i=0;i<b.ni;i++) {
				for (int j=0;j<b.nj;j++) {
					b.data[i*b.nj+j] = count;
					a.data[count*a.nj+0] = i;
					a.data[count*a.nj+1] = j;
					a.data[count*a.nj+2] = g;
					count = count + 1;
				}
			}
		}
	}
}

void SetUpOperator(Indices *indices, Operator *op) {
	// Allocates memory to Operator struct
	int	order; // order of grid transfer operators
	int	stencilSize;
	
	order = indices->coarseningFactor; // order of grid transfer operators is same as the coarsening factor
	op->totalGrids = indices->totalGrids;
	op->res = malloc((op->totalGrids-1)*sizeof(ArrayInt2d));
	op->pro = malloc((op->totalGrids-1)*sizeof(ArrayInt2d));
	stencilSize = 1;
	for (int i=0;i<op->totalGrids-1;i++) {
		stencilSize = (stencilSize+1)*order-1;
		CreateArray2d(stencilSize, stencilSize, op->res+i);
		CreateArray2d(stencilSize, stencilSize, op->pro+i);
	}

}

void DestroyOperator(Operator *op) {
	// Free the memory in Operator struct
	
	for (int i=0;i<op->totalGrids-1;i++) {
		DeleteArray2d(op->res+i);
		DeleteArray2d(op->pro+i);
	}
	free(op->res);
	free(op->pro);
}

void SetUpAssembly(Indices *indices, Assembly *assem) {
	// Allocate memory for Assembly struct
	
	assem->levels = indices->levels;
	assem->res = malloc((assem->levels-1)*sizeof(Mat));
	assem->pro = malloc((assem->levels-1)*sizeof(Mat));
	assem->level = malloc((assem->levels)*sizeof(LinSys));
}

void DestroyAssembly(Assembly *assem) {
	// Free the memory in Assembly struct
	
	for (int l=0;l<assem->levels,l++) {
		MatDestroy(&(assem->level[l].A));
		VecDestroy(&(assem->level[l].b));
		VecDestroy(&(assem->level[l].u));
	}
	free(assem->res);
	free(assem->pro);
	free(assem->level);
}

void insertSubMatValues(Mat *subA, int nrows, Mat *A, int i0, int j0) {
	//Insert values of sub matrix "subA" in "A"
	//
	//nrows	- number of rows in "subA"
	//ncols - number of columns in "subA"
	//
	//A(i0+subi, j0+subj) = subA(subi,subj)
	
	const	int	*subj;
	const	double	*vals;
		int	ncols;
	
	for (int subi=0; subi<nrows; subi++) {
		MatGetRow(*subA, subi, &ncols, &subj, &vals);
		for (int jcol=0; jcol<ncols; jcol++) {
			if (vals[jcol]!=0.0)	MatSetValue(*A, i0+subi, j0+subj[jcol], vals[jcol], INSERT_VALUES);
		}
		MatRestoreRow(*subA, subi, &ncols, &subj, &vals);
	}
}

void insertSubVecValues(Vec *subV, Vec *V, int i0) {
	//Insert values of sub vector "subV" in a vector "V"
	//
	//V(i0+i) = subV(i)
	
	double	*vals;
	int	n;
	
	VecGetLocalSize(*subV, &n);
	VecGetArray(*subV, &vals);
	for (int i=0; i<n; i++) {
		if (vals[i]!=0.0) VecSetValue(*V, i0+i, vals[i], INSERT_VALUES);
	}
	VecRestoreArray(*subV, &vals);
}

Mat levelMatrixA(Array2d metrics, ArrayInt2d IsStencil, int n, int l) {
	// Builds matrix "A" at a given multigrid level
	// metrics	- metric terms in global index array
	// n		- number of unknowns per dimension
	// l		- level
	
//	int	rows, cols, idummy, jdummy;
	double	As[5], h[2];
	Mat	A;
	
	int 	procs, rank;
//	int	ln;
//	int	stencilsize;
	int	*col;
	int	range[2];

//	rows = n*n;
//	cols = rows;
//	
	MPI_Comm_size(PETSC_COMM_WORLD, &procs);
	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
	
//	if (rank<(n*n)%procs) {
//		ln = (n*n)/procs + 1;
//	}
//	else {
//		ln = (n*n)/procs;
//	}

	MatCreateAIJ(PETSC_COMM_WORLD, IsStencil.ni, IsStencil.ni, PETSC_DETERMINE, PETSC_DETERMINE, 5, PETSC_NULL, 4, PETSC_NULL, &A);
//	MatCreateSeqAIJ(PETSC_COMM_SELF, rows, cols, 5, NULL, A);
//	MatGetOwnershipRange(subA[l], &rowStart, &rowEnd);
//	printf("level: %d\n",l);
	MatGetOwnershipRange(A, range, range+1);
//	if (rank==0) {

	h[0] = 1.0/(n+1);
	h[1] = h[0];
	for (int i=range[0]; i<range[1]; i++) {
//		printf("\ni = %d, im = %d, jm = %d\n",i,ipow(2,l)*((i/n[l])+1)-1,ipow(2,l)*((i%n[l])+1)-1);	
//		OpA(As,metrics[ipow(2,l)*((i/n)+1)-1][ipow(2,l)*((i%n)+1)-1],h);
//		idummy = ipow(2,l)*((i/n)+1)-1;
//		jdummy = ipow(2,l)*((i%n)+1)-1;
//		OpA(As,PMETRICS(idummy, jdummy),h);
		OpA(As,PMETRICS(i-range[0]),h);
	//	printf("\nrow = %d; As[0] = %f\n",i,As[0]);
		col = IsStencil.data+((i-range[0])*IsStencil.nj);
		if (col[0]>-1) {
			MatSetValue(A, i, col[0], As[0], INSERT_VALUES);
		}
		if (col[1]>-1) {
			MatSetValue(A, i, col[1], As[1], INSERT_VALUES); 
		}
		MatSetValue(A, i, i, As[2], INSERT_VALUES);
		if (col[3]>-1) {
			MatSetValue(A, i, col[3], As[3], INSERT_VALUES);
		}
		if (col[4]>-1) {
			MatSetValue(A, i, col[4], As[4], INSERT_VALUES);
		}
	}
//	for (int i=0; i<rows; i++) {
////		printf("\ni = %d, im = %d, jm = %d\n",i,ipow(2,l)*((i/n[l])+1)-1,ipow(2,l)*((i%n[l])+1)-1);	
////		OpA(As,metrics[ipow(2,l)*((i/n)+1)-1][ipow(2,l)*((i%n)+1)-1],h);
//		idummy = ipow(2,l)*((i/n)+1)-1;
//		jdummy = ipow(2,l)*((i%n)+1)-1;
//		OpA(As,PMETRICS(idummy, jdummy),h);
//	//	printf("\nrow = %d; As[0] = %f\n",i,As[0]);
//		if (i-n>=0) {
//			MatSetValue(A, i, i-n, As[0], INSERT_VALUES);
//		}
//		if (i-1>=0 && i%n!=0) {
//			MatSetValue(A, i, i-1, As[1], INSERT_VALUES); 
//		}
//		MatSetValue(A, i, i, As[2], INSERT_VALUES);
//		if (i+1<=rows-1 && (i+1)%n!=0) {
//			MatSetValue(A, i, i+1, As[3], INSERT_VALUES);
//		}
//		if (i+n<=rows-1) {
//			MatSetValue(A, i, i+n, As[4], INSERT_VALUES);
//		}
//	}

//	}
	MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);

	return A;
}

void levelMatrixA1(Problem *prob, Mesh *mesh, Level *level, int factor, Mat *A) {
	// Build matrix "A" for a given level
	// level - contains index maps
	// factor - coarsening factor
	
	double		*a, *b;
	int		ai, aj, bi, bj;
	int		grids, *gridId;
	int		range[2];
	double		As[5];

	int		i0, j0, g0;
	int		ifine, jfine;
	int		g1;

	double		metrics[5], **coord;
	
	int	procs, rank;
	
	MPI_Comm_size(PETSC_COMM_WORLD, &procs);
	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
	
	coord = mesh->coord;

	ai = level->global.ni;
	aj = level->global.nj;
	a  = level->global.data;

	grids = level->grids;
	gridId = level->gridId;
	
	MatCreateAIJ(PETSC_COMM_WORLD, PETSC_DECIDE, PETSC_DECIDE, ai, ai, 6*grids, PETSC_NULL, 6*grids, PETSC_NULL, A);

	MatGetOwnershipRange(*A, range, range+1);
	
	// Row-based fill:
	for (int row=range[0];row<range[1];row++) {
		//i0 - row    - y coord
		//j0 - column - x coord
		//A[0]*u(i0-1,j0) + A[1]*u(i0,j0-1) + A[2]*u(i0,j0) + A[3]*u(i0,j0+1) + A[4]*u(i0+1,j0) = f(i0,j0)
		i0 = a[row*aj];
		j0 = a[row*aj+1];
		g0 = a[row*aj+2]; 
		for (int lg=0;lg<grids;lg++) {
			g1 = gridId[lg];
			if (g1-g0 == 0) {
				// Fill the jacobian
				bi = level->grid[lg].ni;
				bj = level->grid[lg].nj;
				b  = level->grid[lg].data;
				ifine = ipow(factor,g0)*(i0+1)-1;
				jfine = ipow(factor,g0)*(j0+1)-1;
				mesh->MetricCoefficients(&mesh, coord[0][ifine], coord[1][jfine], metrics);
				prob->OpA(As, metrics, level->h[lg]);
				
				PetscSynchronizedPrintf(PETSC_COMM_WORLD,"rank = %d; As[%d][%d]: %f %f %f %f %f\n",rank, i0, j0, As[0], As[1], As[2], As[3], As[4]);
				PetscSynchronizedFlush(PETSC_COMM_WORLD,PETSC_STDOUT);

				if (i0-1>=0) {
					MatSetValue(*A, row, b[(i0-1)*bj+j0], As[0], INSERT_VALUES);
				}
				if (j0-1>=0) {
					MatSetValue(*A, row, b[i0*bj+j0-1], As[1], INSERT_VALUES);
				}
				MatSetValue(*A, row, row, As[2], INSERT_VALUES);
				if (j0+1<bj) {
					MatSetValue(*A, row, b[i0*bj+j0+1], As[3], INSERT_VALUES);
				}
				if (i0+1<=bi) {
					MatSetValue(*A, row, b[(i0+1)*bj+j0], As[4], INSERT_VALUES);
				}
			}
		}
	}
	
	MatAssemblyBegin(*A,MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(*A,MAT_FINAL_ASSEMBLY);
}

void Assemble(Problem *prob, Mesh *mesh, Indices *indices, Operator *op, Assembly *assem) {
	// Assembles matrices and vectors in all levels
	int	factor;

	factor = indices->coarseningFactor;
	for (int l=0;l<assem->levels;l++) {

		levelMatrixA1(prob, mesh, &(indices->level[l]), factor, &(assem->level[l].A));
	}
}

//Mat matrixA(double *metrics, double **opIH2h, double **opIh2H, int n0, int levels) {
//	// Builds matrix "A" for implicit multigrid correction method
//	// metrics	- metric terms
//	// opIH2h	- Stencilwise prolongation operator
//	// opIh2H	- Stencilwise restriction operator
//	// n0		- Number of unknowns per dimension
//	// levels	- Number of levels
//	
//	Mat	A, subA[levels], prolongMatrix[levels-1], restrictMatrix[levels-1];
//	Mat	UB[levels-1], LB[levels-1];
//	int	n[levels];
//	int	rows[levels], cols[levels];//, ncols;
//	int	rowStart, rowEnd, blockRowStart[levels], blockColStart[levels];
//	double	As[5], h[2];
//
//	int	rank;
//
//	n[0] = n0;
//	blockRowStart[0] = 0;
//	blockColStart[0] = 0;
//	rows[0] = n[0]*n[0];
//	cols[0] = rows[0];
//
//	for (int l=0;l<levels-1;l++) {
//		blockRowStart[l+1] = blockRowStart[l] + rows[l];
//		blockColStart[l+1] = blockRowStart[l+1];
//		n[l+1] = (n[l]-1)/2;
//		rows[l+1] = n[l+1]*n[l+1];
//		cols[l+1] = rows[l+1];
////		restrictMatrix[l] =  restrictionMatrix(opIh2H, 3, n[l], n[l+1]);
////		prolongMatrix[l] =  prolongationMatrix(opIH2h, 3, n[l], n[l+1]);
//	}
//
//	MatCreateAIJ(PETSC_COMM_WORLD, PETSC_DECIDE, PETSC_DECIDE, blockRowStart[levels-1]+rows[levels-1], blockColStart[levels-1]+cols[levels-1], 6*levels, PETSC_NULL, 6*levels, PETSC_NULL,&A);
////	MatCreate(PETSC_COMM_WORLD, &A);
////	MatSetSizes(A, PETSC_DECIDE, PETSC_DECIDE, blockRowStart[levels-1]+rows[levels-1], blockColStart[levels-1]+cols[levels-1]);
////	MatSetFromOptions(A);
////	MatSetUp(A);
//	
//	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
//	if (rank==0) {
//
//	for (int l=0;l<levels-1;l++) {
//		restrictMatrix[l] =  restrictionMatrix(opIh2H, 3, n[l], n[l+1]);
//		prolongMatrix[l] =  prolongationMatrix(opIH2h, 3, n[l], n[l+1]);
//	}
//
//	for (int l=0;l<levels;l++) {
//
//		h[0] = 1.0/(n[l]+1);
//		h[1] = h[0];
//		
//		MatCreateSeqAIJ(PETSC_COMM_SELF, rows[l], cols[l], 5, NULL, &(subA[l]));
////		MatCreate(PETSC_COMM_WORLD, &(subA[l]));
////		MatSetSizes(subA[l], PETSC_DECIDE, PETSC_DECIDE, rows[l], cols[l]);
////		MatSetFromOptions(subA[l]);
////		MatSetUp(subA[l]);
//		MatGetOwnershipRange(subA[l], &rowStart, &rowEnd);
//	//	printf("level: %d\n",l);
//		for (int i=rowStart; i<rowEnd; i++) {
//	//		printf("\ni = %d, im = %d, jm = %d\n",i,ipow(2,l)*((i/n[l])+1)-1,ipow(2,l)*((i%n[l])+1)-1);	
//			OpA(As,(metrics+5*((ipow(2,l)*((i/n[l])+1)-1)*(n0)+(ipow(2,l)*((i%n[l])+1)-1))),h);
//		//	printf("\nrow = %d; As[0] = %f\n",i,As[0]);
//			if (i-n[l]>=0) {
//				MatSetValue(subA[l], i, i-n[l], As[0], INSERT_VALUES);
//			}
//			if (i-1>=0 && i%n[l]!=0) {
//				MatSetValue(subA[l], i, i-1, As[1], INSERT_VALUES); 
//			}
//			MatSetValue(subA[l], i, i, As[2], INSERT_VALUES);
//			if (i+1<=rows[l]-1 && (i+1)%n[l]!=0) {
//				MatSetValue(subA[l], i, i+1, As[3], INSERT_VALUES);
//			}
//			if (i+n[l]<=rows[l]-1) {
//				MatSetValue(subA[l], i, i+n[l], As[4], INSERT_VALUES);
//			}
//		}
//		MatAssemblyBegin(subA[l],MAT_FINAL_ASSEMBLY);
//		MatAssemblyEnd(subA[l],MAT_FINAL_ASSEMBLY);
//		
//		//MatView(subA[l], PETSC_VIEWER_STDOUT_WORLD);
//		insertSubMatValues(&(subA[l]), rows[l], &A, blockRowStart[l], blockColStart[l]);
//		
//		if (l!=levels-1) {
//			MatMatMult(subA[l], prolongMatrix[l], MAT_INITIAL_MATRIX, 1.0, &(UB[l]));
//			MatMatMult(restrictMatrix[l], subA[l], MAT_INITIAL_MATRIX, 1.0, &(LB[l]));
//			
//			insertSubMatValues(&(UB[l]), rows[l], &A, blockRowStart[l], blockColStart[l+1]);
//			insertSubMatValues(&(LB[l]), rows[l+1], &A, blockRowStart[l+1], blockColStart[l]);
//			
//			for (int b=l+1;b<levels-1;b++) {
//				MatMatMult(UB[b-1], prolongMatrix[b], MAT_INITIAL_MATRIX, PETSC_DEFAULT, &(UB[b]));
//				MatMatMult(restrictMatrix[b], LB[b-1], MAT_INITIAL_MATRIX, PETSC_DEFAULT, &(LB[b]));
//				
//				insertSubMatValues(&(UB[b]), rows[l], &A, blockRowStart[l], blockColStart[b+1]);
//				insertSubMatValues(&(LB[b]), rows[b+1], &A, blockRowStart[b+1], blockColStart[l]);
//				
//				MatDestroy(&(UB[b-1]));
//				MatDestroy(&(LB[b-1]));
//			}
//			MatDestroy(&(UB[levels-2]));
//			MatDestroy(&(LB[levels-2]));
//			MatDestroy(&(prolongMatrix[l]));
//			MatDestroy(&(restrictMatrix[l]));
//		}
//		MatDestroy(&(subA[l]));
//
//	}
//	
//	}
//	MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
//	MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);
//	return A;
//}

Mat GridTransferMatrix(double **Is, int m, int nh, int nH, char *type) {
	// Is	- stencil wise grid transfer operator of size m*m
	// nh	- number of unknowns per dimension in fine grid "h"
	// nH	- number of unknowns per dimension in coarse grid "H"
	// type	- "Restriction" or "Prolongation"
	
	Mat	matI;
	int	rowStart, rowEnd, colStart, colEnd;
	char	res[15] = "Restriction", pro[15] = "Prolongation";
	int	flag;

	MatCreate(PETSC_COMM_WORLD, &matI);
	if (strcmp(type, res)) {
		flag = 0;
		MatSetSizes(matI, PETSC_DECIDE, PETSC_DECIDE, nH*nH, nh*nh);
	} else if (strcmp(type, pro)) {
		flag = 1;
		MatSetSizes(matI, PETSC_DECIDE, PETSC_DECIDE, nh*nh, nH*nH);
	} else {
		printf("'%s' is not a valid grid transfer operation; use '%s' or '%s'\n",type,res,pro);
		return matI;
	}
	MatSetFromOptions(matI);
	MatSetUp(matI);
	for (int bj=0;bj<nH;bj++) {
		colStart = bj*nH;
		colEnd   = colStart+nH;
		rowStart = (bj*nh)*((m+1)/2);
		for (int bi=0;bi<m;bi++) {
			for (int j=colStart;j<colEnd;j++) {
				rowEnd  = rowStart + m;
				for (int i=rowStart;i<rowEnd;i++) {
					if (flag == 0 && Is[bi][i-rowStart]!=0.0) {
						MatSetValue(matI, j, i, Is[bi][i-rowStart], INSERT_VALUES);
					} else if (Is[bi][i-rowStart]!=0.0) {
						MatSetValue(matI, i, j, Is[bi][i-rowStart], INSERT_VALUES);
					}
				}
				rowStart = rowStart + ((m+1)/2);
			}
			rowStart = rowEnd;
		}
	}
	
	MatAssemblyBegin(matI, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(matI, MAT_FINAL_ASSEMBLY);
	//MatView(matI, PETSC_VIEWER_STDOUT_WORLD);
	return matI;
}

Mat restrictionMatrix(double **Is, int m, int nh, int nH) {
	// Is	- stencil wise grid transfer operator of size m*m
	// nh	- number of unknowns per dimension in fine grid "h"
	// nH	- number of unknowns per dimension in coarse grid "H"
	
	Mat	matI;
	int	rowStart, rowEnd, colStart, colEnd;

	//MatCreateAIJ(PETSC_COMM_WORLD, PETSC_DECIDE, PETSC_DECIDE, nH*nH, nh*nh, &matI);
	MatCreateSeqAIJ(PETSC_COMM_SELF, nH*nH, nh*nh, 1, NULL, &matI);
//	MatCreate(PETSC_COMM_WORLD, &matI);
//	MatSetType(matI,MATMPIAIJ);
//	MatSetSizes(matI, PETSC_DECIDE, PETSC_DECIDE, nH*nH, nh*nh);
//	MatSetFromOptions(matI);
//	MatSetUp(matI);
	for (int bj=0;bj<nH;bj++) {
		colStart = bj*nH;
		colEnd   = colStart+nH;
		rowStart = (bj*nh)*((m+1)/2);
		for (int bi=0;bi<m;bi++) {
			for (int j=colStart;j<colEnd;j++) {
				rowEnd  = rowStart + m;
				for (int i=rowStart;i<rowEnd;i++) {
					if (Is[bi][i-rowStart]!=0.0) MatSetValue(matI, j, i, Is[bi][i-rowStart], INSERT_VALUES);
				}
				rowStart = rowStart + ((m+1)/2);
			}
			rowStart = rowEnd;
		}
	}
	
	MatAssemblyBegin(matI, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(matI, MAT_FINAL_ASSEMBLY);
	//MatView(matI, PETSC_VIEWER_STDOUT_WORLD);
	return matI;
}

Mat prolongationMatrix(double **Is, int m, int nh, int nH) {
	// Is	- stencil wise grid transfer operator of size m*m
	// nh	- number of unknowns per dimension in fine grid "h"
	// nH	- number of unknowns per dimension in coarse grid "H"
	
	Mat	matI;
	int	rowStart, rowEnd, colStart, colEnd;

	MatCreateSeqAIJ(PETSC_COMM_SELF, nh*nh, nH*nH, 4, NULL, &matI);
//	MatCreate(PETSC_COMM_WORLD, &matI);
//	MatSetSizes(matI, PETSC_DECIDE, PETSC_DECIDE, nh*nh, nH*nH);
//	MatSetFromOptions(matI);
//	MatSetUp(matI);
	for (int bj=0;bj<nH;bj++) {
		colStart = bj*nH;
		colEnd   = colStart+nH;
		rowStart = (bj*nh)*((m+1)/2);
		for (int bi=0;bi<m;bi++) {
			for (int j=colStart;j<colEnd;j++) {
				rowEnd  = rowStart + m;
				for (int i=rowStart;i<rowEnd;i++) {
					if (Is[bi][i-rowStart]!=0.0) MatSetValue(matI, i, j, Is[bi][i-rowStart], INSERT_VALUES);
				}
				rowStart = rowStart + ((m+1)/2);
			}
			rowStart = rowEnd;
		}
	}
	
	MatAssemblyBegin(matI, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(matI, MAT_FINAL_ASSEMBLY);
	//MatView(matI, PETSC_VIEWER_STDOUT_WORLD);
	return matI;
}

Mat restrictionMatrixMPI(double **Is, int m, IsRange rangeh, IsRange rangeH, ArrayInt2d IsResStencil) {
	// Is	- stencil wise grid transfer operator of size m*m
	// lnh	- number of unknowns in the immediate higher level in this process
	//
	// IsResStencil[i][j=0:8]: i-global index at current level; 
	// 			   j-global index of a point in restriction stencil
	
	Mat	matI;
//	int	range[2];
//	int	rank;
	int	*col;

	//MatCreateAIJ(PETSC_COMM_WORLD, PETSC_DECIDE, PETSC_DECIDE, nH*nH, nh*nh, &matI);
	MatCreateAIJ(PETSC_COMM_WORLD, IsResStencil.ni, rangeh.end-rangeh.start, PETSC_DETERMINE, PETSC_DETERMINE, 1, PETSC_NULL, 1, PETSC_NULL, &matI);
	
//	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
//	if (rank==0) {
//	MatGetOwnershipRange(matI, range, range+1);
	for (int i=rangeH.start;i<rangeH.end;i++) {
		col = IsResStencil.data+((i-rangeH.start)*IsResStencil.nj);
		for (int j=0;j<IsResStencil.nj;j++) {
			if (Is[j/3][j%3]!=0.0) MatSetValue(matI, i, col[j], Is[j/3][j%3], INSERT_VALUES);
		}
	}
//	for (int bj=0;bj<nH;bj++) {
//		colStart = bj*nH;
//		colEnd   = colStart+nH;
//		rowStart = (bj*nh)*((m+1)/2);
//		for (int bi=0;bi<m;bi++) {
//			for (int j=colStart;j<colEnd;j++) {
//				rowEnd  = rowStart + m;
//				for (int i=rowStart;i<rowEnd;i++) {
//					if (Is[bi][i-rowStart]!=0.0) MatSetValue(matI, j, i, Is[bi][i-rowStart], INSERT_VALUES);
//				}
//				rowStart = rowStart + ((m+1)/2);
//			}
//			rowStart = rowEnd;
//		}
//	}
	
//	}	
	MatAssemblyBegin(matI, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(matI, MAT_FINAL_ASSEMBLY);
	//MatView(matI, PETSC_VIEWER_STDOUT_WORLD);
	return matI;
}

Mat prolongationMatrixMPI(double **Is, int m, IsRange rangeh, IsRange rangeH, ArrayInt2d IsProStencil) {
	// Is	- stencil wise grid transfer operator of size m*m
	// lnh	- number of unknowns in the immediate higher level in this process
	//
	// IsProStencil[i][j=0:8]: i-global index at current level; 
	// 			   j-global index of a point in prolongation stencil
	
	Mat	matI;
//	int	range[2];
//	int	rank;
	int	*row;

	MatCreateAIJ(PETSC_COMM_WORLD, rangeh.end-rangeh.start, IsProStencil.ni, PETSC_DETERMINE, PETSC_DETERMINE, 4, PETSC_NULL, 4, PETSC_NULL, &matI);
	
//	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
//	if (rank==0) {

//	MatGetOwnershipRange(matI, range, range+1);
	for (int i=rangeH.start;i<rangeH.end;i++) {
		row = IsProStencil.data+((i-rangeH.start)*IsProStencil.nj);
		for (int j=0;j<IsProStencil.nj;j++) {
			if (Is[j/3][j%3]!=0.0) MatSetValue(matI, row[j], i, Is[j/3][j%3], INSERT_VALUES);
		}
	}
//	for (int bj=0;bj<nH;bj++) {
//		colStart = bj*nH;
//		colEnd   = colStart+nH;
//		rowStart = (bj*nh)*((m+1)/2);
//		for (int bi=0;bi<m;bi++) {
//			for (int j=colStart;j<colEnd;j++) {
//				rowEnd  = rowStart + m;
//				for (int i=rowStart;i<rowEnd;i++) {
//					if (Is[bi][i-rowStart]!=0.0) MatSetValue(matI, i, j, Is[bi][i-rowStart], INSERT_VALUES);
//				}
//				rowStart = rowStart + ((m+1)/2);
//			}
//			rowStart = rowEnd;
//		}
//	}
	
//	}	
	MatAssemblyBegin(matI, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(matI, MAT_FINAL_ASSEMBLY);
	//MatView(matI, PETSC_VIEWER_STDOUT_WORLD);
	return matI;
}

void levelvecb(Vec *b, double *f) {
	// Build vector "b" for fine grid
	// f - logically 2D array containing right hand side values at each grid point
	
	int	rowStart, rowEnd;

	VecGetOwnershipRange(*b, &rowStart, &rowEnd);
	for (int i=rowStart;i<rowEnd;i++) {
		VecSetValue(*b, i, f[i-rowStart], INSERT_VALUES);
	}
	VecAssemblyBegin(*b);
	VecAssemblyEnd(*b);

	//return b;
}


void vecb(Vec *b, Array2d f, double **opIh2H, int n0, int levels) {
	// Build vector "b" for the implicit multigrid correction method
	// f		- logically 2D array containing right hand side values at each grid point
	// opIh2H 	- Stencilwise restriction operator
	// n		- Number of unknowns per dimension on finest grid
	// levels	- Number of levels
	
	//Vec	b, subb[levels]; 
	Vec	subb[levels]; 
	Mat	restrictMatrix[levels-1];
	int	r, rowStart, rowEnd, TotalRows;//, i, j;
	int	n[levels];

	int	rank;

	TotalRows = ((n0+1)*(n0+1)*(ipow(4,levels)-1))/(3*ipow(4,levels-1))-(2*(n0+1)*(ipow(2,levels)-1))/(ipow(2,levels-1))+levels;
	
	n[0] = n0;
	for (int l=0;l<levels-1;l++) {
		n[l+1] = (n[l]-1)/2;
		//restrictMatrix[l] = restrictionMatrix(opIh2H, 3, n[l], n[l+1]);
	}	

//	VecCreate(PETSC_COMM_WORLD, &b);
//	VecSetSizes(b, PETSC_DECIDE, TotalRows);
//	VecSetFromOptions(b);
//	VecGetOwnershipRange(b, &rowStart, &rowEnd);

	MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

	if (rank == 0) {

	for (int l=0;l<levels-1;l++) {
		restrictMatrix[l] = restrictionMatrix(opIh2H, 3, n[l], n[l+1]);
	}	
	VecCreate(PETSC_COMM_SELF, &(subb[0]));
	VecSetSizes(subb[0], PETSC_DECIDE, n[0]*n[0]);
	VecSetFromOptions(subb[0]);
	VecGetOwnershipRange(subb[0], &rowStart, &rowEnd);
	r=0;
	for (int i=0;i<f.ni;i++) {
		for (int j=0;j<f.nj;j++) {
			VecSetValue(subb[0], r, F(i,j) , INSERT_VALUES);
			r = r+1;
		}
	}
	VecAssemblyBegin(subb[0]);
	VecAssemblyEnd(subb[0]);
	
	//insertSubVecValues(&(subb[0]), &(b), 0);
	insertSubVecValues(&(subb[0]), b, 0);
	
	r=0;
	for (int l=0;l<levels-1;l++) {
		r = r+n[l]*n[l];
		VecCreate(PETSC_COMM_SELF, &(subb[l+1]));
		VecSetSizes(subb[l+1], PETSC_DECIDE, n[l+1]*n[l+1]);
		VecSetFromOptions(subb[l+1]);
		MatMult(restrictMatrix[l],subb[l],subb[l+1]);
		//insertSubVecValues(&(subb[l+1]), &(b), r);
		insertSubVecValues(&(subb[l+1]), b, r);
		MatDestroy(&(restrictMatrix[l]));
		VecDestroy(&(subb[l]));
	}
	VecDestroy(&(subb[levels-1]));
	
	}

	VecAssemblyBegin(*b);
	VecAssemblyEnd(*b);

	//return b;
}

