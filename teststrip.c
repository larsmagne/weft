#include "striphtml.h"
#include <sys/stat.h>

main(int argc, char** argv)
{
  if (argc>1) {
    int htmlFileSize, retcode, noOfBytesRead;
    char* htmlFileBuf;
    struct stat stbuf;
    FILE* htmlFile;
    const char* htmlFileName = argv[1];
    /* Read the file into a memory buffer */
    retcode = stat(htmlFileName, &stbuf);
    fprintf(stderr, "stat retcode: %d\n", retcode);
    htmlFileSize = stbuf.st_size;
    fprintf(stderr, "input file size: %d\n", htmlFileSize);
    htmlFileBuf = malloc(htmlFileSize+1);
    htmlFile = fopen(htmlFileName, "r");
    noOfBytesRead = fread(htmlFileBuf, 1, htmlFileSize, htmlFile);
    fprintf(stderr, "input file bytes read: %d\n", noOfBytesRead);
    htmlFileBuf[htmlFileSize] = '\000';

    /* Stripped HTML document and write the stripped doc to stdout */
    stripHtml(htmlFileBuf, htmlFileSize, "-");
  }
}
