/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/scsiio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include "pslr_model.h"

#include "pslr_scsi.h"

#include <assert.h>
#define SCSI_INQUIRY 0x12

void print_scsi_error(scsireq_t *req) {
    int k;

    if (req->senselen_used > 0) {
        DPRINT("SCSI error: sense data: ");
        for (k = 0; k < req->senselen_used; ++k) {
            if ((k > 0) && (0 == (k % 10))) {
                DPRINT("\n  ");
            }
            DPRINT("0x%02x ", req->sense[k]);
        }
        DPRINT("\n");
    }

    DPRINT("SCSI status=0x%x\n", req->status);
}

char **get_drives(int *drive_num) {
    DIR *d;
    struct dirent *ent;
    char *tmp[256];
    char **ret;
    int j,jj;
    d = opendir("/dev");


    if ( !d ) {
        DPRINT("Cannot open /dev\n");
        *drive_num = 0;
        return NULL;
    }
    j=0;
    while ( (ent = readdir(d)) ) {
        if (strncmp(ent->d_name, "rsd", 3) == 0 &&
                strnlen(ent->d_name, 6) > 4 && ent->d_name[4] == 'c') {
            tmp[j] = malloc( strlen(ent->d_name)+1 );
            strncpy(tmp[j], ent->d_name, strlen(ent->d_name)+1);
            ++j;
        }
    }
    closedir(d);
    ret = malloc( j * sizeof(char*) );
    for ( jj=0; jj<j; ++jj ) {
        ret[jj] = malloc( strlen(tmp[jj])+1 );
        strncpy( ret[jj], tmp[jj], strlen(tmp[jj]) );
        ret[jj][strlen(tmp[jj])]='\0';
    }
    *drive_num = j;
    return ret;
}

pslr_result get_drive_info(char* drive_name, int* device,
                           char* vendor_id, int vendor_id_size_max,
                           char* product_id, int product_id_size_max) {
    int fd;
    char buf[100];
    char deviceName[256];
    scsireq_t screq = {
        /* flags */ SCCMD_READ,
        /* timeout */ 1000,
        /* cmd */ {0x12, 0, 0, 0, sizeof(buf), 0},
        /* cmdlen */ 6,
        /* databuf */ (caddr_t)&buf,
        /* datalen */ sizeof(buf),
        /* datalen_used */ 0,
        /* sense */ {},
        /* senselen */ SENSEBUFLEN,
        /* senselen_used */ 0,
        /* status */ 0,
        /* retsts */ 0,
        /* error */ 0
    };
    char *p, *q;

    vendor_id[0] = '\0';
    product_id[0] = '\0';
    snprintf(deviceName, 256, "/dev/%s", drive_name);
    fd = open(deviceName, O_RDWR);
    if (fd == -1) {
        perror("Device open while querying:");
        return PSLR_DEVICE_ERROR;
    }
    if (ioctl(fd, SCIOCCOMMAND, &screq) < 0 ||
            screq.status != 0 ||
            screq.retsts != SCCMD_OK) {
        vendor_id[0] = product_id[0] = '\0';
        close(fd);
        DPRINT("IOCTL failed in query\n");
        return PSLR_DEVICE_ERROR;
    }
    DPRINT("Camera queried.+n");
    /* Vendor */
    p = buf + 8;
    q = buf + ((16 < vendor_id_size_max)?16:vendor_id_size_max);
    while (q > p && q[-1] == ' ') {
        q--;
    }
    memcpy(vendor_id, p, q - p);
    vendor_id[q - p] = '\0';
    /* Product */
    p = buf + 16;
    q = p + ((16 < product_id_size_max)?16:product_id_size_max);
    while (q > p && q[-1] == ' ') {
        q--;
    }
    memcpy(product_id, p, q - p);
    product_id[q - p] = '\0';

    close(fd);

    *device = open(deviceName, O_RDWR);
    if ( *device == -1) {
        return PSLR_DEVICE_ERROR;
    }
    return PSLR_OK;
}

