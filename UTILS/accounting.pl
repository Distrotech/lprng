#!/usr/local/bin/perl -w

# LPRng based accounting script.
# stdin = R/W Accounting File
# stdout = IO device
# stderr = log file
#  
#  command line format:
#   ac [start|stop|truncate] [-options] [accounting file]
#     -Tdebug will turn on debugging
#     start - at start of job; scan accounting file, fix up,
#         put in START entry
#     end  - at end of job; scan accounting file, fix up,
#         put in END entry
#     truncate - truncate the accounting file
#
# Accounting File has format:
# START A=id ....       - accounting script
# start -p=nnn          - filter
# ...
# end   -p=nnn+pagesused  - filter
# END  -p=pagesused     - accounting script
# 
# The accounting script expects to be invoked at the start of each job and
# will put a START line in.  However,  it will only get invoked at the
# end of correctly completed jobs.  This means that for correct accounting
# to be done,  then we need to make sure that jobs get pages assigned to
# them correctly.
#
# For brevity, we will show only the first word on each line in the
# following analysis.  We can make the following assumptions.  
# 
# At job start:
# .*END
#   Correctly updated job file,  the last job completed correctly
#   and the accounting was done completely.
# [.*END,]START,START*
#   None of these jobs was able to have the IF filter get the initial
#   page count.  They had submitted a job,  and used the facility,
#   but no paper/pages were used.
# [.*END],START,start,START*,START,start
#   The first job was able to get the IF filter started, but then was
#   unable to finish correctly.  The last job was able to establish
#   communications.  We can then calculate page counts for the last
#   job,  and update this to:
#    [END],START,start,END(bad),START*,start
#   This should be done before starting a job in order to correctly
#   count pages used prior to the current job.
# 
#   After checking,  a START line will be added to the file.
# 
# At job end:
# 
# [END],START,start,START*,START,start,end
#   We can now update this to:
#   [END],START,start,END(bad),START*,START,start,end,END
# 
#   The analysis for finding the END(bad) job is the same as for the
#   at job start.  The last END uses only the start and end information
#   for the last job entry.
# 
# Updating the accounting file.
#   The file needs to be scanned backwards for the last END entry.
#   This can be done by reading the file in in blocks, and scanning
#   for an END.  Once the END is located in the file,  then the
#   file from that point on only needs to be updated,  if at all.
# 
#   do{
#      bsize = bsize + incr;
#      if bsize > total file size then bsize = total file size;
#      buffer = bsize from end of file;
#      while buffer has \nEND then
#          find next \nEND in buffer.
# 		 find end of the END line;
#          bsize = location of end of END line;
#      endwhile
#   } while( bsize < total file size && END not found ){
#   # at this point buffer consists of
#   # (START(,end)*)* lines
#   split buffer into lines;
#   last_end = -1;
#   first_start = 0;
#   # the first start line should be 1
#   while( first_start < linecount && line[first_start] != START ){
# 	then you have problems;
#     ++first_start;
#   }
#  # look for first START,nnn combination
#   while( first_start+1 < linecount && line[first_start] == START ){
#      ++first_start;
#   }
#   non_start = first_start+1;
#   while( non_start+1 < linecount && line[non_start] != START ){
#      ++non_start;
#   }
#  # found the first START,nnn combination,  now look for next one
#   next_start = next_start+1;
#   while( next_start+1 < linecount && line[next_start] == START ){
#      ++next_start;
#   }
#   next_non_start = next_start+1;
# 
#   We now have:
#     START <-first_start
#     nnn1   <-first_start+1 
#     nnn   <-non-start
#     START*
#     START <-next_start
#     nnn2   <-next_start+1
#   
#   if next_start+1 < linecount
#    pages = nnn2 - nnn1
#     Insert END after non-start
#    START,nnn1,...nnn,END -p=pages,START*,nnn2
#    fixup = 1;
#    NOTE: you can update the user accounting database at this point
#     as well.
#   endif
#    first_start = next_start;
# 
#    and repeat while first_start < linecount;
#    
# if( fixup ){
#    we fseek to bsize from end of the file
#    write the new information to the file at this point
# }
#   
# fseek to end of file;
# 
# if( starting ) then
#   NOTE: you can check the user accounting database for permission
#     to use the printer at this point.
#    we write the new START record;
# else if( ending ) then
# 	we have the first_start entry already found.
# 	we get the 'start/end' information from the file and
# 	calculate the total page count.
# 	we then write an END record
#     NOTE: you can update the user accounting database at this point
#      as well.
# fi;
#   

