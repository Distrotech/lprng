#!/bin/sh
export PATH=/bin:/usr/bin:/usr/local/bin
#echo pid $$ "$0" "$@" 1>&2
#printenv       1>&2

# see the end of this file for the low level language helper
# executable 'read_file_with_line_count'
# you will have to fix this up on your system in the appropriate manner

readfilecount=/usr/local/bin/readfilecount
removeoneline=/usr/local/bin/removeoneline


# first we get the options
#    program [-DV] -S -Pprinter -nuser -Rserveruser -Ttempfile

while [ -n "$1" ] ; do
	case "$1" in
	-D  )  debug=yes ; simulate=yes ;;
	-V  )  simulate=yes ;;
	-E  )  error=yes ;;
	-S  )  server=yes ;;
	-F  )  server=yes ; forward=yes ;;
	-C  )  client=yes ;;
	-A* )  authtype=`expr "$1" : "..\(.*\)"`;
			if [ -z "$authtype" ] ; then shift; authtype="$1"; fi ;;
	-P* )  printer=`expr "$1" : "..\(.*\)"`;
			if [ -z "$printer" ] ; then shift; printer="$1"; fi ;;
	-n* )  user=`expr "$1" : "..\(.*\)"` ;
			if [ -z "${user}" ] ; then shift; user="$1"; fi ;;
	-R* )  serveruser=`expr "$1" : "..\(.*\)"` ;
			if [ -z "$serveruser" ] ; then shift; serveruser="$1"; fi ;;
	-T* )  tempfile=`expr "$1" : "..\(.*\)"` ;
			if [ -z "$tempfile" ] ; then shift; tempfile="$1"; fi ;;
	esac;
	shift;
done;

# for testing set debug to yes
# debug=yes;

# get users home directory - note that HOME may be set to
# root's or daemon's depending on the options to the filter script

# now get the key file location
SERVERKEYFILE="${HOME}"/.pgp/serverkey;
CLIENTKEYFILE="${PGPKEYFILE:-${HOME}/.pgp/clientkey}";

serveruser="${serveruser:-`whoami`}";
#serveruser="${serveruser:-daemon}";
user="${user:-`whoami`}";

authtype="${authtype:-"pgp"}";
if [ "${authtype}" != "pgp" ] ; then
	echo "not pgp authentication" 1>&2;
	exit 1;
fi;

# file names

if [ -n "${debug}" ] ; then
	tempfile="${tempfile:-/tmp/temp}";
	if [ ! -f "${tempfile}" ] ; then
		echo "Info PID $$" >${tempfile};
	fi;
fi;

if [ -z "${tempfile}" ] ; then
	echo "missing -Ttempfile option" 1>&2;
	exit 1;
fi;

# file for holding key received by client
clientauthfile="${tempfile}".clkey;

# file to hold user key
skey="${tempfile}".skey;
ckey="${tempfile}".ckey;

# pgp encryption
cpgpfile="${tempfile}".cpgp;
spgpfile="${tempfile}".spgp;

# for error messages
serrfile="${tempfile}".serr;
cerrfile="${tempfile}".cerr;

# transfered information from server to client and vice versa
stransferfile="${tempfile}".str;
ctransferfile="${tempfile}".ctr;


if [ -n "${debug}" ] ; then
	# client added authorization to temp file
	authtemp="${tempfile}".ctm;
	# file received by server
	srecvfile="${tempfile}".sout;
else
	authtemp="${tempfile}"
	srecvfile="${tempfile}";
fi;

sstatusfile="${tempfile}".ssta;
cstatusfile="${tempfile}".csta;

# set up cleanup
remove () {
	if [ -z "$debug" -a -n "${tempfile}" ] ; then
		rm -f "${tempfile}".* >/dev/null 2>&1;
	fi;
}

cleanup () {
	remove;
	exit 1;
}

trap cleanup 1 2 3

# save the client PGP password
CLIENTPGPPASS="${PGPPASS}";

# get the key from the key file if it exists
# this file should be RO for the user
# note that this should be done only by the servers
# and users should either set this themselves or
# use the interactive method of supplying a key.

if [ -n "${debug}" ] ; then TA="-ta"; fi;
TA="-ta";
PGPINTERACTIVEFLAGS="+force";
PGPFLAGS="+batchmode +force +verbose=0";


