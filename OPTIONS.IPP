This version experimentally implements IPP server (it cannot send jobs to IPP printers).


Printer URI:

    http[s]://server:ipp[s]_listen_port/raw/printername ("raw" URI, raw processing)
    http[s]://server:ipp[s]_listen_port/printers/printername ("printers" URI,
                                                         postscript processing)


Configuration options:

    ipp_listen_port (string)

	IPP listening port, default 631

    ipps_listen_port (string)

        listening port for direct https encrypted connections, default off

    ipp_getjobs_compat (integer)

        server accepts also CUPS-specific URIs: 1 = newer CUPS paths (default),
        2 = older, 3 = both, 0 = do not accept CUPS URIs

    ipp_compat_hrcount (integer)

        mamimum number of jobs in queue, needed for job-id transformation in CUPS-compatible
        modes (ipp_getjobs_compat = 1 - 3), default value 100

    ssl_ca_path, ssl_ca_file, ssl_server_cert, ssl_server_password_file

        parametres to setup SSL communication (see LPRng manual); IPP server supports
        direct https connections and http upgrade (prefered) way (both TLS/1.0)

    default_printer (string)

        default CUPS printer

    printer alias (in printcap)

        send as printer IPP info (printer-make-and model and printer-info)

    cm (string)

        comment send as printer-location IPP attribute

    iauth (string)

        comma separated list of allowed authentication methods: usrname (default)
        and basic (http basic authentication, "lprng" PAM service)

    pppd (string)

        definitions of ppd files for printers are recommended, CUPS clients can download
        the files to show and send printing options


Permission keywords (lpd.perms)


    IPP

        true if client connected to IPP port

    AUTHTYPE=keyword

        authentication types for IPP are usrname or basic; note that different IPP clients
        may require different authentication types for various IPP operations to work
        properly

    PPATH=keyword

        PPATH=raw : connected to "raw" URI,
        PPATH=printers : connected to "printers" URI



Permission required for IPP operations


Operation                             Required permission
Print-Job, Validate-Job, Create-Job   SERVICE=R
                                      SERVICE=C for accept job-hold-until job template attribute
Send-Document                         SERVICE=R
Cancel-Job                            SERVICE=M for job or SERVICE=C for queue
Get-Job-Attributes, Get-Jobs          SERVICE=Q
Get-Printer-Attributes,               SERVICE=S
CUPS-Get-Printers
Hold-Job                              SERVICE=C LPC=hold
Release-Job                           SERVICE=C LPC=release
Restart-Job                           SERVICE=C LPC=restart
Change-Job-Attributes                 SERVICE=C change job job-hold-until attribute: LPC=hold or LPC=release
                                                change job job-priority attribute: LPC=chattr-job-priority
Create-Printer-Subscriptions,         SERVICE=S
Get-Subscription-Attributes,
Get-Subscriptions, Renew-Subscription,
Cancel-Subscription, Get-Notifications
CUPS-Get-Default                      SERVICE=S
                                      and
                                      (SERVICE=d for the default printer
                                       (use SERVICE=d REMOTEHOST=host to specify default printer for particular host; first rule is used)
                                      or set default_printer in lpd.conf  globally)
CUPS-Move-Job                         SERVICE=C LPC=move PRINTER=current_printer
                                      and
                                      SERVICE=P PRINTER=destination_printer

 