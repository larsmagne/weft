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
#include <regex.h>

#include "config.h"
#include "weft.h"
#include "transform.h"

int start_filter = 0;
static char word_chars[256];

typedef struct {
  char *from;
  char *to;
} string_filter_spec;

typedef struct {
  char from;
  char *to;
} char_filter_spec;

string_filter_spec string_filters[] = {
  {":-)", "<img alt=\":-)\" src=\"/img/smilies/smile.png\">"},
  {";-)", "<img alt=\";-)\" src=\"/img/smilies/blink.png\">"},
  {":-]", "<img alt=\":-]\" src=\"/img/smilies/forced.png\">"},
  {"8-)", "<img alt=\"8-)\" src=\"/img/smilies/braindamaged.png\">"},
  {":-|", "<img alt=\":-|\" src=\"/img/smilies/indifferent.png\">"},
  {":-(", "<img alt=\":-(\" src=\"/img/smilies/sad.png\">"},
  {":-{", "<img alt=\":-{\" src=\"/img/smilies/frown.png\">"},
  {"(-:", "<img alt=\"(-:\" src=\"/img/smilies/reverse-smile.png\">"},
  {NULL, NULL}
};

char_filter_spec char_filters[] = {
  {'<', "&lt;"},
  {'>', "&gt;"},
  {'&', "&amp;"},
  {'@', " &lt;at&gt; "},
  {0, NULL}
};

string_filter_spec regex_filters[] = {
  {"-[a-zA-Z0-9/+]*@public\\.gmane\\.org", "@..."},
  {"\\(\\*[a-zA-Z0-9.]*\\*\\)\\([^a-zA-Z0-9]\\)", "<b>\\1</b>\\2"},
  {"\\(/[a-zA-Z0-9.][a-zA-Z0-9.]*/\\)\\([^a-zA-Z0-9]\\)", "<i>\\1</i>\\2"},
  {"_\\([a-zA-Z0-9.][a-zA-Z0-9.]*\\)\\(_[^a-zA-Z0-9]\\)", "_<u>\\1</u>\\2"},
  {"-\\([a-zA-Z0-9.][a-zA-Z0-9.]*\\)\\(-[^a-zA-Z0-9]\\)", "-<strike>\\1</strike>\\2"},
  {"http://[^ \n\t\"<>()]*", "<a href=\"\\0\" target=\"_top\">\\0</a>"},
  {"www\\.[^ \n\t\"<>()]*", "<a href=\"http://\\0\" target=\"_top\">\\0</a>"},
  {NULL, NULL}
};

typedef struct {
  regex_t from;
  char *to;
} compiled_filter_spec;

static compiled_filter_spec *compiled_filters;

void compile_filters(void) {
  int nfilters = 0, size, i;
  string_filter_spec *filter;
  compiled_filter_spec *cfilter;
  char from[1024], errbuf[1024];
  int errcode;

  for (i = 0; regex_filters[i].from != NULL; i++) 
    nfilters++;

  size = (nfilters + 1) * sizeof(compiled_filter_spec);
  compiled_filters = (compiled_filter_spec*) malloc(size);

  bzero(compiled_filters, size);

  for (i = 0; i < nfilters; i++) {
    filter = &regex_filters[i];
    *from = '^';
    strncpy(from + 1, filter->from, 1023);
    cfilter = &compiled_filters[i];
    if ((errcode = regcomp(&cfilter->from, from, REG_ICASE))
	== 0) 
      cfilter->to = filter->to;
    else {
      regerror(errcode, &cfilter->from, errbuf, sizeof(errbuf));
      printf("%s\n", errbuf);
    }
  }
}

void compile_words(void) {
  int i;
  for (i = 'a'; i <= 'z'; i++) 
    word_chars[i] = 1;
  for (i = 'A'; i <= 'Z'; i++) 
    word_chars[i] = 1;
  for (i = '0'; i <= '9'; i++) 
    word_chars[i] = 1;
}

void ostring(FILE *output, const char *string) {
  fwrite(string, strlen(string), 1, output);
}

void output_quote(FILE *output, const char *string) {
  unsigned char c;
  while ((c = *string++) != 0) {
    if (word_chars[c] || c == '.')
      putc(c, output);
    else if (c == ' ')
      putc('+', output);
    else 
      fprintf(output, "%%%02x", c);
  }
}

