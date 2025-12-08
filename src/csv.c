#include "csv.h"

static void resolve_file_path(const char* input, char* resolved) {
    if (strchr(input, '/') != NULL) {
        strncpy(resolved, input, BUFFER_SIZE - 1);
        resolved[BUFFER_SIZE - 1] = '\0';
    } else {
        snprintf(resolved, BUFFER_SIZE, "../config/%s", input);
    }
}

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
    char resolved_path[BUFFER_SIZE];
    resolve_file_path(filename, resolved_path);
    FILE *file = fopen(resolved_path, "r");
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

int analyze_csv(const char* filename) {
    char resolved_path[BUFFER_SIZE];
    resolve_file_path(filename, resolved_path);
    FILE *file = fopen(resolved_path, "r");
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
                contains_keyword(header_cols[i], "label") ||
                contains_keyword(header_cols[i], "performance") ||
                contains_keyword(header_cols[i], "index") ||
                contains_keyword(header_cols[i], "output") ||
                contains_keyword(header_cols[i], "dependent")) {
                target_column_index = i;
            }
        }
    }

    // Fallback: if no target found, use last numeric column
    if (target_column_index == -1) {
        for (int i = total_columns - 1; i >= 0; i--) {
            if (column_types[i] == NUMERIC) {
                target_column_index = i;
                break;
            }
        }
    }

    return 0;
}
