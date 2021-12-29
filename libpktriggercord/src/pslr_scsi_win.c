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
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

#include "pslr_scsi.h"

#define IOCTL_SCSI_PASS_THROUGH_DIRECT  0x4D014
#define SCSI_IOCTL_DATA_IN              1
#define SCSI_IOCTL_DATA_OUT             0

typedef struct _SCSI_PASS_THROUGH_DIRECT {
    USHORT Length;
    UCHAR  ScsiStatus;
    UCHAR  PathId;
    UCHAR  TargetId;
    UCHAR  Lun;
    UCHAR  CdbLength;
    UCHAR  SenseInfoLength;
    UCHAR  DataIn;
    ULONG  DataTransferLength;
    ULONG  TimeOutValue;
    PVOID  DataBuffer;
    ULONG  SenseInfoOffset;
    UCHAR  Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;


typedef struct _SCSI_PASS_THROUGH_WITH_BUFFER {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG             Filler;      // realign buffers to double word boundary
    UCHAR             ucSenseBuf[32];
} SCSI_PASS_THROUGH_WITH_BUFFER;

char **get_drives(int *driveNum) {
    char **ret;
    ret = malloc( ('Z'-'C'+1) * sizeof(char *));
    int driveLetter;
    int j=0;
    for ( driveLetter = 'C'; driveLetter<='Z'; ++driveLetter ) {
#ifdef RAD10
// These Drive types cant be a Pentax. The RAD10 debugger breaks here.
        TCHAR root[4];
        snprintf(root, 4, "%c:\\", driveLetter);
        switch (GetDriveType(root)) {
            case  DRIVE_UNKNOWN:
            case  DRIVE_NO_ROOT_DIR:
            case  DRIVE_FIXED:
            case  DRIVE_REMOTE:
            case  DRIVE_CDROM:
                continue;
            default:
                ;
        }
#endif
        ret[j] = malloc( 2 * sizeof (char) );
        snprintf(ret[j], 2, "%c", driveLetter);
        ++j;
    }
    *driveNum = j;
    return ret;
}

pslr_result get_drive_info(char* drive_name, int* device,
                           char* vendor_id, int vendor_id_size_max,
                           char* product_id, int product_id_size_max
                          ) {
    bool Status;
    STORAGE_PROPERTY_QUERY query;
    STORAGE_DEVICE_DESCRIPTOR* pdescriptor;
    byte descriptorBuf[256];
    DWORD bytesRead;
    pslr_result drive_status = PSLR_DEVICE_ERROR;
    HANDLE hDrive;
    char fullDriveName[7];

    vendor_id[0] = '\0';
    product_id[0] = '\0';
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    snprintf( fullDriveName, 7, "\\\\.\\%s:", drive_name);

    hDrive = CreateFile(fullDriveName,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);

    if (hDrive != INVALID_HANDLE_VALUE) {
        Status = DeviceIoControl(hDrive,
                                 IOCTL_STORAGE_QUERY_PROPERTY,
                                 &query,
                                 sizeof(query),
                                 &descriptorBuf,
                                 sizeof(descriptorBuf),
                                 &bytesRead,
                                 (LPOVERLAPPED)0);
        if (Status==FALSE) {
            int LastError = GetLastError(); //lastError alwasy return 1450
            if (LastError != 0) {
                CancelIo(hDrive);
            }
        } else {
            *device = (int)hDrive;
            drive_status = PSLR_OK;

            pdescriptor = (STORAGE_DEVICE_DESCRIPTOR *)descriptorBuf;

            if (pdescriptor->VendorIdOffset != 0) {
                int i = 0;
                while ((descriptorBuf[pdescriptor->VendorIdOffset + i] != 0)
                        &&(i < vendor_id_size_max)
                      ) {
                    vendor_id[i] = descriptorBuf[pdescriptor->VendorIdOffset + i];
                    i++;
                }
                vendor_id[i]='\0';
            }
            if (pdescriptor->ProductIdOffset != 0) {
                int i = 0;
                while ((descriptorBuf[pdescriptor->ProductIdOffset + i] != 0)
                        &&(i < product_id_size_max)
                      ) {
                    product_id[i] = descriptorBuf[pdescriptor->ProductIdOffset + i];
                    i++;
                }
                product_id[i]='\0';
            }
        }
    }
    return drive_status;
}

void close_drive(int *device) {
    CloseHandle((HANDLE)*device);
}

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen) {
    SCSI_PASS_THROUGH_WITH_BUFFER sptdwb;
    DWORD outByte=0;
    int Status;
    int LastError=0;
    uint8_t dataIn[64*1024];

    sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptdwb.sptd.ScsiStatus = 0;
    sptdwb.sptd.PathId = 0;
    sptdwb.sptd.TargetId = 0;
    sptdwb.sptd.Lun = 0;
    sptdwb.sptd.CdbLength = cmdLen;
    sptdwb.sptd.SenseInfoLength = sizeof(sptdwb.ucSenseBuf);
    sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    sptdwb.sptd.DataTransferLength = bufLen;
    sptdwb.sptd.TimeOutValue = 10;
    sptdwb.sptd.DataBuffer = dataIn;
    sptdwb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFER,ucSenseBuf);

    memset(sptdwb.sptd.Cdb, 0, sizeof(sptdwb.sptd.Cdb));
    memcpy(sptdwb.sptd.Cdb, cmd, cmdLen);

    Status=DeviceIoControl((HANDLE)sg_fd,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           &sptdwb,
                           sizeof(sptdwb),
                           &sptdwb,
                           sizeof(sptdwb),
                           &outByte,
                           NULL);
    if (Status==0) {
        LastError = GetLastError();
        if (LastError != 0) {
            CancelIo((HANDLE)sg_fd);
        }
    }

    memcpy(buf,sptdwb.sptd.DataBuffer,bufLen);

    if (LastError != 0) {
        return -PSLR_SCSI_ERROR;
    } else {
        if (sptdwb.sptd.DataTransferLength == bufLen) {
            return bufLen;
        } else {
            return bufLen - sptdwb.sptd.DataTransferLength;
        }
    }
}

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen) {
    SCSI_PASS_THROUGH_WITH_BUFFER sptdwb;
    DWORD outByte=0;
    int Status;
    int LastError=0;

    sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptdwb.sptd.ScsiStatus = 0;
    sptdwb.sptd.PathId = 0;
    sptdwb.sptd.TargetId = 0;
    sptdwb.sptd.Lun = 0;
    sptdwb.sptd.CdbLength = cmdLen;
    sptdwb.sptd.SenseInfoLength = sizeof(sptdwb.ucSenseBuf);
    sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
    sptdwb.sptd.DataTransferLength = bufLen;
    sptdwb.sptd.TimeOutValue = 10;
    sptdwb.sptd.DataBuffer = buf;
    sptdwb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFER,ucSenseBuf);

    memset(sptdwb.sptd.Cdb, 0, sizeof(sptdwb.sptd.Cdb));
    memcpy(sptdwb.sptd.Cdb, cmd, cmdLen);

    Status=DeviceIoControl((HANDLE)sg_fd,
                           IOCTL_SCSI_PASS_THROUGH_DIRECT,
                           &sptdwb,
                           sizeof(sptdwb),
                           &sptdwb,
                           sizeof(sptdwb),
                           &outByte,
                           NULL);
    if (Status==0) {
        LastError = GetLastError();
        if (LastError != 0) {
            CancelIo((HANDLE)sg_fd);
        }
    }
    if (LastError != 0) {
        return PSLR_SCSI_ERROR;
    } else {
        return PSLR_OK;
    }
}
