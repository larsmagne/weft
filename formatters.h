#ifndef FORMATTERS_H
#define FORMATTERS_H

void from_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void subject_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void date_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void newsgroups_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void face_displayer (FILE *output, const char *face, 
		     const char *output_file_name);
void xface_displayer (FILE *output, const char *face, 
		      const char *output_file_name);

#endif