# we get the arguments and options.  By default, first option
# will be start or end
#

$JFAIL = 32;
$JABORT = 33;
$JREMOVE = 34;
$JHOLD = 37;
$debug = 0;

# print STDERR "XX ACCOUNTING " . join(" ",@ARGV) . "\n" if $debug;
$end = "";
if( @ARGV ){
	$end = shift @ARGV;
}
if( $end eq "start" ){
	$end = "START";
} elsif( $end eq "end" ){
	$end = "END";
} elsif( $end eq "truncate" ){
	$end = "TRUNCATE";
} else {
	print STDERR "$0: first option must be 'start', 'end' or 'truncate'\n";
	exit $JABORT;
}

# pull out the options
for( $i = 0; $i < @ARGV; ++$i ){
	$opt = $ARGV[$i];
	# print STDERR "XX opt= $opt\n" if $debug;
	if( $opt eq '-c' ){
		$opt_c = 1;
	} elsif( ($key, $value) = ($opt =~ /^-(.)(.*)/) ){
		if( $value eq "" ){
			$value = $ARGV[++$i];
		}
		${"opt_$key"} = $value;
		# print STDERR "XX opt_$key = " . ${"opt_$key"} . "\n" if $debug;
	} else {
		$optind = $i;
		last;
	}
}
$af_file = $ARGV[$optind];

if( defined($opt_T) and $opt_T =~ m/debug/ ){
	$debug = 1;
}

$time = time;
print STDERR "XX $end A=$opt_A P=$opt_P n=$opt_n H=$opt_H D=$time\n" if $debug;

# reopen STDIN for R/W
if( $end eq "TRUNCATE" ){
	open( AF,"+<$af_file" ) or die "cannot open $af_file r/w - $!\n";
} else {
	open( AF,"+<&STDIN" ) or die "cannot reuse STDIN r/w - $!\n";
}

$size = -s AF;
print STDERR "XX AF size $size\n" if $debug;
$bsize = 0;
$last_end = -1;
do {
	# 1k increments
	$bsize = $bsize + 1024; 
	$bsize = $size if( $bsize > $size );
	print STDERR "XX bsize=$bsize\n" if $debug;
	seek AF, -$bsize, 2 or die "seek of $bsize failed - $!\n";
	$count = read AF, $buffer, $bsize;
	if( !defined($count)){
		die "read of $bsize failed - $!\n";
	} elsif( $count != $bsize ){
		die "read returned $count instead of $bsize\n";
	}
	print STDERR "XX read \nXX " . join( "\nXX ", split("\n",$buffer))."\n" if $debug;
	$loc = rindex( $buffer, "\nEND");
	print STDERR "XX loc=$loc\n" if $debug;
	if( $loc >= 0 ){
		$last_end = index( $buffer, "\n", $loc+1 );
		print STDERR "XX last_end=$last_end\n" if $debug;
		if( $last_end < 0 ){
			print STDERR "XX bad END entry in file\n" if $debug;
			seek AF, 0, 2 or die "seek to EOF failed\n";
			print AF "\n";
		} else {
			++$last_end;
			$bsize = $bsize - $last_end;
			$buffer = substr( $buffer, $last_end );
		}
	}
} while ( $bsize < $size and $last_end < 0 );

print STDERR "XX final bsize=$bsize, XX ". join("\nXX ",split("\n",$buffer))."\n" if $debug;

# truncate and exit with 0
if( $end eq "TRUNCATE" ){
	truncate AF, 0 or die "cannot truncate $af_file - $!\n";
	seek AF, 0, 0 or die "cannot seek to start $af_file - $!\n";
	print AF $buffer or die "cannot write to $af_file - $!\n";
	close AF or die "cannot close $af_file - $!\n";
	exit 0;
}

@af = split( /\n/, $buffer );

print STDERR "XX split \nXX ".join("\nXX ",@af)."\n" if $debug;

# case 0: [null]      - empty file   - go on
# case 1: START+       - job aborted  - go on
# case 2: START+,n,n,n - some printed - go on
# case 4: START+,n,n,n,START+ - job aborted - go on
# case 4: START+,n,n,n,START+,n,n - some printed - fix

