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

#include <ncurses.h>
#include <string.h>


static int page, start;


#define KEYS 23
static const char *keys[KEYS*2] = {
/*|----key----|  |----------------description----------------|*/
      "Up,Down", "Move cursor up or down",
  "Enter/Right", "Open selected directory",
 "Backspc/Left", "Open parent directory",
            "n", "Sort by name (asc./desc.)",
            "s", "Sort by size on disk (asc./desc.)",
            "c", "Sort by item count (asc./desc.)",
            "a", "Sort by access time (asc./desc.)",
            "m", "Sort by modification time",
            "p", "Print report to file in ~/.ncdu/",
            "t", "Toggle modification / access time",
            "u", "Toggle sort user first",
            "g", "Toggle sort group first",
            "f", "Toggle folders first",
            "1", "Toggle 1024/1000 base size units",
            "2", "Toggle percentage and/or graph",
            "3", "Toggle display of child item counts",
            "4", "Toggle display of extended info",
            "x", "Toggle display of excluded/hidden files",
        "Space", "Toggle information about selected item",
     "Delete/d", "Delete selected file or directory",
            "r", "Recalculate the current directory",
            "b", "Spawn shell in current directory",
            "q", "Quit ncdu"
};


#define FLAGS 9
static const char *flags[FLAGS*2] = {
    "!", "An error occurred while reading this directory",
    ".", "An error occurred while reading a subdirectory",
    "<", "File or directory is excluded from the statistics",
    "e", "Empty directory",
    ">", "Directory was on another filesystem",
    "@", "This is not a file nor a dir (symlink, socket, ...)",
    "^", "Excluded Linux pseudo-filesystem",
    "H", "Same file was already counted (hard link)",
    "F", "Excluded firmlink",
};

void help_draw() {
  int i, line;

  browse_draw();

  nccreate(15, 60, "ncdu help");
  ncaddstr(13, 42, "Press ");
  uic_set(UIC_KEY);
  addch('q');
  uic_set(UIC_DEFAULT);
  addstr(" to close");

  nctab(30, page == 1, 1, "Keys");
  nctab(39, page == 2, 2, "Format");
  nctab(50, page == 3, 3, "About");

  switch(page) {
    case 1:
      line = 1;
      for(i=start*2; i<start*2+20; i+=2) {
        uic_set(UIC_KEY);
        ncaddstr(++line, 13-strlen(keys[i]), keys[i]);
        uic_set(UIC_DEFAULT);
        ncaddstr(line, 15, keys[i+1]);
      }
      if(start != KEYS-10)
        ncaddstr(12, 25, "-- more --");
      break;
    case 2:
      attron(A_BOLD);
      ncaddstr(2, 3, "X  [size] [graph] [file or directory]");
      attroff(A_BOLD);
      ncaddstr(3, 4, "The X is only present in the following cases:");
      line = 4;
      for(i=start*2; i<start*2+14; i+=2) {
      uic_set(UIC_FLAG);
        ncaddstr(++line, 4, flags[i]);
      uic_set(UIC_DEFAULT);
        ncaddstr(line, 7, flags[i+1]);
      }
      if(start != FLAGS-7)
        ncaddstr(12, 25, "-- more --");
      break;
    case 3:
      /* Indeed, too much spare time */
      attron(A_REVERSE);
#define x 12
#define y 3
      /* N */
      ncaddstr(y+0, x+0, "      ");
      ncaddstr(y+1, x+0, "  ");
      ncaddstr(y+2, x+0, "  ");
      ncaddstr(y+3, x+0, "  ");
      ncaddstr(y+4, x+0, "  ");
      ncaddstr(y+1, x+4, "  ");
      ncaddstr(y+2, x+4, "  ");
      ncaddstr(y+3, x+4, "  ");
      ncaddstr(y+4, x+4, "  ");
      /* C */
      ncaddstr(y+0, x+8, "     ");
      ncaddstr(y+1, x+8, "  ");
      ncaddstr(y+2, x+8, "  ");
      ncaddstr(y+3, x+8, "  ");
      ncaddstr(y+4, x+8, "     ");
      /* D */
      ncaddstr(y+0, x+19, "  ");
      ncaddstr(y+1, x+19, "  ");
      ncaddstr(y+2, x+15, "      ");
      ncaddstr(y+3, x+15, "  ");
      ncaddstr(y+3, x+19, "  ");
      ncaddstr(y+4, x+15, "      ");
      /* U */
      ncaddstr(y+0, x+23, "  ");
      ncaddstr(y+1, x+23, "  ");
      ncaddstr(y+2, x+23, "  ");
      ncaddstr(y+3, x+23, "  ");
      ncaddstr(y+0, x+27, "  ");
      ncaddstr(y+1, x+27, "  ");
      ncaddstr(y+2, x+27, "  ");
      ncaddstr(y+3, x+27, "  ");
      ncaddstr(y+4, x+23, "      ");
      attroff(A_REVERSE);
      ncaddstr(y+0, x+30, "NCurses");
      ncaddstr(y+1, x+30, "Disk");
      ncaddstr(y+2, x+30, "Usage");
      ncprint( y+4, x+30, "v%s", PACKAGE_VERSION);
      ncaddstr( 9,  7, "Written by Yoran Heling <projects@yorhel.nl>");
      ncaddstr(10,  7, "   https://dev.yorhel.nl/ncdu/");
      ncaddstr(11,  7, "Tyge Lovset <tylo@norceresearch.no>");
      ncaddstr(12,  7, "   https://github.com/tylov/ncdu");
      break;
  }
}


int help_key(int ch) {
  switch(ch) {
    case '1':
    case '2':
    case '3':
      page = ch-'0';
      start = 0;
      break;
    case KEY_RIGHT:
    case KEY_NPAGE:
      if(++page > 3)
        page = 3;
      start = 0;
      break;
    case KEY_LEFT:
    case KEY_PPAGE:
      if(--page < 1)
        page = 1;
      start = 0;
      break;
    case KEY_DOWN:
    case ' ':
      if((page == 1 && start < KEYS-10) || (page == 2 && start < FLAGS-7))
        start++;
      break;
    case KEY_UP:
      if(start > 0)
        start--;
      break;
    default:
      pstate = ST_BROWSE;
  }
  return 0;
}


void help_init() {
  page = 1;
  start = 0;
  pstate = ST_HELP;
}