if [ -z "${client}" -o -n "${debug}" ] ; then
	# we are the server
    # check to see if user has a key
	if [ -f  "${SERVERKEYFILE}" ] ; then
		SERVERPGPPASS=`cat "${SERVERKEYFILE}"`;
		PGPPASS="${SERVERPGPPASS}";
	fi;
	if [ -n "${debug}" ] ; then
		echo server PGPPASS "${PGPPASS}";
	fi;
	unset PGPPASSFD;
	pgp ${PGPFLAGS} -kv "${user}" >"${skey}" 2>&1;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server key extraction for \"$user\" failed- error code $err" 1>&2
		cat "${skey}" 1>&2
		cleanup;
	fi;
	keycount=`awk '/^pub/{ ++i; } END{ print i+0; }' ${skey}`;
    if [ "${keycount}" != 1 ] ; then
		echo "server keyring lookup for \"${user}\" found ${keycount} PGP keys" 1>&2;
		cat "${serrfile}" 1>&2;
		cleanup;
	fi;

	# generate a random string
	authstr=`jot -r -c 100 \
	 | awk '
		/[a-zA-Z0-9]/{ line = line $1; }
		END { print line; }'`;
	if [ -n "${debug}" ] ; then
		echo "ORIGKEY ${authstr}"
	fi;

	# now we encode it using PGP
	export PGPPASSFD=0;
	(echo "${PGPPASS}"; echo "${authstr}") | \
		pgp $PGPFLAGS $TA -fse "${user}" > "${spgpfile}" \
		-u "${serveruser}" 2>"${serrfile}"
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server authenticate generation failed- error code $err" 1>&2
		cat "${serrfile}" 1>&2
		cleanup;
	fi;

	# now we transfer it to the other end
	size=`ls -l "${spgpfile}" | awk '{ print $5; }' `;

	# redirect stdout to fd 0
	echo "${size}" >${stransferfile};
	cat "${spgpfile}" >>${stransferfile};
	if [ -z "${debug}" ] ; then
		cat "${stransferfile}" 1>&0;
	else
		echo "SERVER AUTH TRANSFERFILE" "${stransferfile}";
		cat "${stransferfile}";
	fi;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server authenticate send failed- error code $err" 1>&2
		cat "${serrfile}" 1>&2
		cleanup;
	fi;
fi;

# the client