# START*,START,n+,START*,START,n     - fix
#        ^                ^
#        last start       next start
#              
$fix = 0;
$first_start=0;
while( $first_start < @af and $af[$first_start] !~ /^START/ ){
	++$first_start;
}
while( $first_start < @af ){
	while( $first_start+1 < @af and $af[$first_start+1] =~ /^START/ ){
		++$first_start;
	}
	print STDERR "XX found first_start $first_start: $af[$first_start]\n" if $debug;
	for( $non_start = $first_start+1;
		$non_start+1 < @af and $af[$non_start+1] !~ /^START/;
		++$non_start ){;}
	last if( $non_start >= @af );
	print STDERR "XX found non_start $non_start: $af[$non_start]\n" if $debug;
	for( $next_start = $non_start+1;
		$next_start+1 < @af and $af[$next_start+1] =~ /^START/;
		++$next_start ){;}
	last if( $next_start >= @af );
	print STDERR "XX found next_start $next_start: $af[$next_start]\n" if $debug;
	# now we have either n or or no more lines
	if( $next_start+1 < @af ){
		($new) = $af[$first_start] =~ /START\s+(.*)/;
		$fix = 1;
		$pages = 0;
		if( !(($start_count) = $af[$first_start+1] =~ /start.*-p=([0-9]+)/)){
			print STDERR "Missing start count for $new\n";
			push(@af,"END p=-1 $new\n");
			$pages =-1;
		}
		if( !(($end_count) = $af[$next_start+1] =~ /start.*-p=([0-9]+)/)){
			print STDERR "Missing end count for $new\n";
			push(@af,"END p=-1 $new\n");
			$pages =-1;
		}
		$pages = $end_count - $start_count if $pages == 0;
		print STDERR "XX start_count=$start_count, end_count=$end_count\n" if $debug;
		splice(@af, $non_start+1, 0, "END p=$pages $new");
		print STDERR "XX new AF \nXX ".join("\nXX ",@af)."\n" if $debug;
	}
	$first_start = $next_start;
}
if( $fix ){
	print STDERR "XX fixing at offset $bsize output to be \nXX ".join("\nXX ",@af)."\n" if $debug;
	seek AF, -$bsize, 2 or die "seek to $bsize from EOF failed - $!\n";
	foreach (@af ){
		print AF "$_\n" or die "write failed - $!\n";
	}
} else {
	print STDERR "XX not fixing output\n" if $debug;
}
seek AF, 0 , 2 or die "seek to EOF failed - $!\n";

if( $end eq "START" ){
	# this is where you can put in a test to see that the user
	# has not exceeded his quota.  Return $JREMOVE if he has
	# put in a marker for this job.
	print STDERR "XX doing start\n" if $debug;
	print AF "START A=$opt_A P=$opt_P n=$opt_n H=$opt_H D=$time\n"
		or die "write failed - $!\n";
} else {
	$new = "A=$opt_A P=$opt_P n=$opt_n H=$opt_H";
	if( $first_start >= @af ){
		print STDERR "Missing START for $new\n";
		print $AF "END p=-1 $new\n" or die "write failed - $!\n";
		exit( $JABORT );
	}
	# check the last start for match
	$new = "A=$opt_A P=$opt_P n=$opt_n H=$opt_H";
	($old) = $af[$first_start] =~ /START\s+(.*)\s+D=.*/;
	print STDERR "XX new='$new', old='$old'\n" if $debug;
	if( $new ne $old ){
		print STDERR "START does not match $new\n";
		print AF "END p=-1 $new\n" or die "write failed - $!\n";
		exit( $JABORT );
	}
	# we get the first and last count values from the file
	if( !(($start_count) = $af[$first_start+1] =~ /start.*-p=([0-9]+)/)){
		print STDERR "Missing start count for $new\n";
		print AF "END p=-1 $new\n" or die "write failed - $!\n";
		exit( $JABORT );
	}
	if( !(($end_count) = $af[@af-1] =~ /end.*-p=([0-9]+)/)){
		print STDERR "Missing end count for $new\n";
		print AF "END p=-1 $new\n" or die "write failed - $!\n";
		exit( $JABORT );
	}
	print STDERR "XX start_count=$start_count, end_count=$end_count\n" if $debug;
	$pages = $end_count - $start_count;
	if( $pages < 0 ){
		print STDERR "START $start_count END $end_count values inconsistent $new\n";
		print AF "END p=-1 $new\n" or die "write failed - $!\n";
		exit( $JABORT );
	}
	print STDERR "XX END p=$pages $new\n" if $debug;
	print AF "END p=$pages $new\n" or die "write failed - $!\n";
}
close AF or die "cannot close AF - $!\n";
close STDIN or die "cannot close STDIN - $!\n"; 

exit 0;
