/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains interface to app supervisor object that is backed through D-Bus
 **************************************************************************
 *
 * LADI Session Handler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LADI Session Handler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LADI Session Handler. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef APP_SUPERVISOR_PROXY_H__A48C609D_0AB6_4C91_A9B0_BC7F1B7E4CB4__INCLUDED
#define APP_SUPERVISOR_PROXY_H__A48C609D_0AB6_4C91_A9B0_BC7F1B7E4CB4__INCLUDED

#include "common.h"

typedef struct ladish_app_supervisor_proxy_tag { int unused; } * ladish_app_supervisor_proxy_handle;

bool ladish_app_supervisor_proxy_create(const char * service, const char * object, ladish_app_supervisor_proxy_handle * proxy_ptr);
void ladish_app_supervisor_proxy_destroy(ladish_app_supervisor_proxy_handle proxy);
bool ladish_app_supervisor_proxy_run_custom(ladish_app_supervisor_proxy_handle proxy, const char * command, const char * name, bool run_in_terminal);

#endif /* #ifndef APP_SUPERVISOR_PROXY_H__A48C609D_0AB6_4C91_A9B0_BC7F1B7E4CB4__INCLUDED */