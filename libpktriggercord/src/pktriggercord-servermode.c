/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>

    based on:

    pslr-shoot

    Command line remote control of Pentax DSLR cameras.
    Copyright (C) 2009 Ramiro Barreiro <ramiro_barreiro69@yahoo.es>
    With fragments of code from PK-Remote by Pontus Lidman.
    <https://sourceforge.net/projects/pkremote>

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
#ifndef WIN32
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>

#include <unistd.h>

#include "pslr.h"
#include "pslr_lens.h"

double timeval_diff_sec(struct timeval *t2, struct timeval *t1) {
    //DPRINT("tv2 %ld %ld t1 %ld %ld\n", t2->tv_sec, t2->tv_usec, t1->tv_sec, t1->tv_usec);
    return (t2->tv_usec - t1->tv_usec) / 1000000.0 + (t2->tv_sec - t1->tv_sec);
}

void camera_close(pslr_handle_t camhandle) {
    pslr_disconnect(camhandle);
    pslr_shutdown(camhandle);
}

pslr_handle_t camera_connect( char *model, char *device, int timeout, char *error_message ) {
    struct timeval prev_time;
    struct timeval current_time;
    pslr_handle_t camhandle;
    int r;

    gettimeofday(&prev_time, NULL);
    while (!(camhandle = pslr_init( model, device ))) {
        gettimeofday(&current_time, NULL);
        DPRINT("diff: %f\n", timeval_diff_sec(&current_time, &prev_time));
        if ( timeout == 0 || timeout > timeval_diff_sec(&current_time, &prev_time)) {
            DPRINT("sleep 1 sec\n");
            sleep_sec(1);
        } else {
            snprintf(error_message, 1000, "%d %ds timeout exceeded\n", 1, timeout);
            return NULL;
        }
    }

    DPRINT("before connect\n");
    if (camhandle) {
        if ((r=pslr_connect(camhandle)) ) {
            if ( r != -1 ) {
                snprintf(error_message, 1000, "%d Cannot connect to Pentax camera. Please start the program as root.\n",1);
            } else {
                snprintf(error_message, 1000, "%d Unknown Pentax camera found.\n",1);
            }
            return NULL;
        }
    }
    return camhandle;
}

#ifndef WIN32
int client_sock;

void write_socket_answer( char *answer ) {
    ssize_t r = write(client_sock, answer, strlen(answer));
    if (r != strlen(answer)) {
        fprintf(stderr, "write(answer) failed");
    }
}

void write_socket_answer_bin( uint8_t *answer, uint32_t length ) {
    ssize_t r = write(client_sock, answer, length);
    if (r != length) {
        fprintf(stderr, "write(answer) failed");
    }

}

char *is_string_prefix(char *str, char *prefix) {
    if ( !strncmp(str, prefix, strlen(prefix) ) ) {
        if ( strlen(str) <= strlen(prefix)+1 ) {
            return str;
        } else {
            return str+strlen(prefix)+1;
        }
    } else {
        return NULL;
    }
}

bool check_camera(pslr_handle_t camhandle) {
    if ( !camhandle ) {
        write_socket_answer("1 No camera connected\n");
        return false;
    } else {
        return true;
    }
}

void strip(char *s) {
    char *p2 = s;
    while (*s != '\0') {
        if (*s != '\r' && *s != '\n') {
            *p2++ = *s++;
        } else {
            ++s;
        }
    }
    *p2 = '\0';
}

