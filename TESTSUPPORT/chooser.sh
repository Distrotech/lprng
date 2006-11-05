#!/bin/sh
echo CHOOSER $0 "$@" 1>&2
echo PRINTERS $PRINTERS 1>&2
sed -e 's/,.*//'
exit 0
