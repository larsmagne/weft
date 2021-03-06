#!/usr/bin/perl
#
# Take a filename as a command line argument.  Create a 
# C include file defining a C string constant holding the
# contents of the file given as the argument.
#
$embedFileName = $ARGV[0];
open(EMBED, $embedFileName);

$arrayName = $embedFileName;
$arrayName =~ s/\./_/g;

$outputFileName = $arrayName.".h";
open(OUT,">$outputFileName");

print OUT "const char $arrayName\[\] =\n";
while(<EMBED>) {
    tr/\n//d;
    s/"/\\"/g;
    print OUT "  \"$_\\n\"\n";
}
print OUT ";\n";

close(OUT);
close(EMBED);

