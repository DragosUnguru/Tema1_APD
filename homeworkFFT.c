#include <stdio.h>
#include <unistd.h>
#include <complex.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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

double complex* data;
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

	data = (double complex*) malloc(n * sizeof(double complex));
	freqs = (double complex*) malloc(n * sizeof(double complex));
	DIE(data < 0 || freqs < 0, "Malloc failed!");

	// Read input data
	for (i = 0; i < n; ++i) {
		double tmp;
		DIE(fscanf(fin_ptr, "%lf", &tmp) < 0, "Error reading data from file.");
		data[i] = CMPLX(tmp, 0);
	}

	fclose(fin_ptr);
}

void flush_output(char* file_name, double complex* out_data) {
	FILE* fout_ptr;

	// Open file for writing
	fout_ptr = fopen(file_name, "w");
	DIE(fout_ptr < 0, "Error opening output file.");

	// Write output
	DIE(fprintf(fout_ptr, "%d\n", n) < 0, "Error writing output");
	for (int i = 0; i < n; ++i) {
		DIE(fprintf(fout_ptr, "%lf %lf\n",
		creal(out_data[i]), cimag(out_data[i])) < 0, "Error writing output.");
	}

	fclose(fout_ptr);
}

void compute_missing_call(double complex* in, int step, int offset) {
	double complex* out = (in == data) ? freqs : data;
	
	for (int i = 0; i < n; i += 2 * step) {
		double complex t = cexp(-I * PI * i / n) * out[i + step + offset];
		in[i / 2 + offset]     = out[i + offset] + t;
		in[(i + n) / 2 + offset] = out[i + offset] - t;
	}
}

void _fft(double complex* in, double complex* out, int step)
{
	if (step < n) {
		_fft(out, in, step * 2);
		_fft(out + step, in + step, step * 2);
 
		for (int i = 0; i < n; i += 2 * step) {
			double complex t = cexp(-I * PI * i / n) * out[i + step];
			in[i / 2]     = out[i] + t;
			in[(i + n)/2] = out[i] - t;
		}
	}
}

 
void* fft(void* args) {
	int thread_id = *(int*) args;

	/*
		For 2 threads, we start with 1 recursive call ahead,
		so we switch pointers
	*/
	double complex* in = (P == 2) ? freqs : data;
	double complex* out = (P == 2) ? data : freqs;

	_fft(in + thread_id, out + thread_id, P);
 
 	return NULL;
}

void execute(int argc, char * argv[]) {
	P = atoi(argv[3]);

	pthread_t tid[P];
	int thread_id[P];
	int i;

	parse_input(argc, argv);

	// Parallelize
	for (i = 0; i < P; ++i) {
		thread_id[i] = i;
		pthread_create(tid + i, NULL, fft, thread_id + i);
	}
	for(i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
	}

	// Compute for skipped recursive calls
	if (P == 2) {
		compute_missing_call(data, 1, 0);
	}
	else if (P == 4) {
		compute_missing_call(freqs, 2, 0);
		compute_missing_call(freqs, 2, 1);
		compute_missing_call(data, 1, 0);
	}

	// Print output into file
	flush_output(argv[2], data);
	
	// Free memory
	free(data);
	free(freqs);
}

int main(int argc, char * argv[]) {
	execute(argc, argv);
	
	return 0;
}
