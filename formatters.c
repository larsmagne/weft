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

typedef struct {
  char from;
  char *to;
} filter_spec;

filter_spec filters[] = {
  {'<', "&lt;"},
  {'>', "&gt;"},
  {'&', "&amp;"},
  {'@', " &lt; at &gt; "},
  {0, NULL}
};

void ostring(FILE *output, const char *string) {
  fwrite(string, strlen(string), 1, output);
}

void filter(FILE *output, const char *string) {
  char c;
  int i, found;
  filter_spec *fs;

  while ((c = *string++) != 0) {
    found = 0;
    for (i = 0; filters[i].from != 0; i++) {
      fs = &filters[i];
      if (c == fs->from) {
	ostring(output, fs->to);
	found = 1;
	break;
      }
    }
    if (! found) 
      fputc(c, output);
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
