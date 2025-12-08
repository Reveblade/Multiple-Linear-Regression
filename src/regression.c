#include "regression.h"
#include <math.h>

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
