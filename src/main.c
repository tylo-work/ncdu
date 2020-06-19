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

#include "global.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>
#include <locale.h>

#include <yopt.h>


int pstate;
int read_only = 0;
long update_delay = 100;
int cachedir_tags = 0;
int follow_symlinks = 0;
int follow_firmlinks = 1;
int confirm_quit = -1;

static int min_rows = 17, min_cols = 60;
static int ncurses_init = 0;
static int ncurses_tty = 0; /* Explicitly open /dev/tty instead of using stdio */
static long lastupdate = 999;


static void screen_draw(void) {
  switch(pstate) {
    case ST_CALC:   dir_draw();    break;
    case ST_BROWSE: browse_draw(); break;
    case ST_HELP:   help_draw();   break;
    case ST_SHELL:  shell_draw();  break;
    case ST_DEL:    delete_draw(); break;
    case ST_QUIT:   quit_draw();   break;
  }
}


/* wait:
 *  -1: non-blocking, always draw screen
 *   0: blocking wait for input and always draw screen
 *   1: non-blocking, draw screen only if a configured delay has passed or after keypress
 */
int input_handle(int wait) {
  int ch;
  struct timeval tv;

  if(wait != 1)
    screen_draw();
  else {
    gettimeofday(&tv, NULL);
    tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / update_delay;
    if(lastupdate != tv.tv_usec) {
      screen_draw();
      lastupdate = tv.tv_usec;
    }
  }

  /* No actual input handling is done if ncurses hasn't been initialized yet. */
  if(!ncurses_init)
    return wait == 0 ? 1 : 0;

  nodelay(stdscr, wait?1:0);
  errno = 0;
  while((ch = getch()) != ERR) {
    if(ch == KEY_RESIZE) {
      if(ncresize(min_rows, min_cols))
        min_rows = min_cols = 0;
      /* ncresize() may change nodelay state, make sure to revert it. */
      nodelay(stdscr, wait?1:0);
      screen_draw();
      continue;
    }
    switch(pstate) {
      case ST_CALC:   return dir_key(ch);
      case ST_BROWSE: return browse_key(ch);
      case ST_HELP:   return help_key(ch);
      case ST_DEL:    return delete_key(ch);
      case ST_QUIT:   return quit_key(ch);
    }
    screen_draw();
  }
  if(errno == EPIPE || errno == EBADF || errno == EIO)
      return 1;
  return 0;
}


