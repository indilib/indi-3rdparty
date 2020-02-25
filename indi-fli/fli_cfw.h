#if 0
FLI CFW
INDI Interface for Finger Lakes Instrument Filter Wheels
Copyright (C) 2003 - 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#pragma once

#include <libfli.h>
#include <indifilterwheel.h>
#include <string.h>

class FLICFW : public INDI::FilterWheel
{
    public:
        FLICFW();
            ~FLICFW() override = default;

            const char *getDefaultName() override;
            virtual bool initProperties() override;
            virtual void ISGetProperties(const char *dev) override;
            virtual bool updateProperties() override;

            virtual bool Connect() override;
            virtual bool Disconnect() override;

            virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        protected:
            virtual bool SelectFilter(int) override;
            virtual int QueryFilter() override;

            virtual void debugTriggered(bool enable) override;

        private:
            typedef struct
            {
                flidomain_t domain;
                char *dname;
                char *name;
                char model[MAXINDINAME];
                long HWRevision;
                long FWRevision;
                long raw_pos;
                long count;
            } filter_t;

            ISwitchVectorProperty PortSP;
            ISwitch PortS[4];

            ITextVectorProperty FilterInfoTP;
            IText FilterInfoT[3] {};

            ISwitchVectorProperty TurnWheelSP;
            ISwitch TurnWheelS[2];

            flidev_t fli_dev;
            filter_t FLIFilter;

            bool findFLICFW(flidomain_t domain);
            void turnWheel();
            bool setupParams();
    };
