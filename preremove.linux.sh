#
# $Id: preremove.linux.sh,v 1.1 2000/06/06 18:16:08 papowell Exp papowell $
#
echo RUNNING preremove.linux.sh
echo "Stopping lpd server"
killall -INT lpd
sleep 2
if [ -f /etc/redhat-release -a -f /sbin/chkconfig ] ; then
	/sbin/chkconfig lprng off
	/sbin/chkconfig --del lprng
else
    for i in /etc/rc.d/*/*lprng ; do
        rm $i;
    done
fi
