#ifndef TRANSFORM_H
#define TRANSFORM_H

void transform_file(const char *input_file_name, 
		    const char *output_file_name);
char *convert_to_utf8(const char *string, const char *charset);

extern const char *message_id;

#endif
