#!/bin/sh
#
# lp -     fake System V lp(1) program for PLP installations.
#
#	   not too much to emulate here, so it's quite convincing. ;)
#
#          -- Justin Mason <jmason@iona.ie> May '94.
#          -- Patrick Powell <papowell@sdsu.edu> Nov '95

LPR=/usr/local/bin/lpr

OPTSTR="cmswrf:d:H:n:o:P:q:S:t:T:y:"

args=

while getopts $OPTSTR opt ; do
case $opt in
  c ) true; ;;  # symbolic link option
  d ) args="-P$OPTARG" ;;	# dest

  m ) args="$args -m `whoami`" ;;	# send mail after printing

  n ) args="$args -K$OPTARG" ;;	# number of copies

  q ) args="$args -C $OPTARG" ;; # priority (class)

  s ) true ;;		# suppress messages -- silently ignored

  t ) args="$args -T\"$OPTARG\"" ;; # title

  w ) echo "lp: write-after-printing (-$opt option) not supported!" 1>&2 ;;
  f ) echo "lp: forms (-$opt option) not supported!" 1>&2 ;;
  H ) echo "lp: special handling (-$opt option) not supported!" 1>&2 ;;
  P ) echo "lp: by-page printing (-$opt option) not supported!" 1>&2 ;;
  S ) echo "lp: character sets (-$opt option) not supported!" 1>&2 ;;
  y ) echo "lp: modes (-$opt option) not supported!" 1>&2 ;;

esac
done
shift `expr $OPTIND - 1`

exec $LPR $args "$@"