if [ -n "${client}" -o -n "${debug}" ] ; then
	# check for missing file to send
	if [ ! -f "${tempfile}" ] ; then
		echo "missing data file ${tempfile}";
		cleanup;
	fi;
 	# you need this low level language helper function
	if [ -z "${CLIENTPGPPASS}" -a -f  "${CLIENTKEYFILE}" ] ; then
		CLIENTPGPPASS=`cat $CLIENTKEYFILE`;
	fi;
	if [ -n "${CLIENTPGPPASS}" ] ; then
		PGPPASS="${CLIENTPGPPASS}";
	fi;
	# echo client PGPPASS \"${PGPPASS}\" 1>&2; 
	if [ -n "${debug}" ] ; then
		echo client PGPPASS \"$PGPPASS\";
	fi;
	unset PGPPASSFD;
	pgp ${PGPFLAGS} -kv "${serveruser}" >"${ckey}" 2>&1;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "client - user '$user' key extraction for \"${serveruser}\" failed- error code $err" 1>&2
		cat "${ckey}" 1>&2;
		cleanup;
	fi;
	keycount=`awk '/^pub/{ ++i; } END{ print i+0; }' ${ckey}`;
    if [ "${keycount}" != 1 ] ; then
		echo "server keyring lookup for \"${serveruser}\" found ${keycount} PGP keys" 1>&2;
		cat "${serrfile}" 1>&2;
		cleanup;
	fi;

	# read pgp file from the server
	if [ -n "${debug}" ] ; then
		${readfilecount} > "${cpgpfile}" <"${stransferfile}" 2>"${cerrfile}";
	else
		${readfilecount} > "${cpgpfile}"                    2>"${cerrfile}";
	fi;
	err="$?";
	if [ "$err" -ne 0 ] ; then
 		echo "client receive authenticate failed- error code $err" 1>&2
 		cat "${cerrfile}" 1>&2
 		cleanup;
 	fi;

	#echo "RECEIVED" 1>&2;
	#cat "${cpgpfile}" 1>&2;
	#echo "RECEIVED END" 1>&2;

 	# decode the file
	export PGPPASSFD=0;
	echo "${PGPPASS}" | \
		pgp $PGPFLAGS "${cpgpfile}" -o "${clientauthfile}" 2>"${cerrfile}" 1>&2;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		unset PGPPASSFD;
		pgp $PGPINTERACTIVEFLAGS "${cpgpfile}" -o "${clientauthfile}" 2>"${cerrfile}";
		err="$?";
	fi;
	if [ "$err" -ne 0 ] ; then
 		echo "client pgp decode interactive authenticate failed- error code $err" 1>&2
 		cat "${cerrfile}" 1>&2
 		cleanup;
 	fi;

 	# we get the first line in the file for the key
 	authkey=`head -1 "${clientauthfile}"`;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "client bad authentication files- error code $err" 1>&2;
		cat "${cerrfile}" 1>&2
		cleanup;
	fi;

 	if [ -n "${debug}" ] ; then
		echo AUTHKEY "${authkey}";
		if [ "${authkey}" != "${authstr}" ] ; then
			echo "client different authentication \"${authkey}\", \"${authstr}\"" 1>&2;
			cleanup;
		fi;
	fi;

	# now we encode the temp file using PGP
	if [ -n "${debug}" ] ; then
		cat "${tempfile}" > "${authtemp}"
		echo >>"${authtemp}";
		echo "${authkey}" >>"${authtemp}";
	else
		echo >>"${tempfile}";
		echo "${authkey}" >>"${tempfile}";
	fi;

	export PGPPASSFD=0;
	echo "$PGPPASS" | \
		pgp $PGPFLAGS $TA -se "${authtemp}" "${serveruser}" \
		-o "${cpgpfile}" -u "${user}" 2>"${cerrfile}" 1>&2;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		unset PGPPASSFD;
		pgp $PGPINTERACTIVEFLAGS $TA -se "${authtemp}" "${serveruser}" \
		-o "${cpgpfile}" -u "${user}" 2>"${cerrfile}";
		err="$?";
	fi;
	if [ "$err" -ne 0 ] ; then
 		echo "client pgp interative temp generation failed- error code $err" 1>&2
 		cat "${cerrfile}" 1>&2
 		cleanup;
 	fi;

	# now we transfer it to the other end
	size=`ls -l "${cpgpfile}" | awk '{ print $5; }' `;

	echo "${size}" >${ctransferfile};
	cat "${cpgpfile}" >>${ctransferfile};
	# use fd 0 == stdout
	if [ -z "${debug}" ] ; then
		cat "${ctransferfile}" 1>&0;
	else
		echo "CLIENT TEMP TRANSFERFILE" "${ctransferfile}";
		cat "${ctransferfile}";
	fi;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server authenticate send failed- error code $err" 1>&2
		cat "${cerrfile}" 1>&2
		cleanup;
	fi;
fi;

# the server side

