/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2020 Yoran Heling

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef _global_h
#define _global_h

#include "config.h"
#include "cvec.h"
#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

/* File Flags (struct dir -> flags) */
#define FF_DIR     0x01
#define FF_FILE    0x02
#define FF_ERR     0x04 /* error while reading this item */
#define FF_OTHFS   0x08 /* excluded because it was another filesystem */
#define FF_EXL     0x10 /* excluded using exclude patterns */
#define FF_SERR    0x20 /* error in subdirectory */
#define FF_HLNKC   0x40 /* hard link candidate (file with st_nlink > 1) */
#define FF_BSEL    0x80 /* selected */
#define FF_EXT    0x100 /* extended struct available */
#define FF_KERNFS 0x200 /* excluded because it was a Linux pseudo filesystem */
#define FF_FRMLNK 0x400 /* excluded because it was a firmlink */

/* Program states */
#define ST_CALC   0
#define ST_BROWSE 1
#define ST_DEL    2
#define ST_HELP   3
#define ST_SHELL  4
#define ST_QUIT   5

struct userdirstats {
    uid_t uid;
    int64_t size;
    int items;
};

#ifndef NOUSERSTATS
using_cvec(usr, struct userdirstats, c_no_compare);
#endif

/* structure representing a file or directory */
struct dir {
  struct dir *parent, *next, *prev, *sub, *hlnk;
  ino_t ino;
  dev_t dev;
  int64_t size;
  int64_t asize;
  int items;
  unsigned short flags;
  unsigned short mode;
  time_t mtime;
  uid_t uid;
  gid_t gid;
#ifndef NOUSERSTATS
  cvec_usr users;
#endif
  char name[];
};

/* A note on the ino and dev fields above: ino is usually represented as ino_t,
 * which POSIX specifies to be an unsigned integer.  dev is usually represented
 * as dev_t, which may be either a signed or unsigned integer, and in practice
 * both are used. dev represents an index / identifier of a device or
 * filesystem, and I'm unsure whether a negative value has any meaning in that
 * context. Hence my choice of using an unsigned integer. Negative values, if
 * we encounter them, will just get typecasted into a positive value. No
 * information is lost in this conversion, and the semantics remain the same.
 */

/* program state */
extern int pstate;
/* read-only flag, 1+ = disable deletion, 2+ = also disable shell */
extern int read_only;
/* minimum screen update interval when calculating, in ms */
extern long update_delay;
/* filter directories with CACHEDIR.TAG */
extern int cachedir_tags;
/* flag if we should ask for confirmation when quitting */
extern int confirm_quit;
/* flag whether we want to follow symlinks */
extern int follow_symlinks;
/* flag whether we want to follow firmlinks */
extern int follow_firmlinks;

/* handle input from keyboard and update display */
int input_handle(int);

/* de-initialize ncurses */
void close_nc(void);


/* import all other global functions and variables */
#include "browser.h"
#include "delete.h"
#include "dir.h"
#include "dirlist.h"
#include "exclude.h"
#include "help.h"
#include "path.h"
#include "util.h"
#include "shell.h"
#include "quit.h"

#endif
