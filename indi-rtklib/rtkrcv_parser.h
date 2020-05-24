/*
 * Copyright © 2014 Kosma Moczek <kosma@cloudyourcar.com>
 * This program is free software. It comes without any warranty, to the extent
 * permitted by applicable law. You can redistribute it and/or modify it under
 * the terms of the Do What The Fuck You Want To Public License, Version 2, as
 * published by Sam Hocevar. See the COPYING file for more details.
 */

#ifndef RTKRCV_H
#define RTKRCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define RTKRCV_MAX_LENGTH 150

#define RTKRCV_FIX_NONE "------"
#define RTKRCV_FIX "FIX"
#define RTKRCV_FIX_FLOAT "FLOAT"
#define RTKRCV_FIX_SBAS "SBAS"
#define RTKRCV_FIX_DGPS "DGPS"
#define RTKRCV_FIX_SINGLE "SINGLE"
#define RTKRCV_FIX_PPP "PPP"
#define RTKRCV_FIX_UNKNOWN ""

enum rtkrcv_fix_status {
    status_no_fix=1,
    status_fix,
    status_float,
    status_sbas,
    status_dgps,
    status_single,
    status_ppp,
    status_unknown
};

void scansolution(char* solution, char *flags, char *type, double *dms, enum rtkrcv_fix_status *fix, double *timestamp);

#ifdef __cplusplus
}
#endif

#endif /* RTKRCV_H */

/* vim: set ts=4 sw=4 et: */
