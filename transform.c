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
#include <ctype.h>

#include "config.h"
#include "formatters.h"
#include "weft.h"
#include "striphtml.h"

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
  {"Data", image_box_start},
  {"X-Face", xface_displayer},
  {"Face", face_displayer},
  {"From", from_picon_displayer},
  {"Data", image_box_end},
  {NULL, NULL}};

char *preferred_alternatives[] = {"text/html", "text/plain", NULL};

char *message_id = NULL;

void limit_line_lengths(char *content) {
  int column = 0;
  char *space = NULL;
  char c;
  
  while ((c = *content) != 0) {
    if (c == ' ') {
      if (column > max_plain_line_length && space != NULL) {
	*space = '\n';
	column = content - space;
      }
      space = content;
    } else if (c == '\n')
      column = 0;
    else
      column++;

    content++;
  }
}

void transform_text_plain(FILE *output, char *content, 
			  const char *output_file_name) {
  if (content != NULL) {
    ostring(output, "<pre>\n");
    limit_line_lengths(content);
    filter(output, content);
    ostring(output, "</pre>\n");
  }
}

void transform_text_html(FILE *output, const char *content, 
			  const char *output_file_name) {
  char *clean;
  int length;

  ostring(output, "<p>\n");
  start_filter = 3;
  clean = stripHtml(content, strlen(content), &length);
  *(clean+length) = 0;
  filter(output, clean);
  start_filter = 0;

  free(clean);
}

void transform_binary(FILE *output, const char *content, 
		      int content_length, const char *attachment_name,
		      const char *content_type,
		      const char *output_file_name) {
  char *suffix = "-.bin";
  int len = strlen(output_file_name) + strlen(suffix) + 1 + 3;
  char *bin_file_name = malloc(len);
  FILE *bin;
  
  if (binary_number++ > 999)
    goto out;

  snprintf(bin_file_name, len, "%s-%03d.bin", 
	   output_file_name, binary_number);
  
  if ((bin = fopen(bin_file_name, "w")) == NULL)
    goto out;

  fwrite(content, content_length, 1, bin);
  fclose(bin);

  if (strstr(content_type, "image/")) {
    ostring(output, "<div class=\"imgattachment\"><img src=\"http://cache.gmane.org/");
    uncached_name(output, bin_file_name);
    ostring(output, "\"></div>\n");
  } else {
    ostring(output, "<div class=\"attachment\"><a href=\"http://cache.gmane.org/");
    uncached_name(output, bin_file_name);
    ostring(output, "\">Attachment");
    if (attachment_name != NULL) 
      fprintf(output, " (%s)", attachment_name);
    fprintf(output, "</a>: %s, ", content_type);
    if (content_length < 8*1024) 
      fprintf(output, "%d bytes", content_length);
    else
      fprintf(output, "%d KiB", content_length / 1024);
    ostring(output, "</div>\n");
  }

 out:
  free(bin_file_name);
  
}

typedef struct {
  const char *content_type;
  void (*function)();
} transform;

transform part_transforms[] = {
  {"text/plain", transform_text_plain},
  {"text/html", transform_text_html},
  {NULL, NULL}};

char *convert_to_utf8(char *string, const char *charset) {
  const char *utf8, *local;
  iconv_t local_to_utf8;
  char *result;

  g_mime_charset_init();
	
  utf8 = g_mime_charset_name("utf-8");
  local = g_mime_charset_name(charset);
  local_to_utf8 = iconv_open(utf8, local);

  result = g_mime_iconv_strdup(local_to_utf8, string);

  return result;
}