if [ -z "${client}" -o -n "${debug}" ] ; then
	# we are the server
    # check to see if server has a key
	export PGPPASSFD=0;
	if [ -n "${SERVERPGPPASS}" ] ; then
		PGPPASS="${SERVERPGPPASS}";
	fi;
	if [ -n "${debug}" ] ; then
		echo server PGPPASS $PGPPASS;
	fi;
	if [ -n "${debug}" ] ; then
		${readfilecount} > "${spgpfile}" <"${ctransferfile}" 2>"${serrfile}";
	else
		${readfilecount} > "${spgpfile}"                    2>"${serrfile}";
	fi;
	err="$?";
	if [ "$err" -ne 0 ] ; then
 		echo "server receive tempfile failed- error code $err" 1>&2
 		cleanup;
 	fi;
 	# decode the file
	export PGPPASSFD=0;
	echo "${PGPPASS}" | \
 		pgp $PGPFLAGS "${spgpfile}" -o "${srecvfile}" >"${serrfile}" 2>&1 ;
	err="$?";
	if [ "$err" -ne 0 ] ; then
 		echo "server pgp decode file failed- error code $err" 1>&2
 		cat "${serrfile}" 1>&2
 		cleanup;
 	fi;

	# Send the server the signature
	# signature=`sed -n -e "/key ID/s/.*ID //p" "${serrfile}"`
	signature=`sed -n -e "/</s/.*<\(${user}@\)/\1/" -e "s/>.*//p" "${serrfile}"`

	#echo "SERVER info received from client" 1>&2;
	#cat "${srecvfile}" 1>&2;
	#echo "SERVER info end" 1>&2;

 	# we get the first line in the file for the key
 	checkkey=`${removeoneline} "${srecvfile}"`
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "cannot get last line in file - error code $err" 1>&2;
		cleanup;
	fi;
	if [ -n "${debug}" ] ; then echo CHECKEY "${checkkey}"; fi;

	# check to see if sent and received authorization same
	if [ "$checkkey" != "${authstr}" ] ; then
		echo "bad authentication - sent ${authstr}, got ${checkkey}" 1>&2;
		cleanup;
	fi;

	# check to see if what we sent is what we received
 	if [ -n "${debug}" ] ; then
 		diff -c "${srecvfile}" "${tempfile}" >${serrfile} 2>&1;
		err="$?";
		if [ "$err" -ne 0 ] ; then
 			echo "different transferred files- error code $err" 1>&2;
			cat "${serrfile}" 1>&2
 			cleanup;
 		fi;
	fi;

	if [ -n "${debug}" ] ; then
		echo SIGNATURE $signature;
	else
		echo $signature;
	fi;

	# now we encrypt and send back the status - we fake this with
	# the received file for testing

 	if [ -z "${debug}" ] ; then
		cat <&3 >"${sstatusfile}";
		# remove comments for debugging
		# echo "STATUS" 1>&2
		# cat "${sstatusfile}" 1>&2;
		# echo "STATUS END" 1>&2
	else
		echo "Sending client back same information" 1>&2
		cat "${srecvfile}" > "${sstatusfile}"
	fi;

	# now we encode it using PGP
	echo "${PGPPASS}" | \
	pgp $PGPFLAGS $TA -se "${sstatusfile}" "${user}" -o "${spgpfile}" \
		-u "${serveruser}" >"${serrfile}" 2>&1
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server errorfile generation failed- error code $err" 1>&2
		cat "${serrfile}" 1>&2
		cleanup;
	fi;

	# now we transfer it to the other end
	size=`ls -l "${spgpfile}" | awk '{ print $5; }' `;

	# redirect stdout to fd 0
	echo "${size}" >${stransferfile};
	cat "${spgpfile}" >>${stransferfile};
	if [ -z "${debug}" ] ; then
		cat "${stransferfile}" 1>&0;
	else
		echo "SERVER STATUS TRANSFERFILE" "${stransferfile}";
		cat "${stransferfile}";
	fi;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		echo "server authenticate send failed- error code $err" 1>&2
		cat "${serrfile}" 1>&2
		cleanup;
	fi;
fi;


# the client

if [ -n "${client}" -o -n "${debug}" ] ; then
 	# you need this low level language helper function
	if [ -n "${CLIENTPGPPASS}" ] ; then
		PGPPASS="${CLIENTPGPPASS}";
	fi;
	if [ -n "${debug}" ] ; then
		echo client PGPPASS $PGPPASS;
	fi;
	if [ -n "${debug}" ] ; then
		cat > "${ctransferfile}" <"${stransferfile}" 2>"${cerrfile}";
	else
		cat > "${ctransferfile}"                     2>"${cerrfile}";
	fi;
	${readfilecount} > "${cpgpfile}" <"${ctransferfile}" 2>"${cerrfile}";
	err="$?";
	if [ "$err" -ne 0 ] ; then
 		echo "client receive statusfile failed- error code $err" 1>&2
 		cat "${cerrfile}" 1>&2
		if [ -n "${debug}" ] ; then
			echo; echo RAW FILE;
		fi;
 		cat "${ctransferfile}"
 		cleanup;
 	fi;
 	# decode the file
	export PGPPASSFD=0;
	echo "$PGPPASS" | \
 		pgp $PGPFLAGS "${cpgpfile}" -o "${cstatusfile}" 2>"${cerrfile}" 1>&2;
	err="$?";
	if [ "$err" -ne 0 ] ; then
		unset PGPPASSFD;
		pgp $PGPINTERACTIVEFLAGS "${cpgpfile}" -o "${cstatusfile}" 2>${cerrfile};
		err="$?";
	fi;
	if [ "$err" -ne 0 ] ; then
 		echo "client pgp decode interactive authenticate failed- error code $err" 1>&2
 		cat "${cerrfile}" 1>&2
 		cleanup;
 	fi;

 	if [ -n "${debug}" ] ; then
 		diff -c "${srecvfile}" "${cstatusfile}" >${cerrfile} 2>&1;
		err="$?";
		if [ "$err" -ne 0 ] ; then
 			echo "different authentication files- error code $err" 1>&2;
			cat "${cerrfile}" 1>&2
 			cleanup;
 		fi;
	else
		#ls -l ${cstatusfile} 1>&2
		#hexdump -c ${cstatusfile} 1>&2
		cat "${cstatusfile}"
 	fi;
fi;

if [ -n "${debug}" ] ; then
	echo successful;
fi;
remove;
exit 0;
