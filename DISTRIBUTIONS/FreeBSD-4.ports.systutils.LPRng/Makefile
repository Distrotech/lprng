# New ports collection makefile for:	LPRng
# Version required:	3.6.8
# Date created:		10 Oct 2001
# Whom:			papowell@lprng.com
#
# $FreeBSD: ports/sysutils/LPRng/Makefile,v 1.10 1999/09/27 01:50:00 steve Exp $
#

PORTNAME=LPRng
PORTVERSION=3.7.9
CATEGORIES=	    sysutils print
MASTER_SITES=	ftp://ftp.lprng.com/pub/LPRng/LPRng/
EXTRACT_SUFX=	.tgz

MAINTAINER=	papowell@astart.com

#RUN_DEPENDS=	ifhp:${PORTSDIR}/print/ifhp

PREFIX?=/usr

HAS_CONFIGURE=	yes
GNU_CONFIGURE=	yes
INSTALLS_SHLIB=	yes

CONFIGURE_ARGS= \
	--prefix=${PREFIX} \
	--sysconfdir=/etc \
	--with-ldopts="-L/usr/local/lib" \
	--with-ccopts="-I/usr/local/include" \
	--with-filterdir=${PREFIX}/libexec/filters \
	--with-ld_library_path="${PREFIX}/lib:/lib:/usr/lib:/usr/local/lib" \
	--with-filter_path="${PREFIX}/bin:/bin:/usr/bin:${PREFIX}/sbin:/sbin:/usr/sbin"
	

MAN1=cancel.1 lp.1 lpbanner.1 lpf.1 lpq.1 lpr.1 lprm.1 lpstat.1 monitor.1 pclbanner.1 psbanner.1
MAN5=lpd.conf.5 lpd.perms.5 printcap.5
MAN8=checkpc.8 lpc.8 lpd.8

post-install:
.if !defined(NOPORTSDOCS)
	${MKDIR} ${DESTDIR}${PREFIX}/share/doc/${DISTNAME}
	${CP} ${WRKSRC}/HOWTO/* ${DESTDIR}${PREFIX}/share/doc/${DISTNAME};
.endif
	@${CAT} pkg-message

.include <bsd.port.mk>