void transform_simple_part(FILE *output, const char *output_file_name,
			   GMimePart* part) {
  const GMimeContentType* ct = 0;
  const gchar* content = 0;
  int contentLen = 0;
  int i = 0;
  char content_type[128];
  const char *part_type;
  char *mcontent, *p, *ccontent;
  const char *charset = NULL;

  ct = g_mime_part_get_content_type(part);

  if (ct == NULL ||
      ct->type == NULL ||
      ct->subtype == NULL) {
    strcpy(content_type, "text/plain");
  } else {
    charset = g_mime_content_type_get_parameter(ct, "charset");
    snprintf(content_type, sizeof(content_type), "%s/%s", 
	     ct->type, ct->subtype);
  }

  if (charset == NULL)
    charset = "iso-8859-1";

  for (p = content_type; *p; p++) 
    *p = tolower(*p);

  content = g_mime_part_get_content(part, &contentLen);
  /* We copy over the content and zero-terminate it. */
  mcontent = malloc(contentLen + 1);
  memcpy(mcontent, content, contentLen);
  *(mcontent + contentLen) = 0;


  if (strcmp(charset, "utf-8")) 
    ccontent = convert_to_utf8(mcontent, charset);
  else
    ccontent = mcontent;

  for (i = 0; ; i++) {
    if ((part_type = part_transforms[i].content_type) == NULL) {
      transform_binary(output, mcontent, contentLen,
		       g_mime_part_get_filename(part),
		       content_type, output_file_name);
      break;
    } else if (! strcmp(part_type, content_type)) {
      (part_transforms[i].function)(output, 
				    (ccontent? ccontent: mcontent),
				    output_file_name);
      break;
    }
  }

  free(mcontent);
  if (ccontent != NULL)
    free(ccontent);
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

void transform_part (FILE *output, const char *output_file_name,
		     GMimePart *mime_part) {
  const GMimeContentType* ct = 0;

  if (mime_part->children) {
    GList *child;
    GMimePart *preferred = NULL;
    char *type, *subtype = NULL;

    ct = g_mime_part_get_content_type(mime_part);

    if (ct != NULL) 
      subtype = ct->subtype;

    if (subtype == NULL)
      subtype = "mixed";

    if (! strcmp(subtype, "alternative")) {
      /* This is multipart/alternative, so we need to decide which
	 part to output. */
      
      child = mime_part->children;
      while (child) {
	ct = g_mime_part_get_content_type(child->data);
	if (ct == NULL) {
	  type = "text";
	  subtype = "plain";
	} else {
	  type = ct->type? ct->type: "text";
	  subtype = ct->subtype? ct->subtype: "plain";
	}
	  
	if (! strcmp(type, "multipart") ||
	    ! strcmp(type, "message")) 
	  preferred = child->data;
	else if (! strcmp(type, "text")) {
	  if (! strcmp(subtype, "html"))
	    preferred = child->data;
	  else if (! strcmp(subtype, "plain") && preferred == NULL)
	    preferred = child->data;
	}
	child = child->next;
      }

      if (! preferred) {
	/* Use the last child as the preferred. */
	child = mime_part->children;
	while (child) {
	  preferred = child->data;
	  child = child->next;
	}
      }

      transform_part(output, output_file_name, preferred);

    } else {
      /* Multipart mixed and related. */
      child = mime_part->children;
      while (child) {
	transform_part(output, output_file_name, 
		       (GMimePart *) child->data);
	child = child->next;
      }
    }
  } else {
    transform_simple_part(output, output_file_name, mime_part);
  }
}

void transform_message (FILE *output, const char *output_file_name,
			GMimeMessage *msg) {
  int i;
  const char *header;
  const char *value;
  
  format_file(output, "start_head", "");

  message_id = g_mime_message_get_header(msg, "Message-ID");

  for (i = 0; (header = wanted_headers[i].header) != NULL; i++) {
    if (! strcmp(header, "Fromm")) /* This test is never true. */
      value = g_mime_message_get_sender(msg);
    else if (! strcmp(header, "Subject"))
      value = g_mime_message_get_subject(msg);
    else
      value = g_mime_message_get_header(msg, header);
    /* Call the formatter function. */
    (wanted_headers[i].function)(output, 
				 value,
				 output_file_name);
  }
  
  format_file(output, "stop_head", "");

  transform_part(output, output_file_name, msg->mime_part);
}


void transform_file(const char *input_file_name, 
		    const char *output_file_name) {
  GMimeStream *stream;
  GMimeMessage *msg = 0;
  int file;
  FILE *output;
  char *subject, *s, c;
  const char *gsubject;

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

  stream = g_mime_stream_fs_new(file);
  msg = g_mime_parser_construct_message(stream);
  g_mime_stream_unref(stream);

  if (msg != 0) {
    gsubject = g_mime_message_get_subject(msg);
    subject = malloc(strlen(gsubject) + 1);
    strcpy(subject, gsubject);

    format_file(output, "preamble", subject);

    transform_message(output, output_file_name, msg);
    
    /* FIXME */
    g_mime_object_unref(GMIME_OBJECT(msg));

    s = subject;
    while ((c = *s) != 0) {
      if (! ((c >= '0' && c <= '9') ||
	     (c >= 'a' && c <= 'z') ||
	     (c >= 'A' && c <= 'Z') ||
	     c == ':' || c == '.' || c == '?'))
	*s = ' ';
      s++;
    }
    format_file(output, "postamble", subject);
    fclose(output);
  }
  close(file);
}
