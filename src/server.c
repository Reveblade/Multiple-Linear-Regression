#include "server.h"
#include <pthread.h>

void handle_client(int client_fd, const char* filename) {
    if (strchr(filename, '/') == NULL) {
        snprintf(current_filename, BUFFER_SIZE, "../config/%s", filename);
    } else {
        strncpy(current_filename, filename, BUFFER_SIZE - 1);
        current_filename[BUFFER_SIZE - 1] = '\0';
    }
    client_fd_global = client_fd;

    char response[BUFFER_SIZE + 20];
    snprintf(response, BUFFER_SIZE + 20, "You entered: %s\n", filename);
    if (send(client_fd, response, strlen(response), 0) < 0) {
        perror("send failed");
        return;
    }

    int result = analyze_csv(current_filename);

    if (result < 0) {
        char error_msg[BUFFER_SIZE + 50];
        snprintf(error_msg, BUFFER_SIZE + 50, "ERROR: Dataset file \"%s\" not found!\n", filename);
        printf("%s", error_msg);
        if (send(client_fd, error_msg, strlen(error_msg), 0) < 0) {
            perror("send failed");
        }
        return;
    }

    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE, "[OK] File \"%.900s\" found.\nReading file...\n", filename);
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

    if (load_csv_data(current_filename) == 0) {
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

            handle_prediction(client_fd);
        }
    }
}
