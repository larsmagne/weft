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
void image_box_start (FILE *output, const char *dummy, 
		      const char *output_file_name);
void image_box_end (FILE *output, const char *dummy, 
		    const char *output_file_name);
void from_picon_displayer (FILE *output, const char *from, 
			   const char *output_file_name);
void ostring(FILE *output, const char *string);
void filter(FILE *output, const char *string);
void compile_filters(void);

extern int start_filter;

#endif
