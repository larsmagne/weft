#ifndef FORMATTERS_H
#define FORMATTERS_H

void from_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void subject_formatter (FILE *output, const char *value, 
		     const char *output_file_name);
void date_formatter (FILE *output, time_t time, int tz);
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
void from_gravatar_displayer (FILE *output, const char *from, 
			      const char *output_file_name);
void expiry_formatter (FILE *output, const char *expiry_string, 
		       const char *output_file_name);
void from_icon_displayer(FILE *output, const char *from, 
			 const char *output_file_name);
void from_cached_gravatar_displayer(FILE *output, const char *from, 
				    const char *output_file_name);
void x_image_url_displayer (FILE *output, const char *url, 
			    const char *output_file_name);
void ostring(FILE *output, const char *string);
void simple_filter(FILE *output, const char *string);
void filter(FILE *output, const unsigned char *string);
void compile_filters(void);
void compile_words(void);

extern int start_filter;

#endif
