#ifndef WEFT_H
#define WEFT_H

char *text_file(char *name);
void uncached_name(FILE *output, const char *file_name);

extern char *preamble_file_name;
extern char *picon_directory;

#endif
