#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gmime/gmime.h>

#include "config.h"
#include "transform.h"
#include "formatters.h"
#include "weft.h"

int main(int argc, char **argv)
{
  int dirn;
  struct stat stat_buf;
  char *file, *output_file_name;

  g_type_init();
  g_mime_init(GMIME_ENABLE_RFC2047_WORKAROUNDS);

  dirn = parse_args(argc, argv);

  compile_filters();
  compile_words();

  if (0) {
    file = "/mnt/var/spool/news/articles/gmane/discuss/4482";
    output_file_name = get_cache_file_name(file);
    ensure_directory(output_file_name);
    transform_file(file, output_file_name);
    free(output_file_name);
  } else {
  
    for ( ; dirn < argc; dirn++) {
      file = argv[dirn];
      if (stat(file, &stat_buf) == -1) {
	perror("weft loop");
	break;
      }

      picon_number = 0;
      binary_number = 0;

      if (S_ISREG(stat_buf.st_mode)) {
	if (override_output_name)
	  output_file_name = override_output_name;
	else
	  output_file_name = get_cache_file_name(file);
	ensure_directory(output_file_name);
	transform_file(file, output_file_name);
	if (! override_output_name)
	  free(output_file_name);
      } else {
	fprintf(stderr, "%s is not a regular file; skipping.\n", file);
	break;
      }
    }
  }
  exit(0);
}   
