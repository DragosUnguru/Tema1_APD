#include <stdio.h>
#include <unistd.h>
#include <complex.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>

/*
 * Error checking macro
 * Eg:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define PI 3.14159265358979323846
#define MIN(x, y) ((x < y) ? x : y)

double* data;
double complex* freqs;
int n, P;

void parse_input(int argc, char* argv[]) {
	int i;
	FILE* fin_ptr;

	// Check input args
	if (argc != 4) {
		printf("Run as: %s <input_file> <output_file> <no_of_threads>\n", argv[0]);
		exit(1);
	}

	// Open file and manage size of input/output data array
	fin_ptr = fopen(argv[1], "r");
	DIE(fin_ptr < 0, "Error opening input file.");

	DIE(fscanf(fin_ptr, "%d", &n) < 0, "Error reading from input file.");

	data = (double*) malloc(n * sizeof(double));
	freqs = (double complex*) malloc(n * sizeof(double complex));
	DIE(data < 0 || freqs < 0, "Malloc failed!");

	// Read input data
	for (i = 0; i < n; ++i) {
		DIE(fscanf(fin_ptr, "%lf", (data + i)) < 0, "Error reading data from file.");
	}

	fclose(fin_ptr);
}

void flush_output(char* file_name) {
	FILE* fout_ptr;

	// Open file for writing
	fout_ptr = fopen(file_name, "w");
	DIE(fout_ptr < 0, "Error opening output file.");

	// Write output
	DIE(fprintf(fout_ptr, "%d\n", n) < 0, "Error writing output");
	for (int i = 0; i < n; ++i) {
		DIE(fprintf(fout_ptr, "%lf %lf\n",
		creal(freqs[i]), cimag(freqs[i])) < 0, "Error writing output.");
	}

	fclose(fout_ptr);
}

void fft_sequential() {
	int i, j;

	for (i = 0; i < n; ++i) {
		// Initialize array on the go
		freqs[i] = 0;

		// Compute frequencies using Discrete Fourier Transform
		for (j = 0; j < n; ++j) {
			freqs[i] += data[j] * cexp(-2.0 * PI * I * i * j / n);
		}
	}
}

void* fft_parallel(void* args) {
	const int SEQ = ceil((double) n / (double) P);
	int thread_id = *(int*) args;
	int start = SEQ * thread_id;
	int end = MIN(n, SEQ * (thread_id + 1));
	int i, j;

	for (i = start; i < end; ++i) {
		// Initialize array on the go
		freqs[i] = 0;

		// Compute frequencies using Discrete Fourier Transform
		for (j = 0; j < n; ++j) {
			freqs[i] += data[j] * cexp(-2.0 * PI * I * i * j / n);
		}
	}
	
	return freqs;
}

void execute(int argc, char * argv[]) {
	P = atoi(argv[3]);

	pthread_t tid[P];
	int thread_id[P];
	int i;

	parse_input(argc, argv);

	for (i = 0; i < P; ++i) {
		thread_id[i] = i;
		pthread_create(tid + i, NULL, fft_parallel, thread_id + i);
	}
	for(i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
	}

	flush_output(argv[2]);
	
	// Free memory
	free(data);
	free(freqs);
}

int main(int argc, char * argv[]) {
	execute(argc, argv);
	
	return 0;
}
