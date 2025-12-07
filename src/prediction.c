#include "prediction.h"
#include <math.h>

void handle_prediction(int client_fd) {
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
