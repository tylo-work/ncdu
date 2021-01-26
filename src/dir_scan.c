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
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
#include <sys/attr.h>
#endif

#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
#include <sys/statfs.h>
#include <linux/magic.h>
#endif


/* set S_BLKSIZE if not defined already in sys/stat.h */
#ifndef S_BLKSIZE
# define S_BLKSIZE 512
#endif


int dir_scan_smfs; /* Stay on the same filesystem */

static uint64_t curdev;   /* current device we're scanning on */

/* scratch space */
static struct dir    *buf_dir = NULL;


#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
int exclude_kernfs; /* Exclude Linux pseudo filesystems */

static int is_kernfs(unsigned long type) {
  if(
#ifdef BINFMTFS_MAGIC
     type == BINFMTFS_MAGIC ||
#endif
#ifdef BPF_FS_MAGIC
     type == BPF_FS_MAGIC ||
#endif
#ifdef CGROUP_SUPER_MAGIC
     type == CGROUP_SUPER_MAGIC ||
#endif
#ifdef CGROUP2_SUPER_MAGIC
     type == CGROUP2_SUPER_MAGIC||
#endif
#ifdef DEBUGFS_MAGIC
     type == DEBUGFS_MAGIC ||
#endif
#ifdef DEVPTS_SUPER_MAGIC
     type == DEVPTS_SUPER_MAGIC ||
#endif
#ifdef PROC_SUPER_MAGIC
     type == PROC_SUPER_MAGIC ||
#endif
#ifdef PSTOREFS_MAGIC
     type == PSTOREFS_MAGIC ||
#endif
#ifdef SECURITYFS_MAGIC
     type == SECURITYFS_MAGIC ||
#endif
#ifdef SELINUX_MAGIC
     type == SELINUX_MAGIC ||
#endif
#ifdef SYSFS_MAGIC
     type == SYSFS_MAGIC ||
#endif
#ifdef TRACEFS_MAGIC
     type == TRACEFS_MAGIC ||
#endif
     0
    )
    return 1;

  return 0;
}
#endif

/* Populates the buf_dir with information from the stat struct.
 * Sets everything necessary for output_dir.item() except FF_ERR and FF_EXL. */
static void stat_to_dir(struct stat *fs) {
  buf_dir->flags |= FF_EXT; /* We always read extended data because it doesn't have an additional cost */
  buf_dir->ino = (uint64_t)fs->st_ino;
  buf_dir->dev = (uint64_t)fs->st_dev;

  if(S_ISREG(fs->st_mode))
    buf_dir->flags |= FF_FILE;
  else if(S_ISDIR(fs->st_mode))
    buf_dir->flags |= FF_DIR;

  if(!S_ISDIR(fs->st_mode) && fs->st_nlink > 1)
    buf_dir->flags |= FF_HLNKC;

  if(dir_scan_smfs && curdev != buf_dir->dev)
    buf_dir->flags |= FF_OTHFS;

  if(!(buf_dir->flags & (FF_OTHFS|FF_EXL|FF_KERNFS))) {
    buf_dir->ds.size = fs->st_blocks * S_BLKSIZE;
  }

  buf_dir->mode = fs->st_mode;
  buf_dir->mtime = fs->st_mtime;
  buf_dir->atime = fs->st_atime;
  buf_dir->ds.uid = fs->st_uid;
  buf_dir->gid = fs->st_gid;
}


/* Reads all filenames in the currently chdir'ed directory and stores it as a
 * nul-separated list of filenames. The list ends with an empty filename (i.e.
 * two nuls). . and .. are not included. Returned memory should be freed. *err
 * is set to 1 if some error occurred. Returns NULL if that error was fatal.
 * The reason for reading everything in memory first and then walking through
 * the list is to avoid eating too many file descriptors in a deeply recursive
 * directory. */
