#!/bin/sh
accounting.pl start 0<>accnt \
-Atesting -CA -Ff -Hastart4.astart.com -J/tmp/hi -Lpapowell \
-Tdebug=1 \
-Pt1 -aaccnt -d/var/tmp/LPD/t1/ -hastart4.astart.com \
-j440 -k/var/tmp/LPD/t1/cfA440astart4.astart.com \
-l66 -npapowell -sstatus -w80 -x0 -y0 accnt
i=$$
j=`expr $i + 1`
##echo "$i, $j";
echo "start -p='$i'" >>accnt;
echo "end -p='$j'" >>accnt;
accounting.pl end 0<>accnt \
-Atesting -CA -Ff -Hastart4.astart.com -J/tmp/hi -Lpapowell \
-Tdebug=1 \
-Pt1 -aaccnt -d/var/tmp/LPD/t1/ -hastart4.astart.com \
-j440 -k/var/tmp/LPD/t1/cfA440astart4.astart.com \
-l66 -npapowell -sstatus -w80 -x0 -y0 accnt
