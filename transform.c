#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <gmime/gmime.h>
#include <pwd.h>
#include <sys/types.h>

#include "config.h"
#include "formatters.h"
#include "weft.h"

typedef struct {
  FILE *output;
  const char *output_file_name;
} info_parameters;

typedef struct {
  const char *header;
  void (*function)();
} formatter;

formatter wanted_headers[] = {
  {"From", from_formatter},
  {"Subject", subject_formatter},
  {"Newsgroups", newsgroups_formatter},
  {"Date", date_formatter},
#if 0
  {"X-Face", xface_displayer},
  {"Face", face_displayer},
#endif
  {NULL, NULL}};

void transform_text_plain(FILE *output, const char *content, 
			  const char *output_file_name) {
  filter(output, content);
}

void transform_text_html(FILE *output, const char *content, 
			  const char *output_file_name) {
}

void transform_binary(FILE *output, const char *content, 
			   const char *output_file_name) {
}

typedef struct {
  const char *content_type;
  void (*function)();
} transform;

transform part_transforms[] = {
  {"text/plain", transform_text_plain},
  {"text/html", transform_text_html},
  {NULL, NULL}};

void transform_part(GMimePart* part, gpointer oinfo) {
  const GMimeContentType* ct = 0;
  const gchar* content = 0;
  int contentLen = 0;
  int i = 0;
  char content_type[128];
  const char *part_type;
  info_parameters *info = (info_parameters*) oinfo;
  FILE *output = info->output;
  const char *output_file_name = info->output_file_name;

  ct = g_mime_part_get_content_type(part);
  printf("%s/%s\n", ct->type, ct->subtype);
  if (ct == NULL ||
      ct->type == NULL ||
      ct->subtype == NULL)
    strcpy(content_type, "text/plain");
  else
    snprintf(content_type, sizeof(content_type), "%s/%s", 
	     ct->type, ct->subtype);

  content = g_mime_part_get_content(part, &contentLen);

  for (i = 0; ; i++) {
    if ((part_type = part_transforms[i].content_type) == NULL) {
      transform_binary(output, content, output_file_name);
      break;
    } else if (! strcmp(part_type, content_type)) {
      (part_transforms[i].function)(output, content, output_file_name);
      break;
    }
  }
}

void format_file(FILE *output, char *type, const char *value) {
  char *name = text_file(type);
  FILE *file;
  char buffer[1024];

  if ((file = fopen(name, "r")) == NULL) {
    fprintf(stderr, "Couldn't open file %s\n", name);
    perror("weft");
    exit(1);
  }

  while (fgets(buffer, sizeof(buffer), file) != NULL) 
    fprintf(output, buffer, value);

  free(name);
  fclose(file);
}

void output_postamble(FILE *output) {
}

void transform_file(const char *input_file_name, 
		    const char *output_file_name) {
  GMimeStream *stream;
  GMimeMessage *msg = 0;
  int offset;
  int file;
  FILE *output;
  const char *subject;
  InternetAddress *iaddr;
  InternetAddressList *iaddr_list;
  int washed_subject = 0;
  char *address, *at;
  int i;
  const char *header;
  info_parameters info;

  if ((file = open(input_file_name, O_RDONLY)) == -1) {
    printf("Can't open %s\n", input_file_name);
    perror("weft");
    return;
  }

  if (! strcmp(output_file_name, "-")) {
    output = stdout;
  } else {
    if ((output = fopen(output_file_name, "w")) == NULL) {
      printf("Can't open %s\n", output_file_name);
      perror("weft");
      return;
    }
  }

  info.output = output;
  info.output_file_name = output_file_name;

  stream = g_mime_stream_fs_new(file);
  msg = g_mime_parser_construct_message(stream);
  g_mime_stream_unref(stream);

  if (msg != 0) {
    subject = g_mime_message_get_subject(msg);
    format_file(output, "preamble", subject);
    format_file(output, "start_head", subject);

    for (i = 0; (header = wanted_headers[i].header) != NULL; i++) {
      /* Call the formatter function. */
      (wanted_headers[i].function)(output, 
				   g_mime_message_get_header(msg, header),
				   output_file_name);
    }

    format_file(output, "stop_head", subject);
    g_mime_message_foreach_part(msg, transform_part, (gpointer) &info);
    
    g_mime_object_unref(GMIME_OBJECT(msg));

    output_postamble(output);
    fclose(output);
  }
  close(file);
}
