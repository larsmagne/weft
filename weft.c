#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <gmime/gmime.h>
#include <sys/types.h>

#include "config.h"
#include "transform.h"
#include "formatters.h"

static char *cache_directory = CACHE_DIRECTORY;
static char *spool_directory = SPOOL_DIRECTORY;
static char *texts_directory = TEXTS_DIRECTORY;
char *picon_directory = PICON_DIRECTORY;
int max_plain_line_length = MAX_PLAIN_LINE_LENGTH;
int picon_number = 0;
int binary_number = 0;

struct option long_options[] = {
  {"cache", 1, 0, 'c'},
  {"help", 1, 0, 'h'},
  {"port", 1, 0, 'p'},
  {0, 0, 0, 0}
};

int parse_args(int argc, char **argv) {
  int option_index = 0, c;
  while (1) {
    c = getopt_long(argc, argv, "hc::", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'c':
      cache_directory = optarg;
      break;

    case 'h':
      printf ("Usage: weft [--cache <directory>] <files...>\n");
      break;

    default:
      break;
    }
  }

  return optind;
}

char *text_file(const char *name) {
  int len = strlen(texts_directory) + strlen(name) + 1 + 1 + 4;
  char *file = malloc(len);
  snprintf(file, len, "%s/%s.txt",  texts_directory, name);
  return file;
}

/* Returns a hierarchical cache file name for file_name. 
   The string returned has to be free()d by the caller. */
char *get_cache_file_name(const char *file_name) {
  int len = strlen(cache_directory) + strlen(file_name) + 1 + 1;
  char *of = malloc(len);
  char *s, *p = spool_directory;
  char c;

  while (*p == *file_name) {
    p++;
    file_name++;
  }
  
  strcpy(of, cache_directory);
  s = of + strlen(cache_directory);

  strcpy(s, file_name);
  while ((c = *s) != 0) {
    if (c == '.')
      *s = '/';
    s++;
  }
  return of;
}

void uncached_name(FILE *output, const char *file_name) {
  char *s = cache_directory;

  while (*s && *file_name && *s == *file_name) {
    s++;
    file_name++;
  }

  ostring(output, file_name);
}

int make_directory (const char *dir) {
  struct stat stat_buf;

  if (stat(dir, &stat_buf) == -1) {
    if (mkdir(dir, 0777) == -1) {
      fprintf(stderr, "Can't create directory %s\n", dir);
      perror("weft");
      return 0;
    } 
    return 1;
  } else if (S_ISDIR(stat_buf.st_mode)) {
    return 1;
  }
  return 0;
}

void ensure_directory(const char *file_name) {
  char *dir = malloc(strlen(file_name) + 1);
  char *s = dir + 1;
  
  strcpy(dir, file_name);
  
  while (*s != 0) {
    while (*s != 0 && *s != '/')
      s++;
    if (*s == '/') {
      *s = 0;
      make_directory(dir);
      *s = '/';
    }
    s++;
  }

  free(dir);
}

