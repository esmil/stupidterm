# This file is part of stupidterm.
# Copyright (C) 2013-2015 Emil Renner Berthing
#
# This is free software; you can redistribute it and/or modify it under
# the terms of the GNU Library General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

CC          ?= gcc -std=gnu99
CFLAGS      ?= -O2 -ggdb -pipe -Wall -Wextra -Wno-unused-parameter

INSTALL      = install
PKGCONFIG    = pkg-config
RM           = rm -f

binary       = st

prefix       = /usr/local
exec_prefix  = ${prefix}
bindir       = ${exec_prefix}/bin
includedir   = ${prefix}/include
libdir       = ${exec_prefix}/lib
datarootdir  = ${prefix}/share
pkgconfigdir = ${libdir}/pkgconfig

CFLAGS      += $(shell $(PKGCONFIG) --cflags vte-2.91)
LIBS        += $(shell $(PKGCONFIG) --libs vte-2.91)

ifdef V
E=@\#
Q=
else
E=@echo
Q=@
endif

.PHONY: all install clean

all: $(binary)

release: CPPFLAGS += -DG_DISABLE_ASSERT -DNDEBUG
release: $(binary)

$(binary): stupidterm.c
	$E '  CC/LD   $@'
	$Q$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(DESTDIR)$(bindir):
	$E '  INSTALL $@'
	$Q$(INSTALL) -d $@

$(DESTDIR)$(bindir)/$(binary): $(binary) $(DESTDIR)$(bindir)
	$E '  INSTALL $@'
	$Q$(INSTALL) -m 755 $< $@

install: $(DESTDIR)$(bindir)/$(binary)

clean:
	$E '  RM      $(binary)'
	$Q$(RM) $(binary)