/* parse command line */
static void argv_parse(int argc, char **argv) {
  yopt_t yopt;
  int v;
  char *val;
  char *export = NULL;
  char *import = NULL;
  char *dir = NULL;

  static yopt_opt_t opts[] = {
    { 'h', 0, "-h,-?,--help" },
    { 'q', 0, "-q" },
    { 'v', 0, "-v,--version" },
    { 'x', 0, "-x" },
    { 'e', 0, "-e" },
    { 'r', 0, "-r" },
    { 'o', 1, "-o" },
    { 'f', 1, "-f" },
    { '0', 0, "-0" },
    { '1', 0, "-1" },
    { '2', 0, "-2" },
    { 'u', 0, "-u" },
    { 'g', 0, "-g" },
    {  1,  1, "--exclude" },
    { 'X', 1, "-X,--exclude-from" },
    { 'L', 0, "-L,--follow-symlinks" },
    { 'C', 0, "--exclude-caches" },
    {  2,  0, "--exclude-kernfs" },
    {  3,  0, "--follow-firmlinks" }, /* undocumented, this behavior is the current default */
    {  4,  0, "--exclude-firmlinks" },
    { 's', 0, "--si" },
    { 'Q', 0, "--confirm-quit" },
    { 'y', 0, "-y" },
    { 'c', 1, "--color" },
    {0,0,NULL}
  };

  dir_ui = -1;
  si = 0;

  yopt_init(&yopt, argc, argv, opts);
  while((v = yopt_next(&yopt, &val)) != -1) {
    switch(v) {
    case  0 : dir = val; break;
    case 'h':
      printf("ncdu <options> <directory>\n");
      printf("  -h,--help                  This help message\n");
      printf("  -q                         Quiet mode, refresh interval 2 seconds\n");
      printf("  -x                         Exclude scanning other file systems\n");
      printf("  -r                         Read only. Disables delete function.\n");
      printf("  -o FILE                    Export scanned directory to FILE\n");
      printf("  -f FILE                    Import scanned directory from FILE\n");
      printf("  -0,-1,-2                   UI to use when scanning (0=none,2=full ncurses)\n");
      printf("  -u,                        Sort user as top-level criteria\n");
      printf("  -g                         Sort group as top-level criteria\n");
      printf("  --si                       Use base 10 (SI) prefixes instead of base 2\n");
      printf("  --exclude PATTERN          Exclude files that match PATTERN\n");
      printf("  -X, --exclude-from FILE    Exclude files that match any pattern in FILE\n");
      printf("  -L, --follow-symlinks      Follow symbolic links (excluding directories)\n");
      printf("  --exclude-caches           Exclude directories containing CACHEDIR.TAG\n");
#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
      printf("  --exclude-kernfs           Exclude Linux pseudo filesystems (procfs,sysfs,cgroup,...)\n");
#endif
#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
      printf("  --exclude-firmlinks        Exclude firmlinks on macOS\n");
#endif
      printf("  --confirm-quit             Confirm quitting ncdu\n");
      printf("  -y                         Quit with no confirm (default on import)\n");
      printf("  --color SCHEME             Set color scheme (off/dark)\n");
      printf("  --version                  Print version\n");
      exit(0);
    case 'q': update_delay = 2000; break;
    case 'v':
      printf("ncdu %s\n", PACKAGE_VERSION);
      exit(0);
    case 'x': dir_scan_smfs = 1; break;
    case 'e': break; // backward comp.
    case 'r': read_only++; break;
    case 's': si = 1; break;
    case 'o': export = val; break;
    case 'f': import = val; break;
    case '0': dir_ui = 0; break;
    case '1': dir_ui = 1; break;
    case '2': dir_ui = 2; break;
    case 'u': dirlist_sort_id = 1; break;
    case 'g': dirlist_sort_id = 2; break;
    case 'Q': confirm_quit = 1; break;
    case 'y': confirm_quit = 0; break;
    case  1 : exclude_add(val); break; /* --exclude */
    case 'X':
      if(exclude_addfile(val)) {
        fprintf(stderr, "Can't open %s: %s\n", val, strerror(errno));
        exit(1);
      }
      break;
    case 'L': follow_symlinks = 1; break;
    case 'C':
      cachedir_tags = 1;
      break;

    case  2 : /* --exclude-kernfs */
#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
      exclude_kernfs = 1; break;
#else
      fprintf(stderr, "This feature is not supported on your platform\n");
      exit(1);
#endif
    case  3 : /* --follow-firmlinks */
#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
      follow_firmlinks = 1; break;
#else
      fprintf(stderr, "This feature is not supported on your platform\n");
      exit(1);
#endif
    case  4 : /* --exclude-firmlinks */
#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
      follow_firmlinks = 0; break;
#else
      fprintf(stderr, "This feature is not supported on your platform\n");
      exit(1);
#endif
    case 'c':
      if(strcmp(val, "off") == 0)  { uic_theme = 0; }
      else if(strcmp(val, "dark") == 0) { uic_theme = 1; }
      else {
        fprintf(stderr, "Unknown --color option: %s\n", val);
        exit(1);
      }
      break;
    case -2:
      fprintf(stderr, "ncdu: %s.\n", val);
      exit(1);
    }
  }

  if(export) {
    if(dir_export_init(export)) {
      fprintf(stderr, "Can't open %s: %s\n", export, strerror(errno));
      exit(1);
    }
    if(strcmp(export, "-") == 0)
      ncurses_tty = 1;
  } else
    dir_mem_init(NULL);

  if(import) {
    if(dir_import_init(import)) {
      fprintf(stderr, "Can't open %s: %s\n", import, strerror(errno));
      exit(1);
    }
    if(strcmp(import, "-") == 0)
      ncurses_tty = 1;
    if (confirm_quit == -1) confirm_quit = 0;
  } else {
    dir_scan_init(dir ? dir : ".");
    if (confirm_quit == -1) confirm_quit = 1;
  }

  /* Use the single-line scan feedback by default when exporting to file, no
   * feedback when exporting to stdout. */
  if(dir_ui == -1)
    dir_ui = export && strcmp(export, "-") == 0 ? 0 : export ? 1 : 2;
}


/* Initializes ncurses only when not done yet. */
static void init_nc(void) {
  int ok = 0;
  FILE *tty;
  SCREEN *term;

  if(ncurses_init)
    return;
  ncurses_init = 1;

  if(ncurses_tty) {
    tty = fopen("/dev/tty", "r+");
    if(!tty) {
      fprintf(stderr, "Error opening /dev/tty: %s\n", strerror(errno));
      exit(1);
    }
    term = newterm(NULL, tty, tty);
    if(term)
      set_term(term);
    ok = !!term;
  } else {
    /* Make sure the user doesn't accidentally pipe in data to ncdu's standard
     * input without using "-f -". An annoying input sequence could result in
     * the deletion of your files, which we want to prevent at all costs. */
    if(!isatty(0)) {
      fprintf(stderr, "Standard input is not a TTY. Did you mean to import a file using '-f -'?\n");
      exit(1);
    }
    ok = !!initscr();
  }

  if(!ok) {
    fprintf(stderr, "Error while initializing ncurses.\n");
    exit(1);
  }

  uic_init();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;
}


void close_nc() {
  if(ncurses_init) {
    erase();
    refresh();
    endwin();
  }
}


/* main program */
int main(int argc, char **argv) {
  read_locale();
  argv_parse(argc, argv);

  if(dir_ui == 2)
    init_nc();

  while(1) {
    /* We may need to initialize/clean up the screen when switching from the
     * (sometimes non-ncurses) CALC state to something else. */
    if(pstate != ST_CALC) {
      if(dir_ui == 1)
        fputc('\n', stderr);
      init_nc();
    }

    if(pstate == ST_CALC) {
      if(dir_process()) {
        if(dir_ui == 1)
          fputc('\n', stderr);
        break;
      }
    } else if(pstate == ST_DEL)
      delete_process();
    else if(input_handle(0))
      break;
  }

  close_nc();
  exclude_clear();

  return 0;
}

