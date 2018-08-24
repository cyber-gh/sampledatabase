//
// Created by cyber-gh on 8/21/18.
//

#include <stdlib.h>
#include <stdio.h>
#include "logger.h"

bool LogCreated = false;
//
//void Log(char* message){
//    FILE* file;
//
//    if (!LogCreated) {
//        file = fopen(LOGFILE, "w");
//        LogCreated = true;
//    } else {
//
//        file = fopen(LOGFILE, "a");
//    }
//
//    if (file == NULL){
//        if (LogCreated){
//            LogCreated = false;
//        }
//        return ;
//    } else {
//        fputs(message, file);
//        fputs("\n", file);
//    }
//
//    if (file){
//        fclose(file);
//    }
//}

void DeleteLog(){
    int res = remove(LOGFILE);
}

