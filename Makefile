GMIME_CONFIG = /usr/bin/pkg-config
GMIME_CFLAGS = `$(GMIME_CONFIG) gmime-2.6 --cflags`
GMIME_LIBS = `$(GMIME_CONFIG) gmime-2.6 --libs`

HEADER_FILES=config.h util.h hash.h weaver.h input.h dispatch.h

CPPFLAGS=$(GMIME_CFLAGS) -I/usr/include/ImageMagick -I/usr/include/libxml2 -I/usr/include/libxslt -I/usr/local/include -g -O3 -Wall
LDFLAGS=$(GMIME_LIBS) -lgcrypt -lMagickWand -lxml2 -lcompface -lxslt
CC = gcc $(CPPFLAGS)

all: weft weftd

weft.o: weft.c config.h

formatters.o: formatters.c config.h

transform.o: transform.c config.h

daemon.o: daemon.c config.h

int.o: int.c config.h

stripHtml.o: stripHtml.c

# Implicit rule to create C string constants containing XSL files
%_xsl.h: %.xsl
	./embed.pl $<

striphtml_xsl.h: embed.pl
striphtml_xsl.h: striphtml.xsl

weft: weft.o formatters.o transform.o striphtml.o striphtml_xsl.h int.o
	$(CC) $(CPPFLAGS) -o weft $(LDFLAGS) weft.o formatters.o\
	 transform.o striphtml.o int.o

weftd: weft.o formatters.o transform.o striphtml.o striphtml_xsl.h daemon.o
	$(CC) $(CPPFLAGS) -o weftd $(LDFLAGS) weft.o formatters.o\
	 transform.o striphtml.o daemon.o

clean:
	$(RM) indexer *.o weft weftd

TAGS: *.h *.c
	etags *.[ch]
