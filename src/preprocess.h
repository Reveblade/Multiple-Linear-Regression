#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_COLUMNS 100
#define MAX_SAMPLES 10000
#define MAX_FEATURES 100
#define STRING_BUFFER_LIMIT 100

enum column_type {
    NUMERIC,
    CATEGORICAL
};

extern char column_names[MAX_COLUMNS][BUFFER_SIZE];
extern enum column_type column_types[MAX_COLUMNS];
extern int target_column_index;
extern int total_rows;
extern int total_columns;

extern double xmin[MAX_COLUMNS];
extern double xmax[MAX_COLUMNS];
extern double normalized_data[MAX_SAMPLES][MAX_COLUMNS];
extern double raw_data[MAX_SAMPLES][MAX_COLUMNS];
extern double encoded_data[MAX_SAMPLES][MAX_COLUMNS];
extern double X_norm[MAX_SAMPLES][MAX_FEATURES];
extern double y_norm[MAX_SAMPLES];

extern double A[MAX_FEATURES][MAX_FEATURES];
extern double B[MAX_FEATURES];
extern double beta[MAX_FEATURES];

extern int num_features;
extern int feature_to_column[MAX_FEATURES];
extern char feature_names[MAX_FEATURES][BUFFER_SIZE];

extern char raw_categorical[MAX_SAMPLES][MAX_COLUMNS][STRING_BUFFER_LIMIT];

extern char current_filename[BUFFER_SIZE];
extern int client_fd_global;

int strcasecmp_simple(const char* s1, const char* s2);
void* normalize_column(void* arg);
void* encode_categorical_column(void* arg);
void build_design_matrix();

#endif
