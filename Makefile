# Unix Makefile for GlkTerm library

# This generates two files. One, of course, is libglkterm.a -- the library
# itself. The other is Make.glkterm; this is a snippet of Makefile code
# which locates the glkterm library and associated libraries (such as
# ncurses.)
#
# When you install glkterm, you must put libglkterm.a in the lib directory,
# and glk.h, glkstart.h, and Make.glkterm in the include directory.

# Pick a C compiler.
#CC = cc
CC = gcc -ansi

# Do you want sound support using SDL2_mixer?
SOUND = yes

OPTIONS = -O

PKG_CONFIG = $(shell which pkg-config)
NO_PKG_CONFIG_NCURSES = 0
NO_PKG_CONFIG_SDL2 = 0
ifeq (, $(PKG_CONFIG))
  NO_PKG_CONFIG_NCURSES = 1
  NO_PKG_CONFIG_SDL2 = 1
  $(warning *** pkg-config not found.)
else
  ifneq (0, $(shell $(PKG_CONFIG) --exists ncurses ; echo $$?))
    $(warning *** No ncurses package found.)
    NO_PKG_CONFIG_NCURSES = 1
  endif
  ifneq (0, $(shell $(PKG_CONFIG) --exists sdl2 SDL2_mixer ; echo $$?))
    $(warning *** No ncurses package found.)
    NO_PKG_CONFIG_NCURSES = 1
  endif
endif

ifneq (0, $(NO_PKG_CONFIG_NCURSES))
  # You may need to set directories to pick up libraries.
  #NCURSES_CFLAGS = -I/usr/5include
  #NCURSES_LIBDIRS = -L/usr/5lib
  NCURSES_LIBS = -lncurses
else
  NCURSES_CFLAGS = $(shell $(PKG_CONFIG) --cflags ncurses)
  NCURSES_LIBDIRS = $(shell $(PKG_CONFIG) --libs-only-L ncurses)
  NCURSES_LIBS = $(shell $(PKG_CONFIG) --libs-only-l --libs-only-other ncurses)
endif

ifneq (0, $(NO_PKG_CONFIG_SDL2))
  # You may need to set directories to pick up libraries.
  #SDL2_CFLAGS =
  #SDL2_LIBDIRS =
  SDL2_LIBS =
else
  SDL2_CFLAGS = $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_mixer)
  SDL2_LIBDIRS = $(shell $(PKG_CONFIG) --libs-only-L sdl2 SDL2_mixer)
  SDL2_LIBS = $(shell $(PKG_CONFIG) --libs-only-l --libs-only-other sdl2 SDL2_mixer)
endif

CFLAGS = $(OPTIONS) $(NCURSES_CFLAGS) $(SDL2_CFLAGS)
LIBDIRS = $(NCURSES_LIBDIRS) $(SDL2_LIBDIRS)
LIBS = $(NCURSES_LIBS) $(SDL2_LIBS)

GLKLIB = libglkterm.a

GLKTERM_OBJS = \
  main.o gtevent.o gtfref.o gtgestal.o gtinput.o \
  gtmessag.o gtmessin.o gtmisc.o gtstream.o gtstyle.o \
  gtw_blnk.o gtw_buf.o gtw_grid.o gtw_pair.o gtwindow.o \
  gtschan.o gtblorb.o cgunicod.o cgdate.o gi_dispa.o gi_blorb.o

GLKTERM_HEADERS = \
  glkterm.h gtoption.h gtw_blnk.h gtw_buf.h \
  gtw_grid.h gtw_pair.h gi_dispa.h

all: $(GLKLIB) Make.glkterm

cgunicod.o: cgunigen.c

$(GLKLIB): $(GLKTERM_OBJS)
	ar r $(GLKLIB) $(GLKTERM_OBJS)
	ranlib $(GLKLIB)

Make.glkterm:
	echo LINKLIBS = $(LIBDIRS) $(LIBS) > Make.glkterm
	echo GLKLIB = -lglkterm >> Make.glkterm

$(GLKTERM_OBJS): glk.h $(GLKTERM_HEADERS)

clean:
	rm -f *~ *.o $(GLKLIB) Make.glkterm
