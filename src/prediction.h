#ifndef PREDICTION_H
#define PREDICTION_H

#include "preprocess.h"
#include "csv.h"
#include <sys/socket.h>

void handle_prediction(int client_fd);

#endif
