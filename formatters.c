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
#include <sys/stat.h>

#include "config.h"
#include "weft.h"

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

void write_xface(char *png_file_name) {
  ExceptionInfo exception;
  Image *image;
  ImageInfo image_info;
  int i;

  /*
    Initialize the image info structure and read an image.
  */

  InitializeMagick(png_file_name);
  GetExceptionInfo(&exception);

  GetImageInfo(&image_info);

  for (i = 0; i < 48 * 48; i++)
    F[i] = (F[i]? 0: 255);

  image=ConstituteImage(48, 48, "A", CharPixel, F, &exception);

  strcpy(image_info.filename, png_file_name);
  strcpy(image->filename, png_file_name);
  strcpy(image->magick, "PNG");

  //DescribeImage(image, stdout, 0);

  WriteImage(&image_info, image);
  DestroyConstitute();
  DestroyImage(image);
  DestroyExceptionInfo(&exception);
  DestroyMagick();
}

void xface_displayer (FILE *output, const char *xface, 
		     const char *output_file_name) {
  char *decoded;
  char *suffix = "-xface.png";
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
  write_xface(png_file_name);
  
  ostring(output, "<img src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\">\n");

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

  ostring(output, "<img src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\">\n");

 out:
  free(decoded);
  free(png_file_name);
}

void image_box_start (FILE *output, const char *dummy, 
		     const char *output_file_name) {
  ostring(output, "<div class=\"face\">\n");
}

void image_box_end (FILE *output, const char *dummy, 
		     const char *output_file_name) {
  ostring(output, "</div>\n");
}

char *reverse_address(char *address) {
  int len = strlen(address), i;
  char *raddress = malloc(len + 1);
  char *p = address + len;
  char *q;

  *raddress = 0;

  for (i = len - 1; i >= 0; i--) {
    if (address[i] == '.' || address[i] == '@') {
      address[i] = 0;
      if (*raddress)
	strcat(raddress, "/");
      strcat(raddress, address + i + 1);
    }
  }

  if (*raddress)
    strcat(raddress, "/");
  strcat(raddress, address + i + 1);

  return raddress;
}

static picon_number = 0;

void output_copy_file(FILE *output, const char *output_file_name,
		      const char *file_name) {
  char *suffix = "-picon-.gif";
  char *gif_file_name;
  int len = strlen(output_file_name) + strlen(suffix) + 1 + 3;
  FILE *in, *out;
  char buffer[4096];
  int count;

  if (picon_number++ > 999)
    return;
  
  gif_file_name = malloc(len);
  snprintf(gif_file_name, len, "%s-picon-%03d.gif", 
	   output_file_name, picon_number);

  ostring(output, "<img src=\"http://cache.gmane.org/");
  uncached_name(output, gif_file_name);
  ostring(output, "\">\n");

  if ((in = fopen(file_name, "r")) == NULL)
    goto out;

  if ((out = fopen(gif_file_name, "w")) == NULL) {
    fclose(in);
    goto out;
  }

  while ((count = fread(buffer, 1, 1, in)) != 0)
    fwrite(buffer, count, 1, out);

  fclose(in);
  fclose(out);

 out:
  free(gif_file_name);
}

/* Return the size of a file. */
loff_t file_size (char *file) {
  struct stat stat_buf;
  if (stat(file, &stat_buf) == -1) {
    return 0;
  }
  return stat_buf.st_size;
}

void from_picon_displayer(FILE *output, const char *from, 
		     const char *output_file_name) {
  InternetAddress *iaddr;
  InternetAddressList *iaddr_list;
  char *address, *raddress;
  char domains[10240], users[10240], *p, file[10240];
  
  if (from == NULL)
    return;

  snprintf(domains, sizeof(domains), "%s/%s/", picon_directory, 
	   "domains");

  snprintf(users, sizeof(users), "%s/%s/", picon_directory, 
	   "users");

  if ((iaddr_list = internet_address_parse_string(from)) != NULL) {
    iaddr = iaddr_list->address;

    internet_address_set_name(iaddr, NULL);
    address = internet_address_to_string(iaddr, FALSE);

    raddress = reverse_address(address);

    strncat(domains, raddress, sizeof(domains));
    strncat(users, raddress, sizeof(users));

    if (strstr(domains, "//") ||
	strstr(domains, ".") ||
	strstr(users, "//") ||
	strstr(users, "."))
      goto out;

    while (strlen(domains) > strlen(picon_directory)) {
      snprintf(file, sizeof(file), "%s%s", domains, "/unknown/face.gif");
      if (file_size(file) != 0) {
	output_copy_file(output, output_file_name, file);
      }
      *strrchr(domains, '/') = 0;
    }

  out:
    free(address);
    free(raddress);
    internet_address_list_destroy(iaddr_list);
  }
}
