#!/usr/bin/perl
eval 'exec /usr/bin/perl -S $0 ${1+"$@"}'
    if $running_under_some_shell;
			# this emulates #! processing on NIH machines.
			# (remove #! line above if indigestible)

use Getopt::Std;

#!/bin/sh
# shell for PCL banner printing 
#
# Input to the script are lines of the form
# <class>:<user> [Host: <hostname>] [Job: <jobtitle>] [User: <username>]
#  Example:
# A:papowell Host: host1 Job: test file
# The first field is the class:user information.  Other fields can be:
#   User: user name
#   Host: host name
#   Job:  job title name
#

getopts(
"a:b:cd:e:f:g:h:i:j:k:l:m:n:o:p:q:r:s:t:u:v:w:x:y:z:" .
"A:B:C:D:E:F:G:H:I:J:K:L:M:N:O:P:Q:R:S:T:U:V:W:X:Y:Z:", \%args );

$arg{'User'} = $args{'n'} if $args{'n'};
$arg{'Host'} = $args{'H'} if $args{'H'};
$arg{'Id'} = $args{'j'} if $args{'j'};
$arg{'Class'} = $args{'C'} if $args{'C'};
$arg{'Job'} = $args{'J'} if $args{'J'};
$i = 0;
$order[$i++] = 'User';
$order[$i++] = 'Host';
$order[$i++] = 'Id';
$order[$i++] = 'Class';
$order[$i++] = 'Job';

$xpos = 0;
$ypos = 0;
$incr = 0;
$margins = "\033&l0u0Z";
$lightbar = "\033*c1800a100b45g2P";
$darkbar = "\033*c1800a100b25g2P";
$fontchange = "\033(8U\033(s1p%dv0s0b4148T";
$position = "\033*p%dx%dY";
$FFEED = "\014";
$UEL = "\033%-12345X";
$UELPJL = "\033%-12345X\@PJL \n";
$PCLRESETSTR = "\033E";
$CRLFSTR = "\033&k2G";

&pcl_banner();


sub moveto {
    local($X, $Y) = @_;
    printf $position, $X, $Y;
}

sub fontsize {
    local($size) = @_;
    $incr = ($size * 300 * 1.1) / 72;
    printf $fontchange, $size;
}

sub outline {
    local($S) = @_;
    printf '%s', $S if defined $S;
}

sub argline {
    local($key, $value) = @_;
    if (defined($value) and $value ne '') {
	&textline("$key: ", 1, 0);
	&textline($value, 0, 1);
    }
}

sub textline {
    local($line, $start, $end) = @_;
    if ($start) {
	&moveto($xpos, $ypos);
    }
    printf '%s', $line;
    if ($end) {
	$ypos += $incr;
    }
}

sub pcl_banner {
    &outline($UEL);
    &outline($PCLRESETSTR);
    &outline($UELPJL);
    &outline($CRLFSTR);
    &outline($margins);

    # do light bar 
    $xpos = 0;
    $ypos = 0;
    &moveto($xpos, $ypos);
    &outline($lightbar);
    $ypos += 100;

    # set font size 
    &fontsize(24);
    $ypos += $incr;
    &moveto($xpos, $ypos);

    foreach $key (@order) {
		&argline($key, $arg{$key}); 
    }

    # smaller font 
    &fontsize(12);

    &moveto($xpos, $ypos);
    $date = `date`;
    &textline('Date: ', 0, 1);
    &textline($date, 0, 1);

    &moveto($xpos, $ypos);
    &outline($darkbar);

    &outline($FFEED);
    &outline($UEL);
    &outline($PCLRESETSTR);
}
