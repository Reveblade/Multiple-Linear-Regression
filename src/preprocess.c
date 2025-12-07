#include "preprocess.h"
#include "csv.h"
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
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = c1 - 'A' + 'a';
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = c2 - 'A' + 'a';
        }
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

void* normalize_column(void* arg) {
    int col_index = *(int*)arg;

    double min_val = raw_data[0][col_index];
    double max_val = raw_data[0][col_index];

    for (int row = 1; row < total_rows; row++) {
        double val = raw_data[row][col_index];
        if (val < min_val) {
            min_val = val;
        }
        if (val > max_val) {
            max_val = val;
        }
    }

    xmin[col_index] = min_val;
    xmax[col_index] = max_val;

    double range = max_val - min_val;
    if (range == 0.0) {
        range = 1.0;
    }

    for (int row = 0; row < total_rows; row++) {
        normalized_data[row][col_index] = (raw_data[row][col_index] - min_val) / range;
    }

    return NULL;
}

void* encode_categorical_column(void* arg) {
    int col_index = *(int*)arg;

    int is_furnishingstatus = 0;

    if (strcasecmp_simple(column_names[col_index], "furnishingstatus") == 0) {
        const char* filename_base = strrchr(current_filename, '/');
        if (filename_base == NULL) {
            filename_base = current_filename;
        } else {
            filename_base++;
        }
        if (strcasecmp_simple(filename_base, "Housing.csv") == 0) {
            is_furnishingstatus = 1;
        }
    }

    char thread_msg[BUFFER_SIZE + 50];
    snprintf(thread_msg, BUFFER_SIZE + 50, "[Thread C%d] %s: encoding started\n", col_index, column_names[col_index]);
    printf("%s", thread_msg);
    if (send(client_fd_global, thread_msg, strlen(thread_msg), 0) < 0) {
        perror("send failed");
    }

    for (int row = 0; row < total_rows; row++) {
        if (raw_categorical[row][col_index][0] != '\0') {
            if (is_furnishingstatus) {
                if (strcasecmp_simple(raw_categorical[row][col_index], "furnished") == 0) {
                    encoded_data[row][col_index] = 2.0;
                } else if (strcasecmp_simple(raw_categorical[row][col_index], "semi-furnished") == 0) {
                    encoded_data[row][col_index] = 1.0;
                } else if (strcasecmp_simple(raw_categorical[row][col_index], "unfurnished") == 0) {
                    encoded_data[row][col_index] = 0.0;
                } else {
                    encoded_data[row][col_index] = 0.0;
                }
            } else {
                if (strcasecmp_simple(raw_categorical[row][col_index], "yes") == 0) {
                    encoded_data[row][col_index] = 1.0;
                } else if (strcasecmp_simple(raw_categorical[row][col_index], "no") == 0) {
                    encoded_data[row][col_index] = 0.0;
                } else {
                    encoded_data[row][col_index] = 0.0;
                }
            }
        } else {
            encoded_data[row][col_index] = 0.0;
        }
    }

    return NULL;
}

void build_design_matrix() {
    num_features = 0;
    int feature_col = 0;

    strcpy(feature_names[feature_col], "(bias)");
    feature_to_column[feature_col] = -1;
    feature_col++;

    for (int orig_col = 0; orig_col < total_columns; orig_col++) {
        if (column_types[orig_col] == NUMERIC && orig_col != target_column_index) {
            strncpy(feature_names[feature_col], column_names[orig_col], BUFFER_SIZE - 1);
            feature_names[feature_col][BUFFER_SIZE - 1] = '\0';
            feature_to_column[feature_col] = orig_col;
            feature_col++;
        }
    }

    for (int orig_col = 0; orig_col < total_columns; orig_col++) {
        if (column_types[orig_col] == CATEGORICAL) {
            strncpy(feature_names[feature_col], column_names[orig_col], BUFFER_SIZE - 1);
            feature_names[feature_col][BUFFER_SIZE - 1] = '\0';
            feature_to_column[feature_col] = orig_col;
            feature_col++;
        }
    }

    num_features = feature_col;

    for (int row = 0; row < total_rows; row++) {
        feature_col = 0;

        X_norm[row][feature_col] = 1.0;
        feature_col++;

        for (int orig_col = 0; orig_col < total_columns; orig_col++) {
            if (column_types[orig_col] == NUMERIC && orig_col != target_column_index) {
                X_norm[row][feature_col] = normalized_data[row][orig_col];
                feature_col++;
            }
        }

        for (int orig_col = 0; orig_col < total_columns; orig_col++) {
            if (column_types[orig_col] == CATEGORICAL) {
                X_norm[row][feature_col] = encoded_data[row][orig_col];
                feature_col++;
            }
        }
    }

    for (int row = 0; row < total_rows; row++) {
        y_norm[row] = normalized_data[row][target_column_index];
    }
}
