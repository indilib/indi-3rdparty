/*******************************************************************************
  Copyright(c) 2020 Ilia Platone - Jasem Mutlaq. All rights reserved.

  INDI RTKLIB Driver

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "rtkrcv_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

#define boolstr(s) ((s) ? "true" : "false")

void scansolution(char* solution, char *flags, char *type, double *dms, enum rtkrcv_fix_status *fix, double *timestamp)
{
    double pos[3]={0},Qe[9]={0},dms1[3]={0},dms2[3]={0},rr[3]={0};
    double sol_age,sol_ratio;
    int sol_ns;
    char solflag, soltype;
    char ns, we, el, enu[3]={0};
    int matched;
    char status[6];
    sscanf(solution,"(%6c)",status);

    if(!strcmp(status, RTKRCV_FIX_NONE))
        *fix = status_no_fix;
    else if(!strcmp(status, RTKRCV_FIX))
        *fix = status_fix;
    else if(!strcmp(status, RTKRCV_FIX_FLOAT))
        *fix = status_float;
    else if(!strcmp(status, RTKRCV_FIX_SBAS))
        *fix = status_sbas;
    else if(!strcmp(status, RTKRCV_FIX_DGPS))
        *fix = status_dgps;
    else if(!strcmp(status, RTKRCV_FIX_SINGLE))
        *fix = status_single;
    else if(!strcmp(status, RTKRCV_FIX_PPP))
        *fix = status_ppp;
    else if(!strcmp(status, RTKRCV_FIX_UNKNOWN))
        *fix = status_unknown;

    solflag = 0;
    matched = 0;
    soltype = 0;
    matched += sscanf(solution," %c:%lf %lf %lf",&ns,&dms1[0],&dms1[1],&dms1[2]);
    matched += sscanf(solution," %c:%lf %lf %lf",&we,&dms2[0],&dms2[1],&dms2[2]);
    matched += sscanf(solution," %c:%lf",&el,&pos[2]);
    if(matched != 9) {
        matched += sscanf(solution," %c:%lf",&ns,&rr[0]);
        matched += sscanf(solution," %c:%lf",&we,&rr[1]);
        matched += sscanf(solution," %c:%lf",&el,&rr[2]);
        if(matched == 6) {
            switch (ns) {
            case 'N':
            case 'S':
                soltype=1;
                break;
            case 'X':
                soltype=2;
                break;
            case 'E':
                soltype=3;
                break;
            case 'P':
                soltype=4;
                break;
            }
        }
    }
    matched = sscanf(solution," (%c:%lf %c:%lf %c:%lf)",&enu[0],&Qe[0],&enu[1],&Qe[4],&enu[2],&Qe[8]);
    if(matched == 6) {
        solflag |= 1;
        switch (enu[0]) {
        case 'N':
            dms[0] = Qe[0];
            dms[1] = Qe[1];
            dms[2] = Qe[2];
            break;
        case 'E':
            dms[0] = Qe[1];
            dms[1] = Qe[0];
            dms[2] = Qe[2];
            break;
        case 'X':
            dms[0] = Qe[0];
            dms[1] = Qe[1];
            dms[2] = Qe[2];
            break;
        }
    }
    *timestamp = 0;
    matched = sscanf(solution," A:%lf R:%lf N:%2d",&sol_age,&sol_ratio,&sol_ns);
    if(matched == 3) {
        *timestamp = sol_age+(sol_ratio/1000000000.0);
        solflag |= 2;
    }
    *flags = solflag;
    *type = soltype;
}
