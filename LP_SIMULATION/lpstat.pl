#!/usr/local/bin/perl
#To: plp@iona.ie
#Subject: Re: CDE and PLP/LPRng 
#Date: Tue, 30 Jan 1996 17:31:22 +0000
#From: Justin Mason <jmason@iona.com>
#
#
# lpstat - fake System V lpstat(1) program for PLP installations.
#          it'll convince most scripts, but don't think your users
#          will be fooled... ;)
#
#          -- Justin Mason <jmason@iona.ie> May '94.
#
# Updated and modified for LPRng
#    Patrick Powell <papowell@sdsu.edu>
#    Sun Mar  3 17:17:45 PST 1996
#
# Note: not all of lpstats  options are supported
#lpstat [ -d ] [ -r ] [ -R ] [ -s ] [ -t ] [ -a [list] ]
#          [ -c [list] ] [ -f [list] [ -l ] ] [ -o [list] ]
#          [ -p [list] [ -D ] [ -l ] ] [ -P ] [ -S [list] [ -l ] ]
#          [ -u [login-ID-list] ] [ -v [list] ]
#
# note: if [list] is 'all', then all printers are listed
#
# -a [list] = reports if dest accepting request - mapped into spooling enabled
# -c = class names and members - prints all printer names
# -d = system default printer - defaults to lp
# -f [list] = forms acceptible - ignored
# -o [list] = output status - prints job status for printers - lpq front end
# -p [list] = printer status (short?)
# -P = Paper types - ignored
# -r = scheduler status - defaults to enabled
# -R = print job status in list form (lpq type of output)
# -s = status summary = configuration information
# -S = shows the character set or print wheels - ignored
# -t = full status information
# -u [login-ID-list] = status of user jobs - ignored
#      all - all users
#      all!login or login - user on all systems 
#      system!login - just this user
#       - this is used to select job submitted by user
# -v [list] = printer names and/or paths for devices


$accepting=0;
$default=0;
$sched=0;
$status=0;
$showjobs=0;

$args = join (' ', @ARGV);
while ($#ARGV >= 0) {
    $_ = $ARGV[0]; shift;
    if ( s/^-(.)// ){
		$opt = $1;
	} elsif( $getlist ){
		$list{$_} = $_;
		++$listcount;
		#print "adding $_\n";
		next;
	} else {
		next;
	}
    if (s/^(.*)$//) { 
        $optarg = $1;
    } else {
        $optarg = $ARGV[0];
    }

    if ($opt eq 'c') { $getlist = 1; next; }
    if ($opt eq 'd') { $getlist = 0; ++$default; next; }
    if ($opt eq 'f') { $getlist = 1; next; }
    if ($opt eq 'l') { $getlist = 0; next; }
    if ($opt eq 'r') { $getlist = 0; ++$sched; next; }
    if ($opt eq 's') { $getlist = 0; $default = ++$sched; next; }
    if ($opt eq 't') { $getlist = 0;
					++$sched;
					$default = 1;
					$status = 1;
					$showjobs = 1;
					next; }
    if ($opt eq 'o') { $getlist = 1;
					$status = 1;
					$showjobs = 1;
					next; }
    if ($opt eq 'a') { $getlist = 1; ++$accepting; next; }
    if ($opt eq 'p') { $getlist = 1; ++$status; next; }
    if ($opt eq 'u') { $getlist = 1; next; }
    if ($opt eq 'v') { $getlist = 0;
                        next; }
    if ($opt eq 'D') { $getlist = 0; next; }
    if ($opt eq 'P') { $getlist = 0; next; }
    if ($opt eq 'R') { $showjobs = 1; next; }
    if ($opt eq 'S') { $getlist = 1; next; }
    warn "unsupported lpstat option: -$opt (from: $args)\n";
}

chop ($date=`date`);

if ($sched) {
    print "scheduler is running\n";
}
if ($default) {
    print "system default destination: lp\n";
}

