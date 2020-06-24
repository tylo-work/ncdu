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

/* public variables */
struct dir *dirlist_parent = NULL,
           *dirlist_par    = NULL;
int64_t dirlist_maxs       = 0,
        dirlist_maxa       = 0;

int    dirlist_sort_desc   = 1,
       dirlist_sort_col    = DL_COL_SIZE,
       dirlist_sort_df     = 0,
       dirlist_hidden      = 0,
       dirlist_sort_id     = 0; // uid=1, gid=2

/* private state vars */
static struct dir *parent_alloc, *head, *head_real, *selected, *top = NULL;

struct dir *dirlist_get_head(void) {
    return head;
}


#define ISHIDDEN(d) (dirlist_hidden && (d) != dirlist_parent && (\
    (d)->flags & FF_EXL || (d)->name[0] == '.' || (d)->name[strlen((d)->name)-1] == '~'\
  ))


static inline int cmp_mtime(struct dir *x, struct dir*y) {
  int64_t x_mtime = 0, y_mtime = 0;
  if (x->flags & FF_EXT)
    x_mtime = x->mtime;
  if (y->flags & FF_EXT)
    y_mtime = y->mtime;
  return (x_mtime > y_mtime ? 1 : (x_mtime == y_mtime ? 0 : -1));
}


static inline int cmp_user(struct dir *x, struct dir *y) {
  char x_id[64] = {0}, y_id[64] = {0};
  int xi = x->flags & FF_EXT ? x->uid : 0;
  int yi = y->flags & FF_EXT ? y->uid : 0;
  if (xi == yi) return 0;
  get_username(xi, x_id, 63);
  get_username(yi, y_id, 63);
  return strcmp(x_id, y_id);
}

static inline int cmp_group(struct dir *x, struct dir *y) {
  char x_id[64] = {0}, y_id[64] = {0};
  int xi = x->flags & FF_EXT ? x->gid : 0;
  int yi = y->flags & FF_EXT ? y->gid : 0;
  if (xi == yi) return 0;
  get_groupname(xi, x_id, 63);
  get_groupname(yi, y_id, 63);
  return strcmp(x_id, y_id);
}


static int dirlist_cmp(struct dir *x, struct dir *y) {
  int r = 0;

  #define CMP_MEMB(M) x->M < y->M ? -1 : (x->M > y->M ? 1 : 0)
  #define CMP_EVAL(cmp, direction) r = cmp; if (r != 0) return direction ? -r : r

  if (dirlist_sort_id == 1) {
    CMP_EVAL(cmp_user(x, y), 0);
  }
  else if (dirlist_sort_id == 2) {
    CMP_EVAL(cmp_group(x, y), 0);
  }

  /* dirs are always before files when that option is set */
  if (dirlist_sort_df) {
    if(y->flags & FF_DIR && !(x->flags & FF_DIR))
      return 1;
    else if(!(y->flags & FF_DIR) && x->flags & FF_DIR)
      return -1;
  }  

  /* sort columns:
   *           1   ->   2   ->   3   ->   4
   *   NAME: name  -> size  -> asize -> items
   *   SIZE: size  -> asize -> items -> name
   *  ASIZE: asize -> size  -> items -> name
   *  ITEMS: items -> size  -> asize -> name
   *
   * Note that the method used below is supposed to be fast, not readable :-)
   */

  switch (dirlist_sort_col) {
    case DL_COL_MTIME:
        CMP_EVAL(cmp_mtime(x, y), dirlist_sort_desc);
        CMP_EVAL(CMP_MEMB(size), 0);
        CMP_EVAL(strcmp(x->name, y->name), 0);
        break;
 	case DL_COL_NAME:
 		CMP_EVAL(strcmp(x->name, y->name), !dirlist_sort_desc);
 		break;
 	case DL_COL_SIZE:
 		CMP_EVAL(CMP_MEMB(size), dirlist_sort_desc);
 		CMP_EVAL(CMP_MEMB(items), 0);
 		CMP_EVAL(strcmp(x->name, y->name), 0);
 		break;
 	case DL_COL_ASIZE:
 		CMP_EVAL(CMP_MEMB(asize), dirlist_sort_desc);
 		CMP_EVAL(CMP_MEMB(items), 0);
 		CMP_EVAL(strcmp(x->name, y->name), 0);	
 		break;
  	case DL_COL_ITEMS:
 		CMP_EVAL(CMP_MEMB(items), dirlist_sort_desc);
 		CMP_EVAL(CMP_MEMB(size), 0);
 		CMP_EVAL(strcmp(x->name, y->name), 0);
 		break;
  }

  return 0;
}


