#include "preprocess.h"
#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <sys/socket.h>

char column_names[MAX_COLUMNS][BUFFER_SIZE];
enum column_type column_types[MAX_COLUMNS];
int target_column_index = -1;
int total_rows = 0;
int total_columns = 0;

double xmin[MAX_COLUMNS];
double xmax[MAX_COLUMNS];
double normalized_data[MAX_SAMPLES][MAX_COLUMNS];
double raw_data[MAX_SAMPLES][MAX_COLUMNS];
double encoded_data[MAX_SAMPLES][MAX_COLUMNS];

double X_norm[MAX_SAMPLES][MAX_FEATURES];
double y_norm[MAX_SAMPLES];

double A[MAX_FEATURES][MAX_FEATURES];
double B[MAX_FEATURES];
double beta[MAX_FEATURES];

int num_features = 0;
int feature_to_column[MAX_FEATURES];
char feature_names[MAX_FEATURES][BUFFER_SIZE];

char raw_categorical[MAX_SAMPLES][MAX_COLUMNS][STRING_BUFFER_LIMIT];

char current_filename[BUFFER_SIZE];
int client_fd_global;

int strcasecmp_simple(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

void* normalize_column(void* arg) {
    int col = *(int*)arg;

    double minv = raw_data[0][col];
    double maxv = raw_data[0][col];

    for (int i = 1; i < total_rows; i++) {
        if (raw_data[i][col] < minv) minv = raw_data[i][col];
        if (raw_data[i][col] > maxv) maxv = raw_data[i][col];
    }

    xmin[col] = minv;
    xmax[col] = maxv;

    double range = maxv - minv;
    if (range == 0.0) range = 1.0;

    for (int i = 0; i < total_rows; i++) {
        normalized_data[i][col] = (raw_data[i][col] - minv) / range;
    }

    return NULL;
}

void* encode_categorical_column(void* arg) {
    int col = *(int*)arg;

    int is_furnish = 0;
    if (strcasecmp_simple(column_names[col], "furnishingstatus") == 0 &&
        strstr(current_filename, "Housing.csv")) {
        is_furnish = 1;
    }

    for (int i = 0; i < total_rows; i++) {
        if (raw_categorical[i][col][0] == '\0') {
            encoded_data[i][col] = 0.0;
        }
        else if (is_furnish) {
            if (strcasecmp_simple(raw_categorical[i][col], "furnished") == 0)
                encoded_data[i][col] = 2.0;
            else if (strcasecmp_simple(raw_categorical[i][col], "semi-furnished") == 0)
                encoded_data[i][col] = 1.0;
            else
                encoded_data[i][col] = 0.0;
        }
        else {
            if (strcasecmp_simple(raw_categorical[i][col], "yes") == 0)
                encoded_data[i][col] = 1.0;
            else
                encoded_data[i][col] = 0.0;
        }
    }
    return NULL;
}

void build_design_matrix() {
    num_features = 0;
    int fc = 0;

    strcpy(feature_names[fc], "(bias)");
    feature_to_column[fc] = -1;
    fc++;

    for (int c = 0; c < total_columns; c++) {
        if (column_types[c] == NUMERIC && c != target_column_index) {
            strcpy(feature_names[fc], column_names[c]);
            feature_to_column[fc] = c;
            fc++;
        }
    }

    for (int c = 0; c < total_columns; c++) {
        if (column_types[c] == CATEGORICAL && c != target_column_index) {
            strcpy(feature_names[fc], column_names[c]);
            feature_to_column[fc] = c;
            fc++;
        }
    }

    num_features = fc;

    for (int i = 0; i < total_rows; i++) {
        fc = 0;
        X_norm[i][fc++] = 1.0;

        for (int c = 0; c < total_columns; c++) {
            if (column_types[c] == NUMERIC && c != target_column_index) {
                X_norm[i][fc++] = normalized_data[i][c];
            }
        }

        for (int c = 0; c < total_columns; c++) {
            if (column_types[c] == CATEGORICAL && c != target_column_index) {
                X_norm[i][fc++] = encoded_data[i][c];
            }
        }
    }

    for (int i = 0; i < total_rows; i++) {
        y_norm[i] = normalized_data[i][target_column_index];
    }
}