int string_begins(const char *string, char *match) {
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
  char c, prev = 0;
  int i;
  compiled_filter_spec *cmfs;
  string_filter_spec *sfs;
  char_filter_spec *cfs;
  int skip = 0;
  regmatch_t pmatch[10];
  char *replace;
  int index;

  while ((c = *string) != 0) {
    skip = 0;

    for (i = start_filter; char_filters[i].from != 0; i++) {
      cfs = &char_filters[i];
      if (c == cfs->from) {
	ostring(output, cfs->to);
	skip = 1;
	goto next;
      } 
    }

    for (i = 0; string_filters[i].from != NULL; i++) {
      sfs = &string_filters[i];
      if (string_begins(string, sfs->from)) {
	ostring(output, sfs->to);
	skip = strlen(sfs->from);
	goto next;
      } 
    }

    if (start_filter == 0) {
      for (i = 0; regex_filters[i].from != NULL; i++) {
	/* Dirty hack to allow the first regexp to follow
	   a word character. */
	if (i > 0 && word_chars[(unsigned int)prev])
	  break;
	cmfs = &compiled_filters[i];
	if (regexec(&cmfs->from, string, 10, pmatch, 0) == 0) {

	  for (replace = cmfs->to; (c = *replace) != 0; replace++) {
	    if (c == '\\' && (index = *(replace + 1)) != 0) {
	      replace++;
	      index -= '0';
	      for (i = pmatch[index].rm_so; i < pmatch[index].rm_eo; i++)
		fputc(*(string + i), output);
	    } else 
	      fputc(*replace, output);
	  }

	  skip = pmatch[0].rm_eo - pmatch[0].rm_so;
	  goto next;
	} 
      }
    }
    
  next:
    if (skip == 0) {
      string++;
      prev = c;
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
  char *address, *name, *kname = NULL;

  if (from == "")
    from = "Unknown <nobody@nowhere.invalid>";

  fprintf(output, "From: ");

  if ((iaddr_list = internet_address_parse_string(from)) != NULL) {
    iaddr = iaddr_list->address;

    name = iaddr->name;
    if (name == NULL)
      name = "";

    if (strrchr(name, ')') == name + strlen(name) - 1) {
      kname = malloc(strlen(name));
      strcpy(kname, name);
      *strrchr(kname, ')') = 0;
      name = kname;
    }

    filter(output, name);
    fprintf(output, " ");
    
    internet_address_set_name(iaddr, NULL);
    address = internet_address_to_string(iaddr, FALSE);
    fprintf(output, "&lt;");
    filter(output, address);
    fprintf(output, "&gt;");
    free(address);
    if (kname)
      free(kname);
    internet_address_list_destroy(iaddr_list);
  } else {
    fprintf(output, "nobody");
  }

  fprintf(output, "<br>\n");
}

void subject_formatter (FILE *output, const char *subject, 
			const char *output_file_name) {
  ostring(output, "Subject: ");
  if (message_id != NULL) {
    ostring(output,
	    "<a target=\"_top\" href=\"http://news.gmane.org/find-root.php?message_id=");
    output_quote(output, message_id);
    ostring(output, "\">");
    filter(output, subject);
    ostring(output, "</a>");
  } else {
    filter(output, subject);
  }
  ostring(output, "<br>\n");
}

void newsgroups_formatter (FILE *output, const char *newsgroups, 
		     const char *output_file_name) {
  int len = strlen(newsgroups);
  char *groups = malloc(len + 1);
  char *group;
  int first = 1;

  ostring(output, "Newsgroups: ");

  strcpy(groups, newsgroups);
  group = strtok(groups, ", ");

  do {
    if (! first)
      ostring(output, ", ");

    first = 0;

    ostring(output, "<a href=\"http://news.gmane.org/");
    output_quote(output, group);
    ostring(output, "\" target=\"_top\">");
    filter(output, group);
    ostring(output, "</a>");
  } while ((group = strtok(NULL, ", ")) != NULL);

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
  char *dp;

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
  
  ostring(output, "<img alt=\"X-Face\" src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\">\n");

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

  ostring(output, "<img alt=\"Face\" src=\"http://cache.gmane.org/");
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

  ostring(output, "<img alt=\"Picon\" src=\"http://cache.gmane.org/");
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
  char *address, *raddress, *p, *user;
  char domains[10240], users[10240], file[10240];
  
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

    strncat(domains, raddress, sizeof(domains) - strlen(raddress));
    strncat(users, raddress, sizeof(users) - strlen(raddress));

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

    while (strstr(users, picon_directory)) {
      snprintf(file, sizeof(file), "%s%s", users, "/face.gif");
      if (file_size(file) != 0) {
	output_copy_file(output, output_file_name, file);
	break;
      }
      user = strrchr(users, '/');
      *user++ = 0;
      for (p = strrchr(users, '/') + 1; *user; )
	*p++ = *user++;
      *p = 0;
    }

  out:
    free(address);
    free(raddress);
    internet_address_list_destroy(iaddr_list);
  }
}