static struct dir *dirlist_sort(struct dir *list) {
  struct dir *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

  insize = 1;
  while(1) {
    p = list;
    list = NULL;
    tail = NULL;
    nmerges = 0;
    while(p) {
      nmerges++;
      q = p;
      psize = 0;
      for(i=0; i<insize; i++) {
        psize++;
        q = q->next;
        if(!q) break;
      }
      qsize = insize;
      while(psize > 0 || (qsize > 0 && q)) {
        if(psize == 0) {
          e = q; q = q->next; qsize--;
        } else if(qsize == 0 || !q) {
          e = p; p = p->next; psize--;
        } else if(dirlist_cmp(p,q) <= 0) {
          e = p; p = p->next; psize--;
        } else {
          e = q; q = q->next; qsize--;
        }
        if(tail) tail->next = e;
        else     list = e;
        e->prev = tail;
        tail = e;
      }
      p = q;
    }
    tail->next = NULL;
    if(nmerges <= 1) {
      if(list->parent)
        list->parent->sub = list;
      return list;
    }
    insize *= 2;
  }
}


/* passes through the dir listing once and:
 * - makes sure one, and only one, visible item is selected
 * - updates the dirlist_(maxs|maxa) values
 * - makes sure that the FF_BSEL bits are correct */
static void dirlist_fixup(void) {
  struct dir *t;

  /* we're going to determine the selected items from the list itself, so reset this one */
  selected = NULL;

  for(t=head; t; t=t->next) {
    /* not visible? not selected! */
    if(ISHIDDEN(t))
      t->flags &= ~FF_BSEL;
    else {
      /* visible and selected? make sure only one item is selected */
      if(t->flags & FF_BSEL) {
        if(!selected)
          selected = t;
        else
          t->flags &= ~FF_BSEL;
      }
    }

    /* update dirlist_(maxs|maxa) */
    if(t->size > dirlist_maxs)
      dirlist_maxs = t->size;
    if(t->asize > dirlist_maxa)
      dirlist_maxa = t->asize;
  }

  /* no selected items found after one pass? select the first visible item */
  if(!selected)
    if((selected = dirlist_next(NULL)))
      selected->flags |= FF_BSEL;
}


void dirlist_open(struct dir *d) {
  dirlist_par = d;

  /* set the head of the list */
  head_real = head = d == NULL ? NULL : d->sub;

  /* reset internal status */
  dirlist_maxs = dirlist_maxa = 0;

  /* stop if this is not a directory list we can work with */
  if(d == NULL) {
    dirlist_parent = NULL;
    return;
  }

  /* sort the dir listing */
  if(head)
    head_real = head = dirlist_sort(head);

  /* set the reference to the parent dir */
  if(d->parent) {
    if(!parent_alloc)
      parent_alloc = xcalloc(1, dir_memsize(".."));
    dirlist_parent = parent_alloc;
    strcpy(dirlist_parent->name, "..");
    dirlist_parent->next = head;
    dirlist_parent->parent = d;
    dirlist_parent->sub = d;
    dirlist_parent->flags = FF_DIR;
    head = dirlist_parent;
  } else
    dirlist_parent = NULL;

  dirlist_fixup();
}


