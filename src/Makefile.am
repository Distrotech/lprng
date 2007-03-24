MAINTAINERCLEANFILES = Makefile.in
CLEANFILES = *.tmp *core ? ?.bak ?.orig lpd.conf log lprng_certs lprng_index_certs
DISTCLEANFILES = pclbanner psbanner
EXTRA_DIST = pclbanner.in psbanner.in lprng_certs.sh lprng_index_certs.sh common/user_objs.c include/user_objs.h AUTHENTICATE/sserver.c AUTHENTICATE/sclient.c

PATHDEFINES = "-DLOCKFILE=\"$(LOCKFILE)\"" \
	"-DPRINTCAP_PATH=\"$(PRINTCAP_PATH)\"" \
	"-DLPD_PRINTCAP_PATH=\"$(LPD_PRINTCAP_PATH)\"" \
	"-DLPD_PERMS_PATH=\"$(LPD_PERMS_PATH)\"" \
	"-DLPD_CONF_PATH=\"$(LPD_CONF_PATH)\"" \
	"-DUNIXSOCKETPATH=\"$(UNIXSOCKETPATH)\"" \
	"-DSSL_CA_FILE=\"$(SSL_CA_FILE)\"" \
	"-DSSL_CA_KEY=\"$(SSL_CA_KEY)\"" \
	"-DSSL_CRL_FILE=\"$(SSL_CRL_FILE)\"" \
	"-DSSL_CERTS_DIR=\"$(SSL_CERTS_DIR)\"" \
	"-DSSL_SERVER_CERT=\"$(SSL_SERVER_CERT)\"" \
	"-DSSL_SERVER_PASSWORD_FILE=\"$(SSL_SERVER_PASSWORD_FILE)\"" \
	"-DSD_DEFAULT=\"$(SD_DEFAULT)\""

AM_CPPFLAGS = -I$(srcdir)/include "-DLOCALEDIR=\"$(localedir)\""

lpdbin_PROGRAMS = lpd
bin_PROGRAMS = lpr lpq lprm lpstat
sbin_PROGRAMS = lpc checkpc
sbin_SCRIPTS = lprng_certs lprng_index_certs
filter_PROGRAMS = lpf lpbanner
filter_SCRIPTS = psbanner pclbanner
noinst_DATA = lpd.conf
noinst_PROGRAMS = monitor
# were not even compiled in the old build system
# and may have issues hidden before because of static linking:
# noinst_PROGRAMS = sserver sclient

install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/lp $(DESTDIR)$(bindir)/cancel
	ln $(DESTDIR)$(bindir)/lpr $(DESTDIR)$(bindir)/lp
	ln $(DESTDIR)$(bindir)/lprm $(DESTDIR)$(bindir)/cancel
# - think about making things suid at install time

COMMON_SOURCES = common/child.c \
	common/copyright.c common/debug.c common/errormsg.c \
	common/fileopen.c common/gethostinfo.c common/getopt.c \
	common/getprinter.c common/getqueue.c common/globmatch.c \
	common/initialize.c common/krb5_auth.c common/linelist.c \
	common/linksupport.c common/lockfile.c common/merge.c \
	common/plp_snprintf.c common/printjob.c \
	common/proctitle.c common/sendjob.c common/sendauth.c \
	common/sendreq.c common/ssl_auth.c \
	common/user_auth.c common/utilities.c vars.c \
	common/md5.c
SEND_SOURCES = common/openprinter.c common/stty.c

lpd_SOURCES = common/lpd.c common/lpd_worker.c common/lpd_jobs.c \
	common/lpd_control.c common/sendmail.c common/lpd_dispatch.c \
	common/lpd_logger.c common/lpd_rcvjob.c common/lpd_remove.c \
	common/lpd_secure.c common/lpd_status.c \
	common/permission.c common/accounting.c common/controlword.c \
	$(SEND_SOURCES) $(COMMON_SOURCES)
