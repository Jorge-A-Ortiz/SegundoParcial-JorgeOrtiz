#ifndef P02_CAPTURE_H
#define P02_CAPTURE_H

#include <stdbool.h>

void P02_Init(void);
void P02_Report(bool (*queueLine)(const char *line));

#endif
