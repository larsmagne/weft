
# Implicit rule to create C string constants containing XSL files
%_xsl.h: %.xsl
	./embed.pl $<


all: striphtml_xsl.h

striphtml_xsl.h: embed.pl
striphtml_xsl.h: striphtml.xsl

