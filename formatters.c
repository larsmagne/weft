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
#include <compface.h>
#include <magick/api.h>

#include "config.h"

typedef struct {
  char *from;
  char *to;
} filter_spec;

filter_spec filters[] = {
  {"<", "&lt;"},
  {">", "&gt;"},
  {"&", "&amp;"},
  {"@", " &lt;at&gt; "},
  {":-)", "<img alt=\":-)\" src=\"/img/smilies/smile.png\">"},
  {";-)", "<img alt=\";-)\" src=\"/img/smilies/blink.png\">"},
  {":-]", "<img alt=\":-]\" src=\"/img/smilies/forced.png\">"},
  {"8-)", "<img alt=\"8-)\" src=\"/img/smilies/braindamaged.png\">"},
  {":-|", "<img alt=\":-|\" src=\"/img/smilies/indifferent.png\">"},
  {":-(", "<img alt=\":-(\" src=\"/img/smilies/sad.png\">"},
  {":-{", "<img alt=\":-{\" src=\"/img/smilies/frown.png\">"},
  {"(-:", "<img alt=\"(-:\" src=\"/img/smilies/reverse-smile.png\">"},
  {0, NULL}
};

void ostring(FILE *output, const char *string) {
  fwrite(string, strlen(string), 1, output);
}

int string_begins (const char *string, char *match) {
  int skip = 0;
  while (*match != 0 && *string != 0 && *string == *match) {
    string++;
    match++;
    skip++;
  }
  if (*match == 0)
    return skip;
  else
    return 0;
}

void filter(FILE *output, const char *string) {
  char c;
  int i, found;
  filter_spec *fs;
  int skip;

  while ((c = *string) != 0) {
    for (i = 0; filters[i].from != 0; i++) {
      fs = &filters[i];
      if ((skip = string_begins(string, fs->from)) != 0) {
	ostring(output, fs->to);
	break;
      } 
    }
    if (! skip) {
      string++;
      fputc(c, output);
    } else {
      string += skip;
    }
  }
}

void from_formatter (FILE *output, const char *from, 
		     const char *output_file_name) {
  InternetAddress *iaddr;
  InternetAddressList *iaddr_list;
  char *address, *name;

  if (from == "")
    from = "Unknown <nobody@nowhere.invalid>";

  fprintf(output, "From: ");

  if ((iaddr_list = internet_address_parse_string(from)) != NULL) {
    iaddr = iaddr_list->address;

    name = iaddr->name;
    if (name == NULL)
      name = "";
    filter(output, name);
    fprintf(output, " ");
    
    internet_address_set_name(iaddr, NULL);
    address = internet_address_to_string(iaddr, FALSE);
    fprintf(output, "&lt;");
    filter(output, address);
    fprintf(output, "&gt;");
    free(address);
    internet_address_list_destroy(iaddr_list);
  } else {
    fprintf(output, "nobody");
  }

  fprintf(output, "<br>\n");
}

void subject_formatter (FILE *output, const char *subject, 
		     const char *output_file_name) {
  ostring(output, "Subject: ");
  filter(output, subject);
  ostring(output, "<br>\n");
}

void newsgroups_formatter (FILE *output, const char *newsgroups, 
		     const char *output_file_name) {
  int len = strlen(newsgroups);
  char *groups = malloc(len + 1);
  char *group, *g, *cgroup;
  int first = 1;

  ostring(output, "Newsgroups: ");

  strcpy(groups, newsgroups);
  group = strtok(groups, ", ");

  do {
    if (! first)
      ostring(output, ", ");

    first = 0;

    ostring(output, "<a href=\"/");
    ostring(output, group);
    ostring(output, "\" target=\"_top\">");
    filter(output, group);
    ostring(output, "</a>");
  } while (group = strtok(NULL, ", "));

  ostring(output, "<br>\n");
  free(groups);
}

void date_formatter (FILE *output, const char *date_string, 
		     const char *output_file_name) {
  ostring(output, "Date: ");
  filter(output, date_string);
  ostring(output, "<br>\n");
}

void write_image(char *png_file_name) {
  ExceptionInfo
    exception;

  Image
    *image,
    *images,
    *resize_image,
    *thumbnails;

  ImageInfo
    *image_info;

  /*
    Initialize the image info structure and read an image.
  */
  InitializeMagick(png_file_name);
  GetExceptionInfo(&exception);
  image_info=CloneImageInfo((ImageInfo *) NULL);
  (void) strcpy(image_info->filename,"image.gif");
  images=ReadImage(image_info,&exception);
  if (exception.severity != UndefinedException)
    CatchException(&exception);
  if (images == (Image *) NULL)
    exit(1);

  /*
    Write the image as MIFF and destroy it.
  */
  (void) strcpy(thumbnails->filename,"xface.png");
  WriteImage(image_info,thumbnails);
  DestroyImageList(thumbnails);
  DestroyImageInfo(image_info);
  DestroyExceptionInfo(&exception);
  DestroyMagick();
}

void xface_displayer (FILE *output, const char *xface, 
		     const char *output_file_name) {
  char *decoded;
  char *suffix = "-xface.pbm";
  char *png_file_name = malloc(strlen(output_file_name) +
			       strlen(suffix) + 1);
  char *dp, *p;
  FILE *png;
  int i;

  if (xface == NULL)
    return;

  decoded = malloc(102400);
  dp = decoded;
  strcpy(decoded, xface);

  sprintf(png_file_name, "%s%s", output_file_name, suffix);
  UnCompAll(decoded);
  UnGenFace();

  /* the compface library exports char F[], which uses a single byte per
     pixel to represent a 48x48 bitmap.  Yuck. */
  
  for (i = 0, p = F; i < (48*48) / 8; ++i) {
    int n, b;
    /* reverse the bit order of each byte... */
    for (b = n = 0; b < 8; ++b) {
      n |= ((*p++) << b);
    }
    *dp++ = (char) n;
  }

  if ((png = fopen(png_file_name, "w")) == NULL) {
    perror("weft face");
    goto out;
  }

  ostring(png, "P4\n48 48\n");
  fwrite(decoded, 48*48/8, 1, png);
  fclose(png);

  ostring(output, "<div class=\"xface\">\n<img src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\">\n</div>\n");

 out:
  free(decoded);
  free(png_file_name);
}

void face_displayer (FILE *output, const char *face, 
		     const char *output_file_name) {
  char *decoded;
  int state = 0, save = 0, ndecoded;
  char *suffix = "-face.png";
  char *png_file_name = malloc(strlen(output_file_name) +
			       strlen(suffix) + 1);
  FILE *png;

  if (face == NULL)
    return;

  decoded = malloc(strlen(face));
  
  sprintf(png_file_name, "%s%s", output_file_name, suffix);
  ndecoded = g_mime_utils_base64_decode_step(face, strlen(face), decoded,
					     &state, &save);

  if ((png = fopen(png_file_name, "w")) == NULL) {
    perror("weft face");
    goto out;
  }

  fwrite(decoded, ndecoded, 1, png);
  fclose(png);

  ostring(output, "<div class=\"face\">\n<img src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\">\n</div>\n");

 out:
  free(decoded);
  free(png_file_name);
}
