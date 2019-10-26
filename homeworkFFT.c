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

typedef struct _args {
	double complex* in_buf;
	double complex* out_buf;
	int step;
} args_t;

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

// void* _fft(void* args) {
// 	args_t params = *(args_t*) args;
// 	args_t reversed_parms;
// 	args_t offset_params;

// 	if (params.step < n) {
// 		reversed_parms.in_buf = params.out_buf;
// 		reversed_parms.out_buf = params.in_buf;
// 		reversed_parms.step = params.step << 1;
// 		_fft(&reversed_parms);

// 		offset_params.in_buf = params.out_buf + params.step;
// 		offset_params.out_buf = params.in_buf + params.step;
// 		offset_params.step = params.step << 1;
// 		_fft(&offset_params);
 
// 		for (int i = 0; i < n; i += 2 * params.step) {
// 			double complex t = cexp(-I * PI * i / n) * params.out_buf[i + params.step];
// 			params.in_buf[i / 2]     = params.out_buf[i] + t;
// 			params.in_buf[(i + n)/2] = params.out_buf[i] - t;
// 		}
// 	}

// 	return NULL;
// }
 
void* fft(void* args) {
	int i, step;
	int thread_id = *(int*) args;
 
	switch (P) {
	case 1:
		_fft(data, freqs, 1);
		break;
	case 2:
		if (thread_id == 0) {
			_fft(freqs, data, 2);
		} else {
			_fft(freqs + 1, data + 1, 2);
		}
		break;
	case 4:
		if (thread_id == 0) {
			_fft(data, freqs, 4);
		}
		else if (thread_id == 1) {
			_fft(data + 2, freqs + 2, 4);
		}
		else if (thread_id == 2) {
			_fft(data + 1, freqs + 1, 4);
		}
		else if (thread_id == 3) {
			_fft(data + 3, freqs + 3, 4);
		}
	default:
		break;
	}

	return NULL;
}

void execute(int argc, char * argv[]) {
	P = atoi(argv[3]);

	// double complex* save_in;
	// double complex* save_out;
	pthread_t tid[P];
	int thread_id[P];
	// args_t thread_args;
	int i;

	parse_input(argc, argv);

	// Manage args for threaded function
	// thread_args.in_buf = (double complex*) malloc (n * sizeof(double complex));
	// thread_args.out_buf = (double complex*) malloc (n * sizeof(double complex));
	// thread_args.step = 1;
	// for (i = 0; i < n; i++) {
	// 	thread_args.in_buf[i] = thread_args.out_buf[i] = data[i];
	// }

	// save_in = thread_args.in_buf;
	// save_out = thread_args.out_buf;

	for (i = 0; i < P; ++i) {
		thread_id[i] = i;
		pthread_create(tid + i, NULL, fft, thread_id + i);

		// Swap pointers, manage step and move starting pointer;
		// double complex* tmp = thread_args.in_buf + thread_args.step;
		// thread_args.in_buf = thread_args.out_buf + thread_args.step;
		// thread_args.out_buf = tmp;
		// thread_args.step <<= 1;
	}
	for(i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
	}

	if (P == 2) {
		// Merge
		for (int i = 0; i < n; i += 2) {
			double complex t = cexp(-I * PI * i / n) * freqs[i + 1];
			data[i / 2]     = freqs[i] + t;
			data[(i + n)/2] = freqs[i] - t;
		}
	}
	else if (P == 4) {
		// Merge
		for (int i = 0; i < n; i += 4) {
			double complex t = cexp(-I * PI * i / n) * data[i + 2];
			freqs[i / 2]     = data[i] + t;
			freqs[(i + n)/2] = data[i] - t;
		}
		for (int i = 0; i < n; i += 4) {
			double complex t = cexp(-I * PI * i / n) * data[i + 1 + 2];
			freqs[i / 2 + 1]     = data[i + 1] + t;
			freqs[(i + n) / 2 + 1] = data[i + 1] - t;
		}
		
		for (int i = 0; i < n; i += 2) {
			double complex t = cexp(-I * PI * i / n) * freqs[i + 1];
			data[i / 2]     = freqs[i] + t;
			data[(i + n)/2] = freqs[i] - t;
		}
	}

	flush_output(argv[2], data);
	
	// Free memory
	free(data);
	free(freqs);
	// free(save_in);
	// free(save_out);
}

int main(int argc, char * argv[]) {
	execute(argc, argv);
	
	return 0;
}
