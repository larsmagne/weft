<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<xsl:output method="html"/>

<xsl:template match="/">
  <div>
  <xsl:apply-templates/>
 </div>
</xsl:template>

<!-- This template lets safe elements through -->
<!-- The list of elements in the match attribute isn't complete -->
<xsl:template match="h1|h2|h3|h4|h5|h6|p|a|div|ul|li|ol|table|tr|td|br">
  <xsl:element name="{name(.)}">
    <xsl:copy-of select="@*[name() != 'background' and name() != 'src']"/>
    <xsl:apply-templates/>
  </xsl:element>
</xsl:template>

<!-- stripping template.  "eat" unwanted tags and their content -->
<!-- The list of elements in the match attribute isn't complete -->
<xsl:template match="head|script|object|form|img|applet"/>

<!-- Anything not matched by either of the above templates -->
<!-- will get its CDATA content output -->

</xsl:stylesheet>
