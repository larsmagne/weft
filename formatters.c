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
#include <gcrypt.h>

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
  {":-)", "<img alt=\":-)\" src=\"http://news.gmane.org/img/smilies/smile.png\">"},
  {";-)", "<img alt=\";-)\" src=\"http://news.gmane.org/img/smilies/blink.png\">"},
  {":-]", "<img alt=\":-]\" src=\"http://news.gmane.org/img/smilies/forced.png\">"},
  {"8-)", "<img alt=\"8-)\" src=\"http://news.gmane.org/img/smilies/braindamaged.png\">"},
  {":-|", "<img alt=\":-|\" src=\"http://news.gmane.org/img/smilies/indifferent.png\">"},
  {":-(", "<img alt=\":-(\" src=\"http://news.gmane.org/img/smilies/sad.png\">"},
  {":-{", "<img alt=\":-{\" src=\"http://news.gmane.org/img/smilies/frown.png\">"},
  {"(-:", "<img alt=\"(-:\" src=\"http://news.gmane.org/img/smilies/reverse-smile.png\">"},
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
  {"\\([-a-zA-Z0-9/+._]*\\)-[a-zA-Z0-9/+]*@public\\.gmane\\.org", "<a target=\"_top\" href=\"http://gmane.org/get-address.php?address=\\0\">\\1@...</a>"},
  {"\\(\\*[a-zA-Z0-9.]*\\*\\)\\([^a-zA-Z0-9]\\)", "<b>\\1</b>\\2"},
  {"\\(/[a-zA-Z0-9.][a-zA-Z0-9.]*/\\)\\([^a-zA-Z0-9]\\)", "<i>\\1</i>\\2"},
  {"_\\([a-zA-Z0-9.][a-zA-Z0-9.]*\\)\\(_[^a-zA-Z0-9]\\)", "_<u>\\1</u>\\2"},
  {"-\\([a-zA-Z0-9.][a-zA-Z0-9.]*\\)\\(-[^a-zA-Z0-9]\\)", "-<strike>\\1</strike>\\2"},
  {"http://[^ \n\t\"<>()]*", "<a href=\"\\0\" target=\"_top\">\\0</a>"},
  {"www\\.[^ \n\t\"<>()]*", "<a href=\"http://\\0\" target=\"_top\">\\0</a>"},
  {"\n *\n\\( *\n\\)*", "\n\n"},
  {NULL, NULL}
};

typedef struct {
  regex_t from;
  char *to;
} compiled_filter_spec;

static compiled_filter_spec *compiled_filters;

/* The same as malloc, but returns a char*, and clears the memory. */
char *cmalloc(int size) {
  char *b = (char*)malloc(size);
  bzero(b, size);
  return b;
}

char *base64_decode(const char *encoded) {
  char *decoded = malloc(strlen(encoded + 1));
  int state = 0, save = 0;

  g_mime_utils_base64_decode_step(encoded, strlen(encoded), decoded,
				  &state, &save);

  return decoded;
}

char *get_key(void) {
  FILE *file;
  char buffer[1024];
  char *key = NULL;
  char *p;

  if ((file = fopen(text_file("key"), "r")) != NULL) {
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
      if ((p = strchr(buffer, '\n')) != NULL)
	*p = 0;
      key = malloc(strlen(buffer) + 1);
      strcpy(key, buffer);
    }
    fclose(file);
  }

  return key;
}

char *decrypt(const char *encrypted, int length) {
  GCRY_CIPHER_HD context;
  char *decrypted = NULL;
  char *key = get_key();
  int res;

  if (key == NULL)
    return NULL;

  decrypted = cmalloc(length + 1);

  context = gcry_cipher_open(GCRY_CIPHER_BLOWFISH, GCRY_CIPHER_MODE_CBC, 0);
  if (context == NULL)
    return NULL;

  res = gcry_cipher_setkey(context, key, strlen(key));
  if (res == GCRYERR_SUCCESS)
    res = gcry_cipher_setiv (context, NULL, 0);
  
  if (res != GCRYERR_SUCCESS) {
    printf("Error %d\n", res);
    return NULL;
  }

  gcry_cipher_decrypt(context, decrypted, length, encrypted, length);
  if (res != GCRYERR_SUCCESS) {
    printf("Error %d\n", res);
    return NULL;
  }

  gcry_cipher_close(context);
  free(key);
  return decrypted; 
}

