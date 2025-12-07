#ifndef REGRESSION_H
#define REGRESSION_H

#include "preprocess.h"
#include <sys/socket.h>

void* compute_coefficient(void* arg);
int gaussian_elimination();

#endif
