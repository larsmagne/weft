#include "striphtml.h"
#include <libxml/HTMLparser.h>

main(int argc, char** argv)
{
  if (argc>1) {
    const char* htmlFileName = argv[1];
    xmlDocPtr htmlDoc = htmlParseFile(htmlFileName, 0);
    xmlDocPtr strippedDoc = stripHtml(htmlDoc);
    /* save document to stdout */
    htmlSaveFile("-", strippedDoc);
  }
}
