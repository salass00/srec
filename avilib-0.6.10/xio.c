/*
 *  libxio.c
 *
 *  Copyright (C) Lukas Hejtmanek - January 2004
 *
 *  This file is part of transcode, a linux video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "xio.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

int xio_open(void *pathname, int flags, ...) {
	va_list ap;
	int fd;

	va_start(ap, flags);

	if (flags & O_CREAT)
		fd = open64(pathname, flags, va_arg(ap, mode_t));
	else
		fd = open64(pathname, flags);

	va_end(ap);

	return fd;
}

ssize_t xio_read(int fd, void *buf, size_t count) {
	return read(fd, buf, count);
}

ssize_t xio_write(int fd, const void *buf, size_t count) {
	return write(fd, buf, count);
}

int xio_ftruncate(int fd, int64_t length) {
	return ftruncate64(fd, length);
}

int64_t xio_lseek(int fd, int64_t offset, int whence) {
	return lseek64(fd, offset, whence);
}

int xio_close(int fd) {
	return close(fd);
}

