#ifndef REPORTS_H
#define REPORTS_H

#include <time.h>

typedef struct {
    int report_id;
    char inspector_name[32];
    float latitude;
    float longitude;
    char category[16];
    int severity;
    time_t timestamp;
    char description[136];
} Report;

#endif