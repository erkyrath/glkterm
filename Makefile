# Makefile for GlkTerm library and model.c

# Pick a C compiler.
#CC = cc
CC = gcc -ansi

# You may need to set directories to pick up the curses library.
#INCLUDEDIRS = -I/usr/5include
#LIBDIRS = -L/usr/5lib 

OPTIONS = -O
LIBS = -lcurses

CFLAGS = $(OPTIONS) $(INCLUDEDIRS)

GLKTERM_OBJS = \
  main.o gtevent.o gtfref.o gtgestal.o gtinput.o \
  gtmessag.o gtmessin.o gtmisc.o gtstream.o gtstyle.o \
  gtw_blnk.o gtw_buf.o gtw_grid.o gtw_pair.o gtwindow.o  

GLKTERM_HEADERS = \
  glkterm.h gtoption.h gtw_blnk.h gtw_buf.h \
  gtw_grid.h gtw_pair.h

model: $(GLKTERM_OBJS)
	$(CC) -o model model.c $(GLKTERM_OBJS) $(LIBDIRS) $(LIBS)

$(GLKTERM_OBJS): glk.h $(GLKTERM_HEADERS)

clean:
	\rm -f *~ *.o model
