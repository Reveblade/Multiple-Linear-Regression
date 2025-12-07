#ifndef CSV_H
#define CSV_H

#include "preprocess.h"

int is_numeric(const char* str);
int contains_keyword(const char* str, const char* keyword);
void parse_csv_line(char* line, char* columns[], int* col_count);
int load_csv_data(const char* filename);
int analyze_csv(const char* filename);

#endif
