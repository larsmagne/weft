#include "striphtml.h"
#include <libxml/HTMLparser.h>
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
  tags.  The resulting stripped file, is written to \a filename.

  The result code from htmlParseDocument is returned (0 for OK, and
  -1 for parsing error).

  A stripped document is output even if HTML parsing errors are
  encountered.
*/
int
stripHtml(const char* htmlBuf, int htmlBufLen, const char* filename)
{
  xmlDocPtr strippedDoc = 0;
  xmlDocPtr htmlDoc = 0;
  htmlParserCtxtPtr ctxt = htmlCreateMemoryParserCtxt(htmlBuf, htmlBufLen);
  int retcode = htmlParseDocument(ctxt);
  htmlDoc = ctxt->myDoc;
  if (htmlDoc != 0) {
    strippedDoc = stripHtmlDoc(htmlDoc);
    htmlSaveFile(filename, strippedDoc);
    xmlFreeDoc(strippedDoc);
  }
  htmlFreeParserCtxt(ctxt);
  return retcode;
}