if( $listcount == 0 ){
	$list{"all"} = "all";
}
if ($accepting || $status) { &getLpq; }

#print "list " . join( " ", sort keys %list ) . "\n";
#print "list " . join( " ", sort keys %printers ) . "\n";

if ($accepting) {
    for $pr (sort keys %printers) {
		if( $disabled{$pr} ne "disabled" ){
			print "$pr accepting requests since $lastjob{$pr}\n";
		}
    }
}

if ($status) {
    for $pr (sort keys %printers) {
		print "printer $pr $offline{$pr}. $disabled{$pr} since $lastjob{$pr}. $available{$pr}.\n";
        print $text{$pr};
		if( $jobstatus{$pr} ){
			print $rank{$pr};
			print $jobstatus{$pr};
		}
    }
}

sub getLpq {
    # print "doing getLpq\n";
    local ($_, $prl, $pr, $where, $cmt, $lastjob, $jobs, $text, @entries);
	local ($jobstatus, $rank );
    for $prl (sort keys %list) {
		#print "doing: lpq -P$list{$prl}\n";
		open (LPQ, "lpq -P$list{$prl} |") || die "couldn't run lpq\n";
		undef $pr;
		while (<LPQ>) {
			#print "read: $_";
			@entries = split(' ');
			#print "entry $entries[0]\n";
			if ( /(Printer:)/ ) {
				# print "found $entries[0]\n";
				if (defined $pr) {
					$printers{$pr} = $pr;
					$hosts{$pr} = $where;
					$lastjob{$pr} = $lastjob;
					$jobs{$pr} = $jobs;
					$text{$pr} = $text;
					$disabled{$pr} = $disabled;
					$offline{$pr} = $offline;
					$available{$pr} = $available;
					$jobstatus{$pr} = $jobstatus;
			# print "pr $pr : disabled $disabled{$pr} offline $offline{$pr}\n"
				}
				$pr = $where = $cmt = $text = '';
				$disabled = "enabled";
				$offline = "online";
				$available = "available";
				$jobstatus = '';
				$lastjob = $date;
				$jobs = 0;
				$pr = $entries[1];
				if( $pr =~ /(.*)@(.*)/ ){
					$printer = $1;
					$where = $2;
				} else {
					$printer = $1;
				}
				if( /\'([^']*)\'/ ){
					$cmt = $1;
				}
				if( /spooling disabled/ ){
					$disabled = "disabled";
					$available = "unavailable";
				}
				if( /printing disabled/ ){
					$offline = "offline";
				}
				# print "found pr $printer, where $where, cmt $cmt\n";
				next;
			}
			if ( /Queue:\s+(\S+)/ ) {
				$jobs = $1;
				if( $jobs eq "no" ) {
					$jobs = 0;
					if( $offline ne "offline" ){
						$offline = "idle";
					} 
				}
				# print "jobs $jobs\n";
			}
			if ( /Status:.*at\s+(.*)/ ) {
				$lastjob = $1;
				$text = $_;
				# print "last $lastjob, text $text\n";
			}
			#print "entry $entries[0]\n";
			if( $entries[0] =~ /Rank/ ){
				#print "rank $rank\n";
				$rank = " " . $_;
			} elsif( ( $entries[0] =~ /active/ ) || ( $entries[0] > 0 ) ){
				$jobstatus .= " " . $_;
			}
		}
		close LPQ;
	}
    if (defined $pr) {
		$printers{$pr} = $pr;
		$hosts{$pr} = $where;
		$lastjob{$pr} = $lastjob;
		$jobs{$pr} = $jobs;
		$text{$pr} = $text;
		$disabled{$pr} = $disabled;
		$offline{$pr} = $offline;
		$available{$pr} = $available;
		$rank{$pr} = $rank;
		$jobstatus{$pr} = $jobstatus;
		# print "pr $pr : disabled $disabled{$pr} offline $offline{$pr}\n"
    }
    1;
}
