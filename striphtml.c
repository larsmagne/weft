#include "striphtml.h"
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
stripHtml(xmlDocPtr htmlDoc)
{
  xmlDocPtr retval = 0;
  initStripHtml();
  retval = xsltApplyStylesheet(stripHtmlXslt, htmlDoc, 0);
  return retval;
}