lpr_SOURCES = common/lpr.c $(SEND_SOURCES) $(COMMON_SOURCES)
lpq_SOURCES = common/lpq.c $(COMMON_SOURCES)
lpstat_SOURCES = common/lpstat.c $(COMMON_SOURCES)
lprm_SOURCES = common/lprm.c $(COMMON_SOURCES)
lpc_SOURCES = common/lpc.c common/controlword.c $(COMMON_SOURCES)
checkpc_SOURCES = common/checkpc.c  common/stty.c $(COMMON_SOURCES)
lpf_SOURCES = common/lpf.c common/plp_snprintf.c
lpbanner_SOURCES = common/lpbanner.c common/plp_snprintf.c
monitor_SOURCES = common/monitor.c $(COMMON_SOURCES)
# sserver_SOURCES = AUTHENTICATE/sserver.c
# sclient_SOURCES = AUTHENTICATE/sclient.c

noinst_HEADERS = include/accounting.h include/checkpc.h include/child.h include/control.h include/copyright.h include/debug.h include/errorcodes.h include/errormsg.h include/fileopen.h include/gethostinfo.h include/getopt.h include/getprinter.h include/getqueue.h include/globmatch.h include/initialize.h include/krb5_auth.h include/license.h include/linelist.h include/linksupport.h include/lockfile.h include/lpc.h include/lpd_control.h include/lpd_dispatch.h include/lpd.h include/lpd_jobs.h include/lpd_logger.h include/lpd_rcvjob.h include/lpd_remove.h include/lpd_secure.h include/lpd_status.h include/lp.h include/lpq.h include/lpr.h include/lprm.h include/lpstat.h include/md5.h include/merge.h include/patchlevel.h include/permission.h include/plp_snprintf.h include/portable.h include/printjob.h include/proctitle.h include/readstatus.h include/sendauth.h include/sendjob.h include/sendmail.h include/sendreq.h include/ssl_auth.h include/stty.h include/user_auth.h include/utilities.h include/openprinter.h include/lpd_worker.h

# vars.c needs all the defines for defaults.
# This only adds them for vars.c, which might need GNU make
# if your make does not support it, try to add them to the
# global AM_CPPFLAGS
vars.$(OBJEXT): AM_CPPFLAGS += $(PATHDEFINES)
vars.$(OBJEXT): ../config.h

./lpd.conf: vars.c ../config.h
	set -e; \
	rm -f $@ ; \
	echo "# lpd.conf" > $@ ; \
	echo "# VERSION=$(VERSION)" >> $@ ; \
	$(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(PATHDEFINES) -C vars.c \
	 | $(SHELL) ../UTILS/make_lpd_conf >> $@ \
	 || rm $@

%: %.sh Makefile
	set -e ; \
	rm -f $@ ; \
	sed \
	-e "s,@OPENSSL.,@OPENSSL@," \
	-e "s,@SSL_CA_FILE.,@SSL_CA_FILE@," \
	-e "s,@SSL_CA_KEY.,@SSL_CA_KEY@," \
	-e "s,@SSL_CRL_FILE.,@SSL_CRL_FILE@," \
	-e "s,@SSL_SERVER_CERT.,@SSL_SERVER_CERT@," \
	-e "s,@SSL_SERVER_PASSWORD_FILE.,@SSL_SERVER_PASSWORD_FILE@," \
	-e "s,@SSL_CERTS_DIR.,@SSL_CERTS_DIR@," \
	$< >$@
	chmod 755 $@

# run make install with SAMPLESUFFIX="" to force files in their final position
# (which will overwrite everything else you have there, you have been warned)
SAMPLESUFFIX = .sample
# run make install with INSTALLCONFIGEXAMPLES=No to not install them at all
INSTALLCONFIGEXAMPLES = Yes

install-data-hook: lpd.conf
	$(mkinstalldirs) $(DESTDIR)$(configdir)
	if [ "$(INSTALLCONFIGEXAMPLES)" = "Yes" ] ; then \
	$(INSTALL_DATA) lpd.conf $(DESTDIR)$(LPD_CONF_PATH)$(SAMPLESUFFIX) ;\
	fi