char *encrypt(const char *decrypted) {
  GCRY_CIPHER_HD context;
  char *encrypted = cmalloc(strlen(decrypted) + 1);
  char *key = get_key();
  int res;
  int length = strlen(decrypted);

  if (key == NULL)
    return NULL;
  
  context = gcry_cipher_open(GCRY_CIPHER_BLOWFISH, GCRY_CIPHER_MODE_CBC, 0);
  if (context == NULL)
    return NULL;

  res = gcry_cipher_setkey(context, key, strlen(key));
  if (res == GCRYERR_SUCCESS)
    res = gcry_cipher_setiv(context, NULL, 0);
  
  if (res != GCRYERR_SUCCESS) {
    printf("Error %d\n", res);
    return NULL;
  }

  res = gcry_cipher_encrypt(context, encrypted, length, decrypted, length);
  if (res != GCRYERR_SUCCESS) {
    printf("Error %d\n", res);
    return NULL;
  }

  printf("Decrypted %s, encrypted %s\n", decrypted, encrypted);

  gcry_cipher_close(context);
  free(key);
  return encrypted; 
}

char *public_to_address(const char *in_public) {
  char *local = NULL;
  char *base64 = NULL;
  char *end, *start, *encrypted, *decrypted = NULL;
  int i, base64_length, ended = 0;
  char *public = malloc(strlen(in_public) + 1);
  char *new_address, *p, *domain = NULL;
  int length;

  strcpy(public, in_public);

  //printf("Got %s\n", public);

  if ((end = strchr(public, '@')) == NULL)
    return NULL;

  *end = 0;

  if ((start = strrchr(public, '-')) == NULL)
    return NULL;

  local = cmalloc(start - public + 1);
  memcpy(local, public, start - public);

  //printf("Local %s\n", local);

  base64_length = end - start + 1 + 4;
  base64 = cmalloc(base64_length);
  memcpy(base64, start + 1, end - start);

  //printf("base64 %s\n", base64);

  length = strlen(base64) / 4 * 3;
  //printf("Length %d\n", length);

  for (i = 0; i < base64_length; i++) {
    if (base64[i] == 0)
      ended = 1;
    if (ended) {
      if (i % 4)
	base64[i] = '=';
      else
	break;
    }
  }

  //printf("New base64 %s\n", base64);

  encrypted = base64_decode(base64);

  //printf("Encrypted %s\n", encrypted);

  domain = cmalloc(length + 20);
  
  for (i = 0; i <= length/8; i++) {
    decrypted = decrypt(encrypted + i*8, 8);
    strcat(domain, decrypted);
    //printf("Decrypted %s\n", decrypted);
  }

  new_address = cmalloc(strlen(local) + strlen(domain) + 1 + 1);
  sprintf(new_address, "%s@%s", local, domain);
  if ((p = strchr(new_address, '\t')) != NULL)
    *p = 0;

  printf("new '%s'\n", new_address);

  free(local);
  free(base64);
  free(encrypted);
  free(decrypted);
  free(domain);

  return new_address;
}

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

void quote_char(char c, FILE *output) {
  if (word_chars[(int)c] || c == '.')
    putc(c, output);
  else if (c == ' ')
    putc('+', output);
  else 
    fprintf(output, "%%%02x", c);
}

void output_quote(FILE *output, const char *string) {
  unsigned char c;
  while ((c = *string++) != 0) 
    quote_char(c, output);
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
  int i, j;
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
	if (word_chars[(unsigned int)prev])
	  break;
	cmfs = &compiled_filters[i];
	if (regexec(&cmfs->from, string, 10, pmatch, 0) == 0) {

	  for (replace = cmfs->to; (c = *replace) != 0; replace++) {
	    if (c == '\\' && (index = *(replace + 1)) != 0) {
	      replace++;
	      index -= '0';
	      for (j = pmatch[index].rm_so; j < pmatch[index].rm_eo; j++) {
		if (i == 0 && index == 0)
		  /* Dirty, dirty hack. */
		  quote_char(*(string + j), output);
		else
		  fputc(*(string + j), output);
	      }
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
      kname = malloc(strlen(name) + 1);
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

void expiry_formatter (FILE *output, const char *expiry_string, 
		       const char *output_file_name) {
  ostring(output, "Expires: This article <a href=\"http://gmane.org/expiry.php\">expires</a> on ");
  filter(output, expiry_string);
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
  
  ostring(output, "<a target=\"_top\" href=\"http://www.dairiki.org/xface/\"><img border=0 alt=\"X-Face\" src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\"></a>\n");

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

  ostring(output, "<a target=\"_top\" href=\"http://quimby.gnus.org/circus/face/\"><img border=0 alt=\"Face\" src=\"http://cache.gmane.org/");
  uncached_name(output, png_file_name);
  ostring(output, "\"></a>\n");

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

  ostring(output, "<a target=\"_top\" href=\"http://ftp.cs.indiana.edu/pub/faces/picons/\"><img border=0 alt=\"Picon\" src=\"http://cache.gmane.org/");
  uncached_name(output, gif_file_name);
  ostring(output, "\"></a>\n");

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
  char *new_address;
  
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

    if (strstr(address, "@public.gmane.org")) {
      new_address = public_to_address(address);
      if (new_address != NULL)
	address = new_address;
    }

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
