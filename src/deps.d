accounting.lo accounting.o: common/accounting.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/accounting.h \
  include/getqueue.h include/errorcodes.h include/child.h \
  include/linksupport.h include/fileopen.h
checkpc.lo checkpc.o: common/checkpc.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h defs.h include/getopt.h \
  include/checkpc.h include/patchlevel.h include/getprinter.h \
  include/getqueue.h include/initialize.h include/lockfile.h \
  include/fileopen.h include/child.h include/stty.h include/proctitle.h \
  include/lpd_remove.h include/linksupport.h include/gethostinfo.h
child.lo child.o: common/child.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getqueue.h \
  include/getopt.h include/gethostinfo.h include/proctitle.h \
  include/linksupport.h include/child.h
controlword.lo controlword.o: common/controlword.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/control.h
copyright.lo copyright.o: common/copyright.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/patchlevel.h \
  include/license.h include/copyright.h
debug.lo debug.o: common/debug.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/getopt.h include/child.h
errormsg.lo errormsg.o: common/errormsg.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/child.h include/getopt.h include/getqueue.h
fileopen.lo fileopen.o: common/fileopen.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/fileopen.h \
  include/errorcodes.h include/child.h
gethostinfo.lo gethostinfo.o: common/gethostinfo.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/gethostinfo.h \
  include/linksupport.h include/getqueue.h include/globmatch.h
getopt.lo getopt.o: common/getopt.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h
getprinter.lo getprinter.o: common/getprinter.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/gethostinfo.h \
  include/getprinter.h include/getqueue.h include/child.h
getqueue.lo getqueue.o: common/getqueue.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/errorcodes.h include/fileopen.h include/getprinter.h \
  include/gethostinfo.h include/getqueue.h include/globmatch.h \
  include/permission.h include/lockfile.h include/merge.h
globmatch.lo globmatch.o: common/globmatch.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h
initialize.lo initialize.o: common/initialize.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h defs.h include/initialize.h \
  include/getopt.h include/child.h include/gethostinfo.h \
  include/proctitle.h include/getqueue.h include/errorcodes.h
krb5_auth.lo krb5_auth.o: common/krb5_auth.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/fileopen.h include/child.h include/getqueue.h \
  include/linksupport.h include/gethostinfo.h include/permission.h \
  include/lpd_secure.h include/lpd_dispatch.h include/krb5_auth.h
linelist.lo linelist.o: common/linelist.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/globmatch.h include/gethostinfo.h include/child.h \
  include/fileopen.h include/getqueue.h include/getprinter.h \
  include/lpd_logger.h include/lpd_dispatch.h include/lpd_jobs.h
linksupport.lo linksupport.o: common/linksupport.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/linksupport.h \
  include/gethostinfo.h include/errorcodes.h
lockfile.lo lockfile.o: common/lockfile.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/lockfile.h \
  include/fileopen.h
lpbanner.lo lpbanner.o: common/lpbanner.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h
lpc.lo lpc.o: common/lpc.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h defs.h include/initialize.h \
  include/getprinter.h include/sendreq.h include/child.h \
  include/control.h include/getopt.h include/patchlevel.h \
  include/errorcodes.h include/lpc.h
lpd.lo lpd.o: common/lpd.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/fileopen.h include/errorcodes.h include/initialize.h \
  include/linksupport.h include/lpd_logger.h include/getqueue.h \
  include/getopt.h include/proctitle.h include/lockfile.h include/lpd.h
lpd_control.lo lpd_control.o: common/lpd_control.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getopt.h \
  include/proctitle.h include/control.h include/child.h \
  include/getprinter.h include/getqueue.h include/fileopen.h \
  include/globmatch.h include/permission.h include/gethostinfo.h \
  include/lpd_control.h
lpd_dispatch.lo lpd_dispatch.o: common/lpd_dispatch.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/getqueue.h include/getprinter.h include/gethostinfo.h \
  include/linksupport.h include/child.h include/fileopen.h \
  include/permission.h include/proctitle.h include/lpd_rcvjob.h \
  include/lpd_remove.h include/lpd_status.h include/lpd_control.h \
  include/lpd_secure.h include/krb5_auth.h include/lpd_dispatch.h
lpd_jobs.lo lpd_jobs.o: common/lpd_jobs.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/accounting.h \
  include/child.h include/errorcodes.h include/fileopen.h \
  include/gethostinfo.h include/getopt.h include/getprinter.h \
  include/getqueue.h include/linksupport.h include/lockfile.h \
  include/lpd_remove.h include/merge.h include/permission.h \
  include/printjob.h include/proctitle.h include/sendjob.h \
  include/sendmail.h include/stty.h include/lpd_jobs.h \
  include/lpd_rcvjob.h
lpd_logger.lo lpd_logger.o: common/lpd_logger.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/errorcodes.h include/fileopen.h include/getopt.h \
  include/getprinter.h include/getqueue.h include/linksupport.h \
  include/proctitle.h include/lpd_logger.h
lpd_rcvjob.lo lpd_rcvjob.o: common/lpd_rcvjob.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/errorcodes.h include/fileopen.h include/gethostinfo.h \
  include/getopt.h include/getqueue.h include/linksupport.h \
  include/lockfile.h include/permission.h include/proctitle.h \
  include/lpd_remove.h include/lpd_rcvjob.h include/lpd_jobs.h
