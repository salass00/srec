/*
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
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ZMBV_H
#define ZMBV_H

#ifndef SREC_H
#include "srec.h"
#endif

struct zmbv_state;

struct zmbv_state *zmbv_init(const struct SRecGlobal *gd, const struct SRecArgs *args);
BOOL zmbv_set_source_bm(struct zmbv_state *state, struct BitMap *bm);
BOOL zmbv_encode(struct zmbv_state *state, void **framep, uint32 *framesizep,
	BOOL *keyframep);
BOOL zmbv_encode_dup(struct zmbv_state *state, void **framep, uint32 *framesizep);
void zmbv_end(struct zmbv_state *state);

#endif

