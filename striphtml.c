#include <stdio.h>
#include <string.h>
#include "striphtml.h"
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>

/* The XSLT file as a string constant */
#include "striphtml_xsl.h"

/* A pointer to a parsed and ready to use version of the style sheet */
static xsltStylesheetPtr stripHtmlXslt = 0;

/*!
  Parse the XSLT style sheet from the string constant, and
  set the stripHtmlXslt pointer to the parsed style sheet.

  Not thread safe.
*/
static void
initStripHtml()
{
  if (stripHtmlXslt == 0) {
    xmlDocPtr stripHtmlXsltDoc;
    stripHtmlXsltDoc = xmlParseMemory(striphtml_xsl, strlen(striphtml_xsl));
    stripHtmlXslt = xsltParseStylesheetDoc(stripHtmlXsltDoc);
  }
}

/*!
  Apply the embedded XSLT style sheet for stripping dangerous
  HTML tags to \a htmlDoc, and return the result as a DOM tree.
 */
xmlDocPtr
stripHtmlDoc(xmlDocPtr htmlDoc)
{
  xmlDocPtr retval = 0;
  initStripHtml();
  retval = xsltApplyStylesheet(stripHtmlXslt, htmlDoc, 0);
  return retval;
}

/*!
  The \a htmlBuf argument points to a character buffer, with length
  \a htmlBufLen, holding an HTML document to be stripped for unwanted
  tags.  The resulting stripped HTML document, is inserted into
  a char buffer, and a pointer to the buffer is returned.  The caller
  takes up ownership of the returned buffer.

  The value of the \a length argument, is set to the length of the
  returned char buffer.

  A stripped document is output even if HTML parsing errors are
  encountered.
*/
char *
stripHtml(const char* htmlBuf, int htmlBufLen, int *length)
{
  xmlDocPtr strippedDoc = 0;
  xmlDocPtr htmlDoc = 0;
  xmlChar* result;
  int len;
  htmlDoc = htmlParseDoc((xmlChar*)htmlBuf, "iso-8859-9");

  if (htmlDoc != 0) {
    strippedDoc = stripHtmlDoc(htmlDoc);
    htmlDocDumpMemory(strippedDoc, &result, &len);
    xmlFreeDoc(strippedDoc);
    xmlFreeDoc(htmlDoc);
  }
  *length = len;
  return result;
}

