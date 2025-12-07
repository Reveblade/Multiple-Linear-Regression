#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_NUMBER 60000
#define BUFFER_SIZE 1024
#define MAX_COLUMNS 100
#define MAX_SAMPLES 10000
#define MAX_FEATURES 100
#define STRING_BUFFER_LIMIT 100

enum column_type {
    NUMERIC,
    CATEGORICAL
};

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

int is_numeric(const char* str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    char *endptr;
    strtod(str, &endptr);
    while (*endptr == ' ' || *endptr == '\t' || *endptr == '\n' || *endptr == '\r') {
        endptr++;
    }
    return *endptr == '\0';
}

int contains_keyword(const char* str, const char* keyword) {
    int len = strlen(str);
    int keylen = strlen(keyword);
    for (int i = 0; i <= len - keylen; i++) {
        int match = 1;
        for (int j = 0; j < keylen; j++) {
            char c1 = str[i + j];
            char c2 = keyword[j];
            if (c1 >= 'A' && c1 <= 'Z') {
                c1 = c1 - 'A' + 'a';
            }
            if (c2 >= 'A' && c2 <= 'Z') {
                c2 = c2 - 'A' + 'a';
            }
            if (c1 != c2) {
                match = 0;
                break;
            }
        }
        if (match) {
            return 1;
        }
    }
    return 0;
}

void parse_csv_line(char* line, char* columns[], int* col_count) {
    *col_count = 0;
    char* token = strtok(line, ",");
    while (token != NULL && *col_count < MAX_COLUMNS) {
        while (*token == ' ' || *token == '\t') {
            token++;
        }
        int len = strlen(token);
        while (len > 0 && (token[len-1] == ' ' || token[len-1] == '\t' || token[len-1] == '\n' || token[len-1] == '\r')) {
            token[len-1] = '\0';
            len--;
        }
        columns[*col_count] = token;
        (*col_count)++;
        token = strtok(NULL, ",");
    }
}

