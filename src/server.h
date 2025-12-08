#ifndef SERVER_H
#define SERVER_H

#include "preprocess.h"
#include "csv.h"
#include "regression.h"
#include "prediction.h"
#include <sys/socket.h>

void handle_client(int client_fd, const char* filename);

#endif
