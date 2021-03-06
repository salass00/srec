/*
 * SRec - Screen Recorder
 * Copyright (C) 2016 Fredrik Wikstrom <fredrik@a500.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef POINTER_H
#define POINTER_H

#ifndef SREC_H
#include "srec.h"
#endif

void strip_info_extension(STRPTR name);
struct srec_pointer *load_pointer(const struct SRecGlobal *gd, CONST_STRPTR name);
void scale_pointer(struct srec_pointer *sp);
void render_pointer(struct srec_pointer *sp, int32 mouse_x, int32 mouse_y);
void free_pointer(struct srec_pointer *sp);

#endif