void close_drive(int *device) {
    close( *device );
}

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen) {

    int r;
    unsigned int i;
    scsireq_t screq;

    memset(&screq, 0, sizeof(screq));
    screq.flags = SCCMD_READ;
    screq.timeout = 20000;

    assert(cmdLen < CMDBUFLEN);

    memcpy(screq.cmd, cmd, cmdLen);
    screq.cmdlen = cmdLen;
    screq.senselen = SENSEBUFLEN;

    screq.databuf = buf;
    screq.datalen = bufLen;

    DPRINT("[S]\t\t\t\t\t >>> [");
    for (i = 0; i < cmdLen; ++i) {
        if (i > 0) {
            DPRINT(" ");
            if ((i%4) == 0 ) {
                DPRINT(" ");
            }
        }
        DPRINT("%02X", cmd[i]);
    }
    DPRINT("]\n");

    r = ioctl(sg_fd, SCIOCCOMMAND, &screq);
    if (r == -1) {
        perror("ioctl");
        return -PSLR_DEVICE_ERROR;
    }

    if (screq.retsts != SCCMD_OK) {
        print_scsi_error(&screq);
        return -PSLR_SCSI_ERROR;
    } else {
        DPRINT("[S]\t\t\t\t\t <<< [");
        for (i = 0; i < 32 && i < screq.datalen_used; ++i) {
            if (i > 0) {
                DPRINT(" ");
                if (i % 16 == 0) {
                    DPRINT("\n\t\t\t\t\t      ");
                } else if ((i%4) == 0 ) {
                    DPRINT(" ");
                }
            }
            DPRINT("%02X", buf[i]);
        }
        DPRINT("]\n");

        /* Older Pentax DSLR will report all bytes remaining, so make
         * a special case for this (treat it as all bytes read). */
        /* Linux solution:
            if (io.resid == bufLen)
                return bufLen;
            else
                return bufLen - io.resid;
        */
        /* In BSD this could be something like:
        if (screq.datalen_used == 0)
            return bufLen;
        */
        return screq.datalen_used;

    }
}

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen) {


    int r;
    unsigned int i;
    scsireq_t screq;

    memset(&screq, 0, sizeof(screq));
    screq.flags = SCCMD_WRITE;
    screq.timeout = 20000;

    assert(cmdLen < CMDBUFLEN);

    memcpy(screq.cmd, cmd, cmdLen);
    screq.cmdlen = cmdLen;
    screq.senselen = SENSEBUFLEN;

    screq.databuf = buf;
    screq.datalen = bufLen;


    //  print debug scsi cmd
    DPRINT("[S]\t\t\t\t\t >>> [");
    for (i = 0; i < cmdLen; ++i) {
        if (i > 0) {
            DPRINT(" ");
            if ((i%4) == 0 ) {
                DPRINT(" ");
            }
        }
        DPRINT("%02X", cmd[i]);
    }
    DPRINT("]\n");
    if (bufLen > 0) {
        //  print debug write buffer
        DPRINT("[S]\t\t\t\t\t >>> [");
        for (i = 0; i < 32 && i < bufLen; ++i) {
            if (i > 0) {
                DPRINT(" ");
                if (i % 16 == 0) {
                    DPRINT("\n\t\t\t\t\t      ");
                } else if ((i%4) == 0 ) {
                    DPRINT(" ");
                }
            }
            DPRINT("%02X", buf[i]);
        }
        DPRINT("]\n");
    }

    r = ioctl(sg_fd, SCIOCCOMMAND, &screq);

    if (r == -1) {
        perror("ioctl");
        return PSLR_DEVICE_ERROR;
    }

    if (screq.retsts != SCCMD_OK) {
        print_scsi_error(&screq);
        return PSLR_SCSI_ERROR;
    } else {
        return PSLR_OK;
    }
}
