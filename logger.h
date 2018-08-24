//
// Created by cyber-gh on 8/21/18.
//

#ifndef SQLITE_LOGGER_H
#define SQLITE_LOGGER_H

#include <stdbool.h>

#define LOGFILE "database.log"

extern bool LogCreated;

//void Log(char* message);
void DeleteLog();

#endif //SQLITE_LOGGER_H
