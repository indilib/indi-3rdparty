/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 Connection Plugin Interface

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "connectionplugins/connectioninterface.h"
#include "httplib.h"
#include <stdint.h>
#include <cstdlib>
#include <string>

namespace Connection
{
class HTTP : public Interface
{
  public:
    HTTP(INDI::DefaultDevice *dev);
    virtual ~HTTP() = default;

    virtual bool Connect() override;

    virtual bool Disconnect() override;

    virtual void Activated() override;

    virtual void Deactivated() override;

    virtual std::string name() override { return "CONNECTION_HTTP"; }

    virtual std::string label() override { return "HTTP"; }

    virtual const char *host() const { return AddressT[0].text; }

    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;

    httplib::Client *getClient() const { return client; }
    void setDefaultHost(const char *addressHost);

  protected:
    // IP Address
    ITextVectorProperty AddressTP;
    IText AddressT[1] {};

    httplib::Client *client;
};
}