static char *dir_read(int *err) {
  DIR *dir;
  struct dirent *item;
  char *buf = NULL;
  size_t buflen = 512;
  size_t off = 0;

  if((dir = opendir(".")) == NULL) {
    *err = 1;
    return NULL;
  }

  buf = xmalloc(buflen);
  errno = 0;

  while((item = readdir(dir)) != NULL) {
    if(item->d_name[0] == '.' && (item->d_name[1] == 0 || (item->d_name[1] == '.' && item->d_name[2] == 0)))
      continue;
    size_t req = off+3+strlen(item->d_name);
    if(req > buflen) {
      buflen = req < buflen*2 ? buflen*2 : req;
      buf = xrealloc(buf, buflen);
    }
    strcpy(buf+off, item->d_name);
    off += strlen(item->d_name)+1;
  }
  if(errno)
    *err = 1;
  if(closedir(dir) < 0)
    *err = 1;

  buf[off] = 0;
  buf[off+1] = 0;
  return buf;
}


static int dir_walk(char *);


/* Tries to recurse into the current directory item (buf_dir is assumed to be the current dir) */
static int dir_scan_recurse(const char *name) {
  int fail = 0;
  char *dir;

  if(chdir(name)) {
    dir_setlasterr(dir_curpath);
    buf_dir->flags |= FF_ERR;
    if(dir_output.item(buf_dir, name) || dir_output.item(NULL, 0)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    return 0;
  }

  if((dir = dir_read(&fail)) == NULL) {
    dir_setlasterr(dir_curpath);
    buf_dir->flags |= FF_ERR;
    if(dir_output.item(buf_dir, name) || dir_output.item(NULL, 0)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    if(chdir("..")) {
      dir_seterr("Error going back to parent directory: %s", strerror(errno));
      return 1;
    } else
      return 0;
  }

  /* readdir() failed halfway, not fatal. */
  if(fail)
    buf_dir->flags |= FF_ERR;

  if(dir_output.item(buf_dir, name)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }
  fail = dir_walk(dir);
  if(dir_output.item(NULL, 0)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }

  /* Not being able to chdir back is fatal */
  if(!fail && chdir("..")) {
    dir_seterr("Error going back to parent directory: %s", strerror(errno));
    return 1;
  }

  return fail;
}


/* Scans and adds a single item. Recurses into dir_walk() again if this is a
 * directory. Assumes we're chdir'ed in the directory in which this item
 * resides. */
static int dir_scan_item(const char *name) {
  static struct stat st, stl;
  int fail = 0;

#ifdef __CYGWIN__
  /* /proc/registry names may contain slashes */
  if(strchr(name, '/') || strchr(name,  '\\')) {
    buf_dir->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }
#endif

  if(exclude_match(dir_curpath))
    buf_dir->flags |= FF_EXL;

  if(!(buf_dir->flags & (FF_ERR|FF_EXL)) && lstat(name, &st)) {
    buf_dir->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }

#if HAVE_LINUX_MAGIC_H && HAVE_SYS_STATFS_H && HAVE_STATFS
  if(exclude_kernfs && !(buf_dir->flags & (FF_ERR|FF_EXL)) && S_ISDIR(st.st_mode)) {
    struct statfs fst;
    if(statfs(name, &fst)) {
      buf_dir->flags |= FF_ERR;
      dir_setlasterr(dir_curpath);
    } else if(is_kernfs(fst.f_type))
      buf_dir->flags |= FF_KERNFS;
  }
#endif

#if HAVE_SYS_ATTR_H && HAVE_GETATTRLIST && HAVE_DECL_ATTR_CMNEXT_NOFIRMLINKPATH
  if(!follow_firmlinks) {
    struct attrlist list = {
      .bitmapcount = ATTR_BIT_MAP_COUNT,
      .forkattr = ATTR_CMNEXT_NOFIRMLINKPATH,
    };
    struct {
      uint32_t length;
      attrreference_t reference;
      char extra[PATH_MAX];
    } __attribute__((aligned(4), packed)) attributes;
    if (getattrlist(name, &list, &attributes, sizeof(attributes), FSOPT_ATTR_CMN_EXTENDED) == -1) {
      buf_dir->flags |= FF_ERR;
      dir_setlasterr(dir_curpath);
    } else if (strcmp(dir_curpath, (char *)&attributes.reference + attributes.reference.attr_dataoffset))
      buf_dir->flags |= FF_FRMLNK;
  }
#endif

  if(!(buf_dir->flags & (FF_ERR|FF_EXL))) {
    if(follow_symlinks && S_ISLNK(st.st_mode) && !stat(name, &stl) && !S_ISDIR(stl.st_mode))
      stat_to_dir(&stl);
    else
      stat_to_dir(&st);
  }

  if(cachedir_tags && (buf_dir->flags & FF_DIR) && !(buf_dir->flags & (FF_ERR|FF_EXL|FF_OTHFS|FF_KERNFS|FF_FRMLNK)))
    if(has_cachedir_tag(name)) {
      buf_dir->flags |= FF_EXL;
      buf_dir->ds.size = 0;
    }

  /* Recurse into the dir or output the item */
  if(buf_dir->flags & FF_DIR && !(buf_dir->flags & (FF_ERR|FF_EXL|FF_OTHFS|FF_KERNFS|FF_FRMLNK)))
    fail = dir_scan_recurse(name);
  else if(buf_dir->flags & FF_DIR) {
    if(dir_output.item(buf_dir, name) || dir_output.item(NULL, 0)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  } else if(dir_output.item(buf_dir, name)) {
    dir_seterr("Output error: %s", strerror(errno));
    fail = 1;
  }

  return fail || input_handle(1);
}


/* Walks through the directory that we're currently chdir'ed to. *dir contains
 * the filenames as returned by dir_read(), and will be freed automatically by
 * this function. */
static int dir_walk(char *dir) {
  int fail = 0;
  char *cur;

  fail = 0;
  for(cur=dir; !fail&&cur&&*cur; cur+=strlen(cur)+1) {
    dir_curpath_enter(cur);
    memset(buf_dir, 0, offsetof(struct dir, name));
    buf_dir->users = cvec_usr_init();
    fail = dir_scan_item(cur);
    dir_curpath_leave();
  }

  free(dir);
  return fail;
}


static int process(void) {
  char *path;
  char *dir;
  int fail = 0;
  struct stat fs;

  memset(buf_dir, 0, offsetof(struct dir, name));
  buf_dir->users = cvec_usr_init();

  if((path = path_real(dir_curpath)) == NULL)
    dir_seterr("Error obtaining full path: %s", strerror(errno));
  else {
    dir_curpath_set(path);
    free(path);
  }

  if(!dir_fatalerr && path_chdir(dir_curpath) < 0)
    dir_seterr("Error changing directory: %s", strerror(errno));

  /* Can these even fail after a chdir? */
  if(!dir_fatalerr && lstat(".", &fs) != 0)
    dir_seterr("Error obtaining directory information: %s", strerror(errno));
  if(!dir_fatalerr && !S_ISDIR(fs.st_mode))
    dir_seterr("Not a directory");

  if(!dir_fatalerr && !(dir = dir_read(&fail)))
    dir_seterr("Error reading directory: %s", strerror(errno));

  if(!dir_fatalerr) {
    curdev = (uint64_t)fs.st_dev;
    if(fail)
      buf_dir->flags |= FF_ERR;
    stat_to_dir(&fs);

    if(dir_output.item(buf_dir, dir_curpath)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
    if(!fail)
      fail = dir_walk(dir);
    if(!fail && dir_output.item(NULL, 0)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  }

  while(dir_fatalerr && !input_handle(0))
    ;
  return dir_output.final(dir_fatalerr || fail);
}


void dir_scan_init(const char *path) {
  dir_curpath_set(path);
  dir_setlasterr(NULL);
  dir_seterr(NULL);
  dir_process = process;
  if (!buf_dir) {
    buf_dir = xcalloc(1, dir_memsize(""));
    buf_dir->users = cvec_usr_init();
  }
  pstate = ST_CALC;
}