lpd_remove.lo lpd_remove.o: common/lpd_remove.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/lpd_remove.h \
  include/getqueue.h include/getprinter.h include/gethostinfo.h \
  include/getopt.h include/permission.h include/child.h \
  include/proctitle.h include/fileopen.h include/sendreq.h
lpd_secure.lo lpd_secure.o: common/lpd_secure.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/user_auth.h \
  include/lpd_dispatch.h include/getopt.h include/getqueue.h \
  include/proctitle.h include/permission.h include/linksupport.h \
  include/errorcodes.h include/fileopen.h include/lpd_rcvjob.h \
  include/child.h include/globmatch.h include/lpd_jobs.h \
  include/krb5_auth.h include/lpd_secure.h
lpd_status.lo lpd_status.o: common/lpd_status.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getopt.h \
  include/gethostinfo.h include/proctitle.h include/getprinter.h \
  include/getqueue.h include/child.h include/fileopen.h include/sendreq.h \
  include/globmatch.h include/permission.h include/lockfile.h \
  include/errorcodes.h include/lpd_jobs.h include/lpd_status.h
lpf.lo lpf.o: common/lpf.c include/portable.h ../config.h
lpq.lo lpq.o: common/lpq.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/getopt.h include/getprinter.h include/getqueue.h \
  include/initialize.h include/linksupport.h include/patchlevel.h \
  include/sendreq.h include/lpq.h
lpr.lo lpr.o: common/lpr.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/errorcodes.h include/fileopen.h include/getopt.h \
  include/getprinter.h include/getqueue.h include/gethostinfo.h \
  include/initialize.h include/linksupport.h include/patchlevel.h \
  include/printjob.h include/sendjob.h include/lpd_jobs.h include/lpr.h
lprm.lo lprm.o: common/lprm.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/getopt.h include/getprinter.h include/getqueue.h \
  include/initialize.h include/linksupport.h include/patchlevel.h \
  include/sendreq.h include/lprm.h
lpstat.lo lpstat.o: common/lpstat.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/getopt.h include/getprinter.h include/initialize.h \
  include/linksupport.h include/patchlevel.h include/sendreq.h \
  include/lpstat.h
md5.lo md5.o: common/md5.c include/md5.h
merge.lo merge.o: common/merge.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/merge.h
monitor.lo monitor.o: common/monitor.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getopt.h \
  include/linksupport.h include/getqueue.h
permission.lo permission.o: common/permission.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/fileopen.h \
  include/globmatch.h include/gethostinfo.h include/getqueue.h \
  include/permission.h include/linksupport.h
plp_snprintf.lo plp_snprintf.o: common/plp_snprintf.c ../config.h
printjob.lo printjob.o: common/printjob.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/printjob.h include/getqueue.h include/child.h \
  include/fileopen.h
proctitle.lo proctitle.o: common/proctitle.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/proctitle.h
sendauth.lo sendauth.o: common/sendauth.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/user_auth.h \
  include/sendjob.h include/globmatch.h include/permission.h \
  include/getqueue.h include/errorcodes.h include/linksupport.h \
  include/krb5_auth.h include/fileopen.h include/child.h \
  include/gethostinfo.h include/sendauth.h
sendjob.lo sendjob.o: common/sendjob.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/accounting.h \
  include/errorcodes.h include/fileopen.h include/getqueue.h \
  include/user_auth.h include/linksupport.h include/sendjob.h \
  include/sendauth.h
sendmail.lo sendmail.o: common/sendmail.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/errorcodes.h \
  include/fileopen.h include/getqueue.h include/sendmail.h \
  include/child.h
sendreq.lo sendreq.o: common/sendreq.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/child.h \
  include/fileopen.h include/getqueue.h include/linksupport.h \
  include/readstatus.h include/user_auth.h include/sendreq.h \
  include/sendauth.h
ssl_auth.lo ssl_auth.o: common/ssl_auth.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/fileopen.h \
  include/errorcodes.h include/getqueue.h include/user_auth.h \
  include/lpd_secure.h include/ssl_auth.h
stty.lo stty.o: common/stty.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/stty.h
user_auth.lo user_auth.o: common/user_auth.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/user_auth.h \
  include/krb5_auth.h include/errorcodes.h include/fileopen.h \
  include/linksupport.h include/child.h include/getqueue.h \
  include/lpd_secure.h include/lpd_dispatch.h include/permission.h \
  include/ssl_auth.h include/md5.h
user_objs.lo user_objs.o: common/user_objs.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getqueue.h
utilities.lo utilities.o: common/utilities.c include/lp.h include/portable.h \
  ../config.h include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h include/getopt.h \
  include/errorcodes.h
vars.lo vars.o: common/vars.c include/lp.h include/portable.h ../config.h \
  include/linelist.h include/utilities.h include/debug.h \
  include/errormsg.h include/plp_snprintf.h defs.h include/child.h \
  include/gethostinfo.h include/getqueue.h include/accounting.h \
  include/permission.h include/printjob.h
