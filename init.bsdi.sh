#
# -- START --
# init.bsdi.sh,v 1.1 2001/08/21 20:33:15 root Exp
# This file can be installed in /usr/local/etc/init.d
#  as lprng.sh
# You should install the following script to run the files in this
# directory:
#  # set the local_startup to have the directories with startup scripts
#  local_startup="/usr/local/etc"
#  # put this in /etc/rc or /etc/rc.local
#  case ${local_startup} in
#  [Nn][Oo] | '') ;;
#  *)
#      echo -n 'Local package initialization:'
#      for dir in ${local_startup}; do
#          if [ -d "${dir}" ]; then
#              for script in ${dir}/*.sh; do
#                  if [ -x "${script}" ]; then
#                      (set -T; trap 'exit 1' 2; ${script} start)
#                  fi
#              done
#          fi
#      done
#      ;;
#  esac
#

# ignore INT signal
trap '' 2

case "$1" in
    restart ) 
			$0 stop
			sleep 1
			$0 start
            ;;
    stop  )
		/usr/bin/killall -INT lpd >/dev/null 2>&1
            ;;
    start )
            echo -n ' lpd';
            ${LPD_PATH}
            ;;
esac
