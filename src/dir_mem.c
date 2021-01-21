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

#include <string.h>
#include <stdlib.h>
#include "cmap.h"


static struct dir *root;   /* root directory struct we're scanning */
static struct dir *curdir; /* directory item that we're currently adding items to */
static struct dir *orig;   /* original directory, when refreshing an already scanned dir */

/* Table of struct dir items with more than one link (in order to detect hard links) */
#define hlink_equals(a, b) ((a)[0]->dev == (b)[0]->dev && (a)[0]->ino == (b)[0]->ino)
using_cset(hl, struct dir *, hlink_equals, c_default_hash32);
static cset_hl links = cset_inits;


/* recursively checks a dir structure for hard links and fills the lookup array */
static void hlink_init(struct dir *d) {
  struct dir *t;

  for(t=d->sub; t!=NULL; t=t->next)
    hlink_init(t);

  if(!(d->flags & FF_HLNKC))
    return;
  cset_hl_insert(&links, d);
}


/* checks an individual file for hard links and updates its cicrular linked
 * list, also updates the sizes of the parent dirs */
static void hlink_check(struct dir *d) {
  struct dir *t, *pt, *par;

  /* add to links table */
  cset_hl_result_t res = cset_hl_insert(&links, d);

  /* found in the table? update hlnk */
  if (!res.second) {
    t = *res.first;
    d->hlnk = t->hlnk == NULL ? t : t->hlnk;
    t->hlnk = d;
  }

  /* now update the sizes of the parent directories,
   * This works by only counting this file in the parent directories where this
   * file hasn't been counted yet, which can be determined from the hlnk list.
   * XXX: This may not be the most efficient algorithm to do this */
  int i;
  for(i=1,par=d->parent; i&&par; par=par->parent) {
    if(d->hlnk)
      for(t=d->hlnk; i&&t!=d; t=t->hlnk)
        for(pt=t->parent; i&&pt; pt=pt->parent)
          if(pt==par)
            i=0;
    if(i) {
      add_dirstats(par, d->uid, d->size, 0);
    }
  }
}


/* Add item to the correct place in the memory structure */
static void item_add(struct dir *item) {
  if(!root) {
    root = item;
    /* Make sure that the *root appears to be part of the same dir structure as
     * *orig, otherwise the directory size calculation will be incorrect in the
     * case of hard links. */
    if(orig)
      root->parent = orig->parent;
  } else {
    item->parent = curdir;
    item->next = curdir->sub;
    if(item->next)
      item->next->prev = item;
    curdir->sub = item;
  }
}


static int item(struct dir *dir, const char *name) {
  struct dir *t, *item;

  /* Go back to parent dir */
  if(!dir) {
    curdir = curdir->parent;
    return 0;
  }

  if(!root && orig)
    name = orig->name;

  item = xcalloc(1, dir_memsize(name));
  memcpy(item, dir, offsetof(struct dir, name));
  strcpy(item->name, name);

  item_add(item);

  /* Ensure that any next items will go to this directory */
  if(item->flags & FF_DIR)
    curdir = item;

  /* Special-case the name of the root item to be empty instead of "/". This is
   * what getpath() expects. */
  if(item == root && strcmp(item->name, "/") == 0)
    item->name[0] = 0;

  /* Update stats of parents. Don't update the size fields if this is a
   * possible hard link, because hlnk_check() will take care of it in that
   * case. */
  if(item->flags & FF_HLNKC) {
    addparentstats(item->parent, item->uid, 0, 0, 0, 1);
    hlink_check(item);
  } else {
    addparentstats(item->parent, item->uid, item->size, item->atime, item->mtime, 1);
  }

  /* propagate ERR and SERR back up to the root */
  if(item->flags & FF_SERR || item->flags & FF_ERR)
    for(t=item->parent; t; t=t->parent)
      t->flags |= FF_SERR;

  dir_output.size = root->size;
  dir_output.items = root->items;

  return 0;
}


static int final(int fail) {
  cset_hl_del(&links);
  links = cset_hl_init(); // not needed..

  if(fail) {
    freedir(root);
    if(orig) {
      browse_init(orig);
      return 0;
    } else
      return 1;
  }

  /* success, update references and free original item */
  if(orig) {
    root->next = orig->next;
    root->prev = orig->prev;
    if(root->parent && root->parent->sub == orig)
      root->parent->sub = root;
    if(root->prev)
      root->prev->next = root;
    if(root->next)
      root->next->prev = root;
    orig->next = orig->prev = NULL;
    freedir(orig);
  }

  browse_init(root);
  dirlist_top(-3);
  return 0;
}


void dir_mem_init(struct dir *_orig) {
  orig = _orig;
  root = curdir = NULL;
  pstate = ST_CALC;

  dir_output.item = item;
  dir_output.final = final;
  dir_output.size = 0;
  dir_output.items = 0;

  /* Init hash table for hard link detection */
  links = cset_hl_init();
  if(orig)
    hlink_init(getroot(orig));
}

