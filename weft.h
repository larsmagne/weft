#ifndef WEFT_H
#define WEFT_H

char *text_file(char *name);
void uncached_name(FILE *output, const char *file_name);
int parse_args(int argc, char **argv);
char *get_cache_file_name(const char *file_name);
void ensure_directory(const char *file_name);

extern char *preamble_file_name;
extern char *picon_directory;
extern int max_plain_line_length;
extern int picon_number;
extern int binary_number;
extern char *default_charset;

#endif
