# Turn on debugging
CFLAGS+=-g

# Find libxml2 and libxslt header files and sharedlib
CPPFLAGS+=$(shell xml2-config --cflags) $(shell xslt-config --cflags)
LDFLAGS+=$(shell xml2-config --libs) $(shell xslt-config --libs)


# Implicit rule to create C string constants containing XSL files
%_xsl.h: %.xsl
	./embed.pl $<


all: teststrip

teststrip: teststrip.o striphtml.o

teststrip.o striphtml.o: striphtml.h
striphtml.o: striphtml_xsl.h

striphtml_xsl.h: embed.pl
striphtml_xsl.h: striphtml.xsl

clean:
	$(RM) *.o *_xsl.h teststrip
