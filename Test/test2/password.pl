
use strict;  # which you should be using
use CGI;
my $q = CGI->new;

my $data = $q->param('pw');

print "Content-type:text/html\n\n"; 
print $data