int load_csv_data(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    char line[BUFFER_SIZE];
    int row = 0;
    int first_line = 1;

    while (fgets(line, BUFFER_SIZE, file) != NULL && row < MAX_SAMPLES) {
        if (first_line) {
            first_line = 0;
            continue;
        }

        char* cols[MAX_COLUMNS];
        char line_copy[BUFFER_SIZE];
        strcpy(line_copy, line);
        int col_count = 0;
        parse_csv_line(line_copy, cols, &col_count);

        for (int i = 0; i < col_count && i < total_columns; i++) {
            if (cols[i] != NULL) {
                if (column_types[i] == NUMERIC) {
                    raw_data[row][i] = strtod(cols[i], NULL);
                } else {
                    strncpy(raw_categorical[row][i], cols[i], STRING_BUFFER_LIMIT - 1);
                    raw_categorical[row][i][STRING_BUFFER_LIMIT - 1] = '\0';
                    int len = strlen(raw_categorical[row][i]);
                    while (len > 0 && (raw_categorical[row][i][len-1] == ' ' || raw_categorical[row][i][len-1] == '\t' || raw_categorical[row][i][len-1] == '\n' || raw_categorical[row][i][len-1] == '\r')) {
                        raw_categorical[row][i][len-1] = '\0';
                        len--;
                    }
                }
            } else {
                if (column_types[i] == NUMERIC) {
                    raw_data[row][i] = 0.0;
                } else {
                    raw_categorical[row][i][0] = '\0';
                }
            }
        }
        row++;
    }

    fclose(file);
    return 0;
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

void* compute_coefficient(void* arg) {
    int j = *(int*)arg;

    char thread_msg[BUFFER_SIZE + 50];
    snprintf(thread_msg, BUFFER_SIZE + 50, "[β-Thread %d] Calculating β%d\n", j, j);
    printf("%s", thread_msg);
    if (send(client_fd_global, thread_msg, strlen(thread_msg), 0) < 0) {
        perror("send failed");
    }

    for (int k = 0; k < num_features; k++) {
        double sum = 0.0;
        for (int i = 0; i < total_rows; i++) {
            sum += X_norm[i][j] * X_norm[i][k];
        }
        A[j][k] = sum;
    }

    double sum = 0.0;
    for (int i = 0; i < total_rows; i++) {
        sum += X_norm[i][j] * y_norm[i];
    }
    B[j] = sum;

    return NULL;
}

int gaussian_elimination() {
    int n = num_features;

    for (int i = 0; i < n; i++) {
        int max_row = i;
        for (int k = i + 1; k < n; k++) {
            if (fabs(A[k][i]) > fabs(A[max_row][i])) {
                max_row = k;
            }
        }

        for (int k = i; k < n; k++) {
            double temp = A[max_row][k];
            A[max_row][k] = A[i][k];
            A[i][k] = temp;
        }
        double temp = B[max_row];
        B[max_row] = B[i];
        B[i] = temp;

        if (A[i][i] == 0.0) {
            return -1;
        }

        for (int k = i + 1; k < n; k++) {
            double factor = A[k][i] / A[i][i];
            for (int j = i; j < n; j++) {
                A[k][j] -= factor * A[i][j];
            }
            B[k] -= factor * B[i];
        }
    }

    for (int i = n - 1; i >= 0; i--) {
        beta[i] = B[i];
        for (int j = i + 1; j < n; j++) {
            beta[i] -= A[i][j] * beta[j];
        }
        beta[i] /= A[i][i];
    }

    return 0;
}

int analyze_csv(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    char line[BUFFER_SIZE];
    int column_count = 0;
    int row_count = 0;
    int first_line = 1;
    char header_line[BUFFER_SIZE];
    char first_data_line[BUFFER_SIZE];

    while (fgets(line, BUFFER_SIZE, file) != NULL) {
        if (first_line) {
            strcpy(header_line, line);
            int len = strlen(line);
            for (int i = 0; i < len; i++) {
                if (line[i] == ',') {
                    column_count++;
                }
            }
            column_count++;
            first_line = 0;
        } else {
            if (row_count == 0) {
                strcpy(first_data_line, line);
            }
            row_count++;
        }
    }

    fclose(file);

    total_rows = row_count;
    total_columns = column_count;
    target_column_index = -1;

    if (row_count == 0) {
        return 0;
    }

    char* header_cols[MAX_COLUMNS];
    char header_copy[BUFFER_SIZE];
    strcpy(header_copy, header_line);
    int header_col_count = 0;
    parse_csv_line(header_copy, header_cols, &header_col_count);

    char* data_cols[MAX_COLUMNS];
    char data_copy[BUFFER_SIZE];
    strcpy(data_copy, first_data_line);
    int data_col_count = 0;
    parse_csv_line(data_copy, data_cols, &data_col_count);

    for (int i = 0; i < column_count && i < header_col_count; i++) {
        if (header_cols[i] != NULL) {
            strncpy(column_names[i], header_cols[i], BUFFER_SIZE - 1);
            column_names[i][BUFFER_SIZE - 1] = '\0';
        } else {
            snprintf(column_names[i], BUFFER_SIZE, "column_%d", i);
        }

        if (i < data_col_count && data_cols[i] != NULL) {
            column_types[i] = is_numeric(data_cols[i]) ? NUMERIC : CATEGORICAL;
        } else {
            column_types[i] = CATEGORICAL;
        }

        if (header_cols[i] != NULL) {
            if (contains_keyword(header_cols[i], "price") ||
                contains_keyword(header_cols[i], "target") ||
                contains_keyword(header_cols[i], "label")) {
                target_column_index = i;
            }
        }
    }

    return 0;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT_NUMBER);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        if (send(client_fd, "WELCOME TO PRICE PREDICTION SERVER\r\n", 37, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        if (send(client_fd, "Enter CSV file name to load:\r\n", 32, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0) {
            perror("recv failed");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0';
        }
        char *carriage = strchr(buffer, '\r');
        if (carriage) {
            *carriage = '\0';
        }

        printf("%s\n", buffer);

        strncpy(current_filename, buffer, BUFFER_SIZE - 1);
        current_filename[BUFFER_SIZE - 1] = '\0';
        client_fd_global = client_fd;

        char response[BUFFER_SIZE + 20];
        snprintf(response, BUFFER_SIZE + 20, "You entered: %s\n", buffer);
        if (send(client_fd, response, strlen(response), 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        int result = analyze_csv(buffer);

        if (result < 0) {
            char error_msg[BUFFER_SIZE + 50];
            snprintf(error_msg, BUFFER_SIZE + 50, "ERROR: Dataset file \"%s\" not found!\n", buffer);
            printf("%s", error_msg);
            if (send(client_fd, error_msg, strlen(error_msg), 0) < 0) {
                perror("send failed");
            }
        } else {
            char msg[BUFFER_SIZE];
            snprintf(msg, BUFFER_SIZE, "[OK] File \"%.900s\" found.\nReading file...\n", buffer);
            printf("%s", msg);
            if (send(client_fd, msg, strlen(msg), 0) < 0) {
                perror("send failed");
            }

            snprintf(msg, BUFFER_SIZE, "%d rows loaded.\n%d columns detected.\n", total_rows, total_columns);
            printf("%s", msg);
            if (send(client_fd, msg, strlen(msg), 0) < 0) {
                perror("send failed");
            }

            if (send(client_fd, "Column analysis:\n", 17, 0) < 0) {
                perror("send failed");
            }
            printf("Column analysis:\n");

            for (int i = 0; i < total_columns; i++) {
                char type_str[20];
                if (column_types[i] == NUMERIC) {
                    strcpy(type_str, "numeric");
                } else {
                    strcpy(type_str, "categorical");
                }

                char report_line[BUFFER_SIZE + 50];
                if (i == target_column_index) {
                    snprintf(report_line, BUFFER_SIZE + 50, "%s : %s (TARGET)\n", column_names[i], type_str);
                } else {
                    snprintf(report_line, BUFFER_SIZE + 50, "%s : %s\n", column_names[i], type_str);
                }

                printf("%s", report_line);
                if (send(client_fd, report_line, strlen(report_line), 0) < 0) {
                    perror("send failed");
                }
            }

            if (load_csv_data(buffer) == 0) {
                pthread_t threads[MAX_COLUMNS];
                int thread_args[MAX_COLUMNS];
                int thread_count = 0;

                for (int i = 0; i < total_columns; i++) {
                    if (column_types[i] == NUMERIC) {
                        thread_args[thread_count] = i;
                        if (pthread_create(&threads[thread_count], NULL, normalize_column, &thread_args[thread_count]) != 0) {
                            perror("pthread_create failed");
                        } else {
                            thread_count++;
                        }
                    }
                }

                for (int i = 0; i < thread_count; i++) {
                    pthread_join(threads[i], NULL);
                }

                char norm_msg[BUFFER_SIZE];
                snprintf(norm_msg, BUFFER_SIZE, "Normalization completed using %d threads.\n", thread_count);
                printf("%s", norm_msg);
                if (send(client_fd, norm_msg, strlen(norm_msg), 0) < 0) {
                    perror("send failed");
                }

                for (int i = 0; i < total_columns; i++) {
                    if (column_types[i] == NUMERIC) {
                        char col_info[BUFFER_SIZE + 50];
                        snprintf(col_info, BUFFER_SIZE + 50, "%s : min=%.0f, max=%.0f\n", column_names[i], xmin[i], xmax[i]);
                        printf("%s", col_info);
                        if (send(client_fd, col_info, strlen(col_info), 0) < 0) {
                            perror("send failed");
                        }
                    }
                }

                int cat_count = 0;
                for (int i = 0; i < total_columns; i++) {
                    if (column_types[i] == CATEGORICAL) {
                        cat_count++;
                    }
                }

                if (cat_count > 0) {
                    char enc_msg[BUFFER_SIZE];
                    snprintf(enc_msg, BUFFER_SIZE, "Starting attribute-level categorical encoding...\n");
                    printf("%s", enc_msg);
                    if (send(client_fd, enc_msg, strlen(enc_msg), 0) < 0) {
                        perror("send failed");
                    }

                    pthread_t cat_threads[MAX_COLUMNS];
                    int cat_thread_args[MAX_COLUMNS];
                    int cat_thread_count = 0;

                    for (int i = 0; i < total_columns; i++) {
                        if (column_types[i] == CATEGORICAL) {
                            cat_thread_args[cat_thread_count] = i;
                            if (pthread_create(&cat_threads[cat_thread_count], NULL, encode_categorical_column, &cat_thread_args[cat_thread_count]) != 0) {
                                perror("pthread_create failed");
                            } else {
                                cat_thread_count++;
                            }
                        }
                    }

                    for (int i = 0; i < cat_thread_count; i++) {
                        pthread_join(cat_threads[i], NULL);
                    }

                    char complete_msg[BUFFER_SIZE];
                    snprintf(complete_msg, BUFFER_SIZE, "[OK] All categorical encoding threads completed.\n");
                    printf("%s", complete_msg);
                    if (send(client_fd, complete_msg, strlen(complete_msg), 0) < 0) {
                        perror("send failed");
                    }
                }

                char x_msg[BUFFER_SIZE];
                snprintf(x_msg, BUFFER_SIZE, "Building normalized feature matrix X_norm...\n");
                printf("%s", x_msg);
                if (send(client_fd, x_msg, strlen(x_msg), 0) < 0) {
                    perror("send failed");
                }

                build_design_matrix();

                char y_msg[BUFFER_SIZE];
                snprintf(y_msg, BUFFER_SIZE, "Building normalized target vector y_norm...\n");
                printf("%s", y_msg);
                if (send(client_fd, y_msg, strlen(y_msg), 0) < 0) {
                    perror("send failed");
                }

                char design_complete[BUFFER_SIZE];
                snprintf(design_complete, BUFFER_SIZE, "[OK] Design matrix construction completed.\n");
                printf("%s", design_complete);
                if (send(client_fd, design_complete, strlen(design_complete), 0) < 0) {
                    perror("send failed");
                }

                char spawn_msg[BUFFER_SIZE];
                snprintf(spawn_msg, BUFFER_SIZE, "Spawning coefficient calculation threads...\n");
                printf("%s", spawn_msg);
                if (send(client_fd, spawn_msg, strlen(spawn_msg), 0) < 0) {
                    perror("send failed");
                }

                pthread_t beta_threads[MAX_FEATURES];
                int beta_thread_args[MAX_FEATURES];

                for (int j = 0; j < num_features; j++) {
                    beta_thread_args[j] = j;
                    if (pthread_create(&beta_threads[j], NULL, compute_coefficient, &beta_thread_args[j]) != 0) {
                        perror("pthread_create failed");
                    }
                }

                for (int j = 0; j < num_features; j++) {
                    pthread_join(beta_threads[j], NULL);
                }

                char join_msg[BUFFER_SIZE];
                snprintf(join_msg, BUFFER_SIZE, "All coefficient threads joined.\n");
                printf("%s", join_msg);
                if (send(client_fd, join_msg, strlen(join_msg), 0) < 0) {
                    perror("send failed");
                }

                char solve_msg[BUFFER_SIZE];
                snprintf(solve_msg, BUFFER_SIZE, "Solving (XᵀX)β = Xᵀy ...\n");
                printf("%s", solve_msg);
                if (send(client_fd, solve_msg, strlen(solve_msg), 0) < 0) {
                    perror("send failed");
                }

                if (gaussian_elimination() == 0) {
                    char train_complete[BUFFER_SIZE];
                    snprintf(train_complete, BUFFER_SIZE, "Training completed.\n");
                    printf("%s", train_complete);
                    if (send(client_fd, train_complete, strlen(train_complete), 0) < 0) {
                        perror("send failed");
                    }

                    char eq_line[BUFFER_SIZE + 200];
                    snprintf(eq_line, BUFFER_SIZE + 200, "%s_norm =\n", column_names[target_column_index]);
                    printf("%s", eq_line);
                    if (send(client_fd, eq_line, strlen(eq_line), 0) < 0) {
                        perror("send failed");
                    }

                    char beta0_line[BUFFER_SIZE + 200];
                    snprintf(beta0_line, BUFFER_SIZE + 200, "  %.6f\n", beta[0]);
                    printf("%s", beta0_line);
                    if (send(client_fd, beta0_line, strlen(beta0_line), 0) < 0) {
                        perror("send failed");
                    }

                    for (int j = 1; j < num_features; j++) {
                        char beta_line[BUFFER_SIZE + 200];
                        if (beta[j] >= 0) {
                            snprintf(beta_line, BUFFER_SIZE + 200, "+ %.6f * %s_norm\n", beta[j], feature_names[j]);
                        } else {
                            snprintf(beta_line, BUFFER_SIZE + 200, "- %.6f * %s_norm\n", -beta[j], feature_names[j]);
                        }
                        printf("%s", beta_line);
                        if (send(client_fd, beta_line, strlen(beta_line), 0) < 0) {
                            perror("send failed");
                        }
                    }

                    char pred_prompt[BUFFER_SIZE];
                    snprintf(pred_prompt, BUFFER_SIZE, "Enter values for prediction:\n");
                    printf("%s", pred_prompt);
                    if (send(client_fd, pred_prompt, strlen(pred_prompt), 0) < 0) {
                        perror("send failed");
                    }

                    double X_input_norm[MAX_FEATURES];
                    X_input_norm[0] = 1.0;

                    for (int j = 1; j < num_features; j++) {
                        int orig_col = feature_to_column[j];
                        char input_prompt[BUFFER_SIZE + 50];
                        snprintf(input_prompt, BUFFER_SIZE + 50, "Enter %s:\n", feature_names[j]);
                        printf("%s", input_prompt);
                        if (send(client_fd, input_prompt, strlen(input_prompt), 0) < 0) {
                            perror("send failed");
                        }

                        char input_buffer[BUFFER_SIZE];
                        memset(input_buffer, 0, BUFFER_SIZE);
                        int bytes_received = recv(client_fd, input_buffer, BUFFER_SIZE - 1, 0);
                        if (bytes_received < 0) {
                            perror("recv failed");
                            break;
                        }

                        input_buffer[bytes_received] = '\0';
                        char *newline = strchr(input_buffer, '\n');
                        if (newline) {
                            *newline = '\0';
                        }
                        char *carriage = strchr(input_buffer, '\r');
                        if (carriage) {
                            *carriage = '\0';
                        }

                        if (column_types[orig_col] == NUMERIC) {
                            double raw_value = strtod(input_buffer, NULL);
                            double range = xmax[orig_col] - xmin[orig_col];
                            if (range == 0.0) {
                                range = 1.0;
                            }
                            X_input_norm[j] = (raw_value - xmin[orig_col]) / range;
                        } else {
                            int len = strlen(input_buffer);
                            while (len > 0 && (input_buffer[len-1] == ' ' || input_buffer[len-1] == '\t')) {
                                input_buffer[len-1] = '\0';
                                len--;
                            }

                            int is_furnishingstatus = 0;
                            if (strcasecmp_simple(column_names[orig_col], "furnishingstatus") == 0) {
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

                            if (is_furnishingstatus) {
                                if (strcasecmp_simple(input_buffer, "furnished") == 0) {
                                    X_input_norm[j] = 2.0;
                                } else if (strcasecmp_simple(input_buffer, "semi-furnished") == 0) {
                                    X_input_norm[j] = 1.0;
                                } else if (strcasecmp_simple(input_buffer, "unfurnished") == 0) {
                                    X_input_norm[j] = 0.0;
                                } else {
                                    X_input_norm[j] = 0.0;
                                }
                            } else {
                                if (strcasecmp_simple(input_buffer, "yes") == 0) {
                                    X_input_norm[j] = 1.0;
                                } else if (strcasecmp_simple(input_buffer, "no") == 0) {
                                    X_input_norm[j] = 0.0;
                                } else {
                                    X_input_norm[j] = 0.0;
                                }
                            }
                        }
                    }

                    double y_pred_norm = 0.0;
                    for (int j = 0; j < num_features; j++) {
                        y_pred_norm += beta[j] * X_input_norm[j];
                    }

                    double range_target = xmax[target_column_index] - xmin[target_column_index];
                    if (range_target == 0.0) {
                        range_target = 1.0;
                    }
                    double y_pred_real = y_pred_norm * range_target + xmin[target_column_index];

                    char result_header[BUFFER_SIZE];
                    snprintf(result_header, BUFFER_SIZE, "Prediction Result:\n");
                    printf("%s", result_header);
                    if (send(client_fd, result_header, strlen(result_header), 0) < 0) {
                        perror("send failed");
                    }

                    char norm_result[BUFFER_SIZE + 50];
                    snprintf(norm_result, BUFFER_SIZE + 50, "Normalized: %.6f\n", y_pred_norm);
                    printf("%s", norm_result);
                    if (send(client_fd, norm_result, strlen(norm_result), 0) < 0) {
                        perror("send failed");
                    }

                    char real_result[BUFFER_SIZE + 50];
                    snprintf(real_result, BUFFER_SIZE + 50, "Real Scale: %.6f\n", y_pred_real);
                    printf("%s", real_result);
                    if (send(client_fd, real_result, strlen(real_result), 0) < 0) {
                        perror("send failed");
                    }
                }
            }
        }

        if (send(client_fd, "Closing connection...\n", 22, 0) < 0) {
            perror("send failed");
            close(client_fd);
            continue;
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
