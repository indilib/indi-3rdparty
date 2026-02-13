/*
 GigE interface wrapper on araviss
 Copyright (C) 2016 Hendrik Beijeman (hbeyeman@gmail.com)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "ArvGeneric.h"
#include "BlackFly.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>

#define BLACKFLY_MODEL "BFLY-PGE-31S4M"

arv::ArvCamera *ArvFactory::find_first_available(void)
{
    /* We first ensure the library knows of all available devices and info */
    arv_update_device_list();

    if (arv_get_n_devices() == 0)
        /* no devices found */
        return nullptr;


    const char *device_id = arv_get_device_id(0);
    const char *model_name = arv_get_device_model(0);

    if (memmem(model_name, strlen(model_name), BLACKFLY_MODEL, strlen(BLACKFLY_MODEL)))
    {
        printf("Creating BlackFly... for %s-%s\n", model_name, device_id);
        return new BlackFly(device_id, model_name);
    }
    else
    {
        printf("Creating Generic... for %s-%s\n", model_name, device_id);
        return new ArvGeneric(device_id, model_name);
    }
}
