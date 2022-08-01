
struct svm_params
{
	int num_sv;					// Number of support vectors
	int num_features;			// Number of features when trainning
	double* support_vectors;	// Matrix shape (num_sv, num_features)
	double* dual_coefs;			// Matrix shape (1, num_sv)
	double gamma;
	double intercept;
};

void int_svm_params(struct svm_params* params);
double frobenius_norm(double* arr, int m);
double decision_function(struct svm_params* params, double* X_test, double* support_vectors);
double* substract_matrix_array(double* mat, double* arr, int m, int n);
int predict(struct frame_struct* q_frame, uint8_t len);