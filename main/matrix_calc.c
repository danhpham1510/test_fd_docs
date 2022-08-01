#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include"matrix_calc.h"



/*
* @brief:	Subtract a matrix by a matrix (same size)
* @param:	src	source matrix
* @param:	des	matrix to subtract
* @param:	m	matrix row
* @param:	n	matrix col
*/
double* substract_two_matrixes(double* src, double* des, int m, int n)
{
	double* result = (double*)malloc(m * n * sizeof(double));
	for (int i = 0; i < m; i++) {
		for (int j = 0; j < n; j++) {
			*(result + i * n + j) = *(src + i * n + j) - *(des + i * n + j);
		}
	}
	return result;
}


/*
* @brief:	Subtract a matrix by an array
* @param:	mat	source matrix
* @param:	arr	array to subtract
* @param:	m	matrix row
* @param:	n	matrix col
*/
double* substract_matrix_array(double* mat, double* arr, int m, int n)
{
	double* result = (double*)malloc(m * n * sizeof(double));
	for (int i = 0; i < m; i++) {
		for (int j = 0; j < n; j++) {
			*(result + i * n + j) = *(mat + i * n + j) - *(arr + j);
		}
	}
	return result;
}

/*
* @brief:	Transpose a matrix
* @param:	mat	target matrix
* @param:	m	number of rows
* @param:	n	number of cols
*/
double* transpose_matrix(double* mat, int m, int n) {
	double* result = (double*)malloc(n * m * sizeof(double));
	for (int row = 0; row < m; row++) {
		for (int col = 0; col < n; col++) {
			*(result + col * m + row) = *(mat + row * n + col);
		}
	}
	return result;
}

double* dot_two_matrixes(double* src, double* des, int msrc, int nsrc, int mdes, int ndes)
{
	if (nsrc != mdes) {
		printf("Cannot dot matix with shape (%d, %d) with (%d, %d)\n", msrc, nsrc, mdes, ndes);
		return NULL;
	}
	double* result = (double*)malloc(msrc * ndes * sizeof(double));
	for (int src_row = 0; src_row < msrc; src_row++) {
		for (int des_col = 0; des_col < ndes; des_col++) {
			*(result + src_row * ndes + des_col) = 0;
			for (int idx = 0; idx < nsrc; idx++) {
				*(result + src_row * ndes + des_col) += *(src + src_row * nsrc + idx) * *(des + idx * ndes + des_col);
			}
		}
	}
	return result;
}


void print_matrix(double* matrix, int m, int n)
{
	for (int i = 0; i < m; i++) {
		for (int j = 0; j < n; j++) {
			printf("%lf ", *(matrix + i * n + j));
		}
		printf("\n");
	}
}



/* ================================================	Interact elementwise with array	================================================*/
void exp_array(double* arr, int n) 
{
	for (int i = 0; i < n; i++) {
		*(arr + i) = exp(*(arr + i));
	}
}

void mul_array(double* arr, int n, double t) 
{
	for (int i = 0; i < n; i++) {
		*(arr + i) = *(arr + i) * t;
	}
}


void pow_array(double* arr, int n, double pow_value) 
{
	for (int i = 0; i < n; i++) {
		*(arr + i) = pow(*(arr + i), pow_value);
	}
}