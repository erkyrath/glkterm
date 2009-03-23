GlkTerm: Curses.h Implementation of the Glk API.

GlkTerm Library: version 0.1 alpha.
Glk API which this implements: version 0.3.
Designed by Andrew Plotkin <erkyrath@netcom.com>
http://www.edoc.com/zarf/glk/index.html

This is source code for an implementation of the Glk library which runs
in a terminal window, using the curses.h library for screen control.
Curses.h (no relation to the Meldrews) should be available on all Unix
systems. I don't know whether it's available under DOS/Windows, but I
trust someone will tell me.

I will try to incorporate options for as many OSes as I can, using those
ugly #ifdefs. So if you get this to compile under some OS not listed
below, send me mail and tell me how it went.

This source code is not directly applicable to any other display system.
Curses library calls are scattered all through the code; I haven't tried
to abstract them out. If you want to create a Glk library for a
different display system, you'd best start over. Use GlkTerm for a
starting point for terminal-window-style display systems, and MacGlk for
graphical/windowed display systems. (Except that you shouldn't do either
until the Glk API is finalized.) (Besides, this is still an alpha
release, and I haven't released MacGlk yet. Sigh.)

* Command-line arguments:

At the moment GlkTerm only accepts command-line options for itself, not
for whatever program runs under the Glk API. This will be fixed
eventually.

    -width NUM, -height NUM: These set the screen width and height
manually. Normally GlkTerm determines the screen size itself, by asking
the curses library. If this doesn't work, you can set a fixed size using
these options.
    -ml BOOL: Use message line (default "yes"). Normally GlkTerm
reserves the bottom line of the screen for special messages. By setting
this to "no", you can free that space for game text. Note that some
operations will grab the bottom line temporarily anyway.
    -revgrid BOOL: Reverse text in grid (status) windows (default "no").
Set this to "yes" to display all textgrid windows (status windows) in
reverse text.
    -border BOOL: Draw one-character borders between windows (default
"yes"). These are lines of '-' and '|' characters. If you set this "no",
there's a little more room for game text, but it may be hard to
distinguish windows. The -revgrid option may help.
    -precise BOOL: More precise timing for timed input (default "no").
The curses.h library only provides timed input in increments of a tenth
of a second. So Glk timer events will only be checked ten times a
second, even on fast machines. If this isn't good enough, you can try
setting this option to "yes"; then timer events will be checked
constantly. This busy-spins the CPU, probably slowing down everything
else on the machine, so use it only when necessary. For that matter, it
may not even work on all OSes. (If GlkTerm is compiled without support
for timed input, this option will be removed.)
    -version: Display Glk library version.
    -help: Display list of command-line options.
    
NUM values can be any number. BOOL values can be "yes" or "no", or no
value to toggle.

Future versions of GlkTerm will have options to control display styles,
window border styles, and maybe other delightful things.

* Notes on building this mess:

There are a few compile-time options. These are defined in gtoption.h.
Before you compile, you should go into gtoption.h and make any changes
necessary. You may also need to edit some include and library paths in
the Makefile.

* Operating systems which this has been compiled on:

SunOS:
    Uncomment the lines in the Makefile:
        INCLUDEDIRS = -I/usr/5include
        LIBDIRS = -L/usr/5lib
    #define NO_MEMMOVE in gtoption.h
    
IRIX:
    Compiles as is

* Notes on the source code:

Functions which begin with glk_ are, of course, Glk API functions. These
are declared in glk.h, which is not included in this package. Make sure
glk.h is available before you try to compile this stuff.

Functions which begin with gli_ are internal to the GlkTerm library
implementation. They don't exist in every Glk library, because different
libraries implement things in different ways. (In fact, they may be
declared differently, or have different meanings, in different Glk
libraries.) These gli_ functions (and other internal constants and
structures) are declared in glkterm.h.

As you can see from the code, I've kept a policy of catching every error
that I can possibly catch, and printing visible warnings.

The 32-bit integer identifiers used by the game program (winid_t,
strid_t, frefid_t) are created by casting structure pointers to uint32.
See the beginning of glkterm.h for the macros that handle this. If your
system uses pointers larger than 32 bits, or if there's some wackiness
that prevents you casting pointers to numbers and back, you'll have to
create some other identifier system. (A hash table would work fine.)
(Really I should make this a compile-time option; maybe later. GlkTerm
is only alpha at the moment, after all.)

Other than that, this code should be portable to any C environment which
has an ANSI stdio library and a curses.h library. The likely trouble
spots are glk_fileref_delete_file() and glk_fileref_does_file_exist() --
I've implemented them with the Unix calls unlink() and stat()
respectively. glk_fileref_create_by_prompt() also contains a call to
stat(), to implement a "Do you want to overwrite that file?" prompt. 

I have not yet tried to deal with character-set issues. The library
assumes that all input and output characters are in Latin-1. Alpha, like
I said.

* Permissions

The source code in this package is copyright 1998 by Andrew Plotkin. You
may copy and distribute it freely, by any means and under any
conditions, as long as the code and documentation is not changed. You
may also modify this code and incorporate it into your own programs, as
long as you retain a notice in your program or documentation which
mentions my name and the URL shown above.

