/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2009 Nedko Arnaudov <nedko@arnaudov.name>
 *
 **************************************************************************
 * This file contains interface to studio object helpers
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

#ifndef STUDIO_H__0BEDE85E_4FB3_4D74_BC08_C373A22409C0__INCLUDED
#define STUDIO_H__0BEDE85E_4FB3_4D74_BC08_C373A22409C0__INCLUDED

#include "common.h"

bool
studio_create(
  struct studio ** studio_ptr_ptr);

void
studio_destroy(
  struct studio * studio_ptr);

#endif /* #ifndef STUDIO_H__0BEDE85E_4FB3_4D74_BC08_C373A22409C0__INCLUDED */