int servermode_socket(int servermode_timeout) {
    int socket_desc, c, read_size;
    struct sockaddr_in server, client;
    char client_message[2000];
    char *arg;
    char buf[2100];
    pslr_handle_t camhandle=NULL;
    pslr_status status;
    char C;
    float F = 0;
    pslr_rational_t shutter_speed = {0, 0};
    uint32_t iso = 0;
    uint32_t auto_iso_min = 0;
    uint32_t auto_iso_max = 0;

    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        fprintf(stderr, "Could not create socket");
    }

    int enable = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
    }
    DPRINT("Socket created\n");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8888 );

    //Bind
    if ( bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "bind failed. Error");
        return 1;
    }
    DPRINT("bind done\n");

    //Listen
    listen(socket_desc, 3);

    //Accept and incoming connection
    DPRINT("Waiting for incoming connections...\n");
    c = sizeof(struct sockaddr_in);

    while ( true ) {
        fd_set rfds;
        struct timeval tv;
        int retval;
        FD_ZERO(&rfds);
        FD_SET(socket_desc, &rfds);
        tv.tv_sec = servermode_timeout;
        tv.tv_usec = 0;
        retval = select(socket_desc+1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            DPRINT("select error\n");
            exit(1);
        } else if (retval) {
            client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
            if (client_sock < 0) {
                fprintf(stderr, "accept failed");
                return 1;
            }
            DPRINT("Connection accepted\n");
        } else {
            DPRINT("Timeout\n");
            close(socket_desc);
            exit(0);
        }

        //Receive a message from client
        while ( (read_size = recv(client_sock, client_message, 2000, 0)) > 0 ) {
            client_message[read_size]='\0';
            strip( client_message );
            DPRINT(":%s:\n",client_message);
            if ( !strcmp(client_message, "stopserver" ) ) {
                if ( camhandle ) {
                    camera_close(camhandle);
                }
                write_socket_answer("0\n");
                exit(0);
            } else if ( !strcmp(client_message, "disconnect" ) ) {
                if ( camhandle ) {
                    camera_close(camhandle);
                }
                write_socket_answer("0\n");
            } else if ( (arg = is_string_prefix( client_message, "echo")) != NULL ) {
                sprintf( buf, "0 %.100s\n", arg);
                write_socket_answer(buf);
            } else if (  (arg = is_string_prefix( client_message, "usleep")) != NULL ) {
                int microseconds = atoi(arg);
                usleep(microseconds);
                write_socket_answer("0\n");
            } else if ( !strcmp(client_message, "connect") ) {
                if ( camhandle ) {
                    write_socket_answer("0\n");
                } else if ( (camhandle = camera_connect( NULL, NULL, -1, buf ))  ) {
                    write_socket_answer("0\n");
                } else {
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "update_status") ) {
                if ( check_camera(camhandle) ) {
                    if ( !pslr_get_status(camhandle, &status) ) {
                        sprintf( buf, "%d\n", 0);
                    } else {
                        sprintf( buf, "%d\n", 1);
                    }
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_camera_name") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %s\n", 0, pslr_camera_name(camhandle));
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_lens_name") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %s\n", 0, get_lens_name(status.lens_id1, status.lens_id2));
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_current_shutter_speed") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %d/%d\n", 0, status.current_shutter_speed.nom, status.current_shutter_speed.denom);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_current_aperture") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %s\n", 0, format_rational( status.current_aperture, "%.1f"));
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_current_iso") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %d\n", 0, status.current_iso);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_bufmask") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %d\n", 0, status.bufmask);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_auto_bracket_mode") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %d\n", 0, status.auto_bracket_mode);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "get_auto_bracket_picture_count") ) {
                if ( check_camera(camhandle) ) {
                    sprintf(buf, "%d %d\n", 0, status.auto_bracket_picture_count);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "focus") ) {
                if ( check_camera(camhandle) ) {
                    pslr_focus(camhandle);
                    sprintf(buf, "%d\n", 0);
                    write_socket_answer(buf);
                }
            } else if ( !strcmp(client_message, "shutter") ) {
                if ( check_camera(camhandle) ) {
                    pslr_shutter(camhandle);
                    sprintf(buf, "%d\n", 0);
                    write_socket_answer(buf);
                }
            } else if (  (arg = is_string_prefix( client_message, "delete_buffer")) != NULL ) {
                int bufno = atoi(arg);
                if ( check_camera(camhandle) ) {
                    pslr_delete_buffer(camhandle,bufno);
                    sprintf(buf, "%d\n", 0);
                    write_socket_answer(buf);
                }
            } else if (  (arg = is_string_prefix( client_message, "get_preview_buffer")) != NULL ) {
                int bufno = atoi(arg);
                if ( check_camera(camhandle) ) {
                    uint8_t *pImage;
                    uint32_t imageSize;
                    if ( pslr_get_buffer(camhandle, bufno, PSLR_BUF_PREVIEW, 4, &pImage, &imageSize) ) {
                        sprintf(buf, "%d %d\n", 1, imageSize);
                        write_socket_answer(buf);
                    } else {
                        sprintf(buf, "%d %d\n", 0, imageSize);
                        write_socket_answer(buf);
                        write_socket_answer_bin(pImage, imageSize);
                    }
                }
            } else if (  (arg = is_string_prefix( client_message, "get_buffer")) != NULL ) {
                int bufno = atoi(arg);
                if ( check_camera(camhandle) ) {
                    uint32_t imageSize;
                    if ( pslr_buffer_open(camhandle, bufno, PSLR_BUF_DNG, 0) ) {
                        sprintf(buf, "%d\n", 1);
                        write_socket_answer(buf);
                    } else {
                        imageSize = pslr_buffer_get_size(camhandle);
                        sprintf(buf, "%d %d\n", 0, imageSize);
                        write_socket_answer(buf);
                        uint32_t current = 0;
                        while (1) {
                            uint32_t bytes;
                            uint8_t buf[65536];
                            bytes = pslr_buffer_read(camhandle, buf, sizeof (buf));
                            if (bytes == 0) {
                                break;
                            }
                            write_socket_answer_bin( buf, bytes);
                            current += bytes;
                        }
                        pslr_buffer_close(camhandle);
                    }
                }
            } else if (  (arg = is_string_prefix( client_message, "set_shutter_speed")) != NULL ) {
                if ( check_camera(camhandle) ) {
                    // TODO: merge with pktriggercord-cli shutter speed parse
                    if (sscanf(arg, "1/%d%c", &shutter_speed.denom, &C) == 1) {
                        shutter_speed.nom = 1;
                        sprintf(buf, "%d %d %d\n", 0, shutter_speed.nom, shutter_speed.denom);
                    } else if ((sscanf(arg, "%f%c", &F, &C)) == 1) {
                        if (F < 2) {
                            F = F * 10;
                            shutter_speed.denom = 10;
                            shutter_speed.nom = F;
                        } else {
                            shutter_speed.denom = 1;
                            shutter_speed.nom = F;
                        }
                        sprintf(buf, "%d %d %d\n", 0, shutter_speed.nom, shutter_speed.denom);
                    } else {
                        shutter_speed.nom = 0;
                        sprintf(buf,"1 Invalid shutter speed value.\n");
                    }
                    if (shutter_speed.nom) {
                        pslr_set_shutter(camhandle, shutter_speed);
                    }
                    write_socket_answer(buf);
                }
            } else if (  (arg = is_string_prefix( client_message, "set_iso")) != NULL ) {
                if ( check_camera(camhandle) ) {
                    // TODO: merge with pktriggercord-cli shutter iso
                    if (sscanf(arg, "%d-%d%c", &auto_iso_min, &auto_iso_max, &C) != 2) {
                        auto_iso_min = 0;
                        auto_iso_max = 0;
                        iso = atoi(arg);
                    } else {
                        iso = 0;
                    }
                    if (iso==0 && auto_iso_min==0) {
                        sprintf(buf,"1 Invalid iso value.\n");
                    } else {
                        pslr_set_iso(camhandle, iso, auto_iso_min, auto_iso_max);
                        sprintf(buf, "%d %d %d-%d\n", 0, iso, auto_iso_min, auto_iso_max);
                    }
                    write_socket_answer(buf);
                }
            } else {
                write_socket_answer("1 Invalid servermode command\n");
            }
        }

        if (read_size == 0) {
            DPRINT("Client disconnected\n");
            fflush(stdout);
        } else if (read_size == -1) {
            fprintf(stderr, "recv failed\n");
        }
    }
    return 0;
}
#endif
