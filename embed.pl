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

print OUT "$arrayName=\n";
while(<EMBED>) {
    tr/\n//d;
    s/"/\\"/g;
    if ($_) {
	print OUT "  \"$_\"\n";
    }
}
print OUT ";\n";

close(OUT);
close(EMBED);

