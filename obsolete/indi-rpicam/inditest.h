/*******************************************************************************
 Copyright(c) 2010-2018 Jasem Mutlaq. All rights reserved.

 Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

 Rapid Guide support added by CloudMakers, s. r. o.
 Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

 Star detection algorithm is based on PHD Guiding by Craig Stark
 Copyright (c) 2006-2010 Craig Stark. All rights reserved.

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

#ifndef _INDITEST_H
#define _INDITEST_H
#include <stdio.h>

#ifndef NDEBUG
#define LOG_TEST(fmt) fprintf(stderr, "%s(%s:%d): " fmt "\n", __FUNCTION__, __FILE__, __LINE__)
#define LOGF_TEST(fmt, ...) fprintf(stderr, "%s(%s:%d): " fmt "\n", __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_TEST(fmt, ...)
#define LOGF_TEST(fmt, ...)
#endif

#endif // _INDITEST_H
