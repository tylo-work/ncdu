ncdu v1.16
==========

Description
-----------

  ncdu (NCurses Disk Usage) is a curses-based version of
  the well-known 'du', and provides a fast way to see what
  directories are using your disk space.
  ncdu is forked git://g.blicky.net/ncdu.git
  
Differences from v1.15.x
------------------------

  - Added user and groups columns. Possible to sort by users or groups 
    as the top-level sorting.
  - Added per-user stats (total usage, items) at each directory level.
  - Always shows stats for current user on bottom status line.
  - Added option to save report with list of current view. Report also
    lists stats for each user for the current folder tree.
  - Added parsing of date metadata of imported index file, and shows it
    as [imported YYYY-MM-DD] in upper right corner.
  - Remapped key mapping, and changed many defaults compared to ncdu.
  - Not possible to disable "extended info", also completely removed internally.
  - Added **ncduz** front-end script that eases import/export of gzipped index files.
  
Quick user guide
----------------

  **ncdu** can be executed in three modes, 1) in-memory scan-mode (default),
  2) import-mode, switch -f,  3) export-mode, switch -o. When using import
  or export mode, it is recommended to use **ncduz** front-end script instead,
  because it handles compressed index files directly.
  
  A few important **ncdu** keyboard key functions:
  - Press H to show help screen.
  - Press UP ARROW, DOWN ARROW, PAGE UP, PAGE DOWN to move between items.
  - Press ENTER or RIGHT ARROW to go to selected directory
  - Press BACKSPACE or LEFT ARROW to go to parent directory
  - Press N, M to sort items by name or modification time (twice=toggle direction)
  - Press S, A, C to sort items by size, apparent size, or count
  - Press U, G to do first-order sorting by user or group (twice=toggle off)
  - Press P to print report from current view, including per-user usage.
      
Requirements
------------

  In order to compile and install ncdu, you need to have
  at least...

  - a POSIX-compliant operating system (Linux, BSD, etc)
  - curses libraries and header files


Install
-------

  The usual:

    ./configure --prefix=/usr
    make
    make install

  If you're building directly from the git repository, make sure you have perl
  (or rather, pod2man), pkg-config and GNU autoconf/automake installed, then
  run 'autoreconf -i', and you're ready to continue with the usual ./configure
  and make route.


Copying
-------

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