struct dir *dirlist_next(struct dir *d) {
  if(!head)
    return NULL;
  if(!d) {
    if(!ISHIDDEN(head))
      return head;
    else
      d = head;
  }
  while((d = d->next)) {
    if(!ISHIDDEN(d))
      return d;
  }
  return NULL;
}


static struct dir *dirlist_prev(struct dir *d) {
  if(!head || !d)
    return NULL;
  while((d = d->prev)) {
    if(!ISHIDDEN(d))
      return d;
  }
  if(dirlist_parent)
    return dirlist_parent;
  return NULL;
}


struct dir *dirlist_get(int i) {
  struct dir *t = selected, *d;

  if(!head)
    return NULL;

  if(ISHIDDEN(selected)) {
    selected = dirlist_next(NULL);
    return selected;
  }

  /* i == 0? return the selected item */
  if(!i)
    return selected;

  /* positive number? simply move forward */
  while(i > 0) {
    d = dirlist_next(t);
    if(!d)
      return t;
    t = d;
    if(!--i)
      return t;
  }

  /* otherwise, backward */
  while(1) {
    d = dirlist_prev(t);
    if(!d)
      return t;
    t = d;
    if(!++i)
      return t;
  }
}


void dirlist_select(struct dir *d) {
  if(!d || !head || ISHIDDEN(d) || d->parent != head->parent)
    return;

  selected->flags &= ~FF_BSEL;
  selected = d;
  selected->flags |= FF_BSEL;
}



/* We need a hint in order to figure out which item should be on top:
 *  0 = only get the current top, don't set anything
 *  1 = selected has moved down
 * -1 = selected has moved up
 * -2 = selected = first item in the list (faster version of '1')
 * -3 = top should be considered as invalid (after sorting or opening another dir)
 * -4 = an item has been deleted
 * -5 = hidden flag has been changed
 *
 * Actions:
 *  hint = -1 or -4 -> top = selected_is_visible ? top : selected
 *  hint = -2 or -3 -> top = selected-(winrows-3)/2
 *  hint =  1       -> top = selected_is_visible ? top : selected-(winrows-4)
 *  hint =  0 or -5 -> top = selected_is_visible ? top : selected-(winrows-3)/2
 *
 * Regardless of the hint, the returned top will always be chosen such that the
 * selected item is visible.
 */
struct dir *dirlist_top(int hint) {
  struct dir *t;
  int i, visible = 0;

  if(hint == -2 || hint == -3)
    top = NULL;

  /* check whether the current selected item is within the visible window */
  if(top) {
    i = winrows-3;
    t = dirlist_get(0);
    while(t && i--) {
      if(t == top) {
        visible++;
        break;
      }
      t = dirlist_prev(t);
    }
  }

  /* otherwise, get a new top */
  if(!visible)
    top = hint == -1 || hint == -4 ? dirlist_get(0) :
          hint ==  1               ? dirlist_get(-1*(winrows-4)) :
                                     dirlist_get(-1*(winrows-3)/2);

  /* also make sure that if the list is longer than the window and the last
   * item is visible, that this last item is also the last on the window */
  t = top;
  i = winrows-3;
  while(t && i--)
    t = dirlist_next(t);
  t = top;
  do {
    top = t;
    t = dirlist_prev(t);
  } while(t && i-- > 0);

  return top;
}


void dirlist_set_sort(int col, int desc, int df) {
  /* update config */
  if(col != DL_NOCHANGE)
    dirlist_sort_col = col;
  if(desc != DL_NOCHANGE)
    dirlist_sort_desc = desc;
  if(df != DL_NOCHANGE)
    dirlist_sort_df = df;

  /* sort the list (excluding the parent, which is always on top) */
  if(head_real)
    head_real = dirlist_sort(head_real);
  if(dirlist_parent)
    dirlist_parent->next = head_real;
  else
    head = head_real;
  dirlist_top(-3);
}


void dirlist_set_hidden(int hidden) {
  dirlist_hidden = hidden;
  dirlist_fixup();
  dirlist_top(-5);
}

