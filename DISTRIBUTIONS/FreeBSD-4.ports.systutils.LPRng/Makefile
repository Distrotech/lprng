# New ports collection makefile for:	LPRng
# Date created:		10 Oct 2001
# Whom:			papowell@lprng.com
#
# $FreeBSD: ports/sysutils/LPRng/Makefile,v 1.10 1999/09/27 01:50:00 steve Exp $
#

PORTNAME=LPRng
PORTVERSION=3.8.2
CATEGORIES=	 sysutils print
MASTER_SITES=	ftp://ftp.lprng.com/pub/LPRng/LPRng/ \
	ftp://ftp.cise.ufl.edu/pub/mirrors/%SUBDIR%/ \
	ftp://ftp.cs.umn.edu/pub/%SUBDIR%/ \
	ftp://ftp.informatik.uni-hamburg.de/pub/os/unix/utils/%SUBDIR%/ \
	ftp://ftp.uni-paderborn.de/pub/unix/printer/%SUBDIR%/
MASTER_SITE_SUBDIR=  LPRng/LPRng
EXTRACT_SUFX=	.tgz

MAINTAINER=	papowell@astart.com

#RUN_DEPENDS=	ifhp:${PORTSDIR}/print/ifhp

PREFIX?=/usr

HAS_CONFIGURE=	yes
GNU_CONFIGURE=	yes
INSTALLS_SHLIB=	yes

SYSCONFDIR?=${PREFIX}/etc

CONFIGURE_ARGS=  --prefix="${PREFIX}" \
	--with-sysconfdir="${SYSCONFDIR}" \
	--with-ldopts="-L${LOCALBASE}/lib" \
	--with-ccopts="-I${LOCALBASE}/include" \
	--with-filterdir=${PREFIX}/libexec/filters \
	--with-ld_library_path="${PREFIX}/lib:/lib:/usr/lib:/usr/local/lib" \
	--with-filter_path="${PREFIX}/bin:/bin:/usr/bin:${PREFIX}/sbin:/sbin:/usr/sbin"

MAN1=        cancel.1 lp.1 lpbanner.1 lpf.1 lpq.1 lpr.1 lprm.1 lpstat.1 monitor.1 pclbanner.1 psbanner.1
MAN5=        lpd.conf.5 lpd.perms.5 printcap.5
MAN8=        checkpc.8 lpc.8 lpd.8

pre-everything::
	@${ECHO_MSG} "If you are SURE and want to replace the system's lpd with"
	@${ECHO_MSG} "LPRng's, you can set PREFIX=/usr SYSCONFDIR=/etc"
	@${ECHO_MSG} "Just do 'make PREFIX=/usr SYSCONFDIR=/etc'"

post-install:
.if !defined(NOPORTSDOCS)
	@${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0555 ${DOCSDIR}
.for ext in html pdf ppt ps txt gif jpg png
	@${INSTALL_DATA} ${WRKSRC}/HOWTO/*.${ext} ${DOCSDIR}
.endfor
.endif
	@${SED} -e "s!/usr/local/!${PREFIX}/!" ${PKGMESSAGE}

.include <bsd.port.mk>
