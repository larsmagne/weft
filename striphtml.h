#ifndef STRIPHTML_H
#define STRIPHTML_H

#include <libxml/tree.h>

xmlDocPtr stripHtmlDoc(xmlDocPtr htmlDoc);
void stripHtml(const char* htmlBuf, int htmlBufLen, const char* filename);


#endif /* STRIPHTML_H */

