INDEX

 - description
 - installation
 - what's new in this version
 - usage
 - examples
 - future improvements
 - copyright
 - authors
 - contributors


DESCRIPTION:

Blat is a Public Domain (generous aren't we?) Windows NT console utility that
sends the contents of a file in an e-mail message using the SMTP protocol.
Blat is useful for creating scripts where mail has to be sent automatically
(CGI, backups, etc.) To use Blat you must have access to a SMTP server via
TCP-IP. Blat uses the a DLL ("gensock" or "gwinsock") from WinVN, the public
domain usenet newsreader for windows. Blat can store a default SMTP server
address and a default "From:" field in the registry. The server's address can
be overriden using the -server flag, and the "From:" address using the -f
flag. Input from the console (stdin) can be used instead of a disk file (if
the special filename '-' is specified). Blat can also "carbon copy" and
"blind carbon copy" the message. Impersonation can be done with the -i flag
which puts the value specified in the "From:" line, however when this is done
the real senders address is stamped in the "Reply-To:" and "Sender:" lines.
This feature can be useful when using the program to send messages from NT
users that are not registered on the SMTP host.

Optionally, blat can also attach multiple binary files to your message.


INSTALLATION:

If you are upgrading from version 1.2 or later, simply copy Blat.exe
over the old one.  Blat no longer needs gensock.dll or gwinsock.dll.
You can delete these unless another application you use requires them.

If you are upgrading from Blat 1.1 or 1.0 (phew!) or you never used Blat before
you must follow these steps:

1) Copy the file "Blat.exe" to your "\WINNT\SYSTEM32" directory, or to any
   other directory in your path.

2) Run "Blat -install yourhost.site.blah.blah youruserid@site.blah.blah"


WHAT'S NEW IN THIS VERSION

Version 1.8.5b 2000.03.13
  - Fixed -body

Version 1.8.5 2000.02.08
  - RFC compliant options added by Skough Axel IT-S <axel.skough@scb.se>
    -mailfrom <addr>   The RFC 821 MAIL From: statement
    -from <addr>       The RFC 822 From: statement
    -replyto <addr>    The RFC 822 Reply-To: statement
    -returnpath <addr> The RFC 822 Return-Path: statement
    -sender <addr>     The RFC 822 Sender: statement
  - Migrated -t to -to, -c to -cc, -b to -bcc, -s to -subject, -o to -organization

Version 1.8.4e 2000.01.14
  - -body translates '|' into CRLF

Version 1.8.4d 99.11.22
  - Added "-body" parameter (requires argv[1]=="-")
  - Fixed problem with missing To/CC headers

Version 1.8.4b 99.09.22
  - buffer from 255 to 2048 chars (I forget which one)
  - -x fixed
  - -noh2 fixed (-noh or -noh2 was messing up headers)
  - single quotes in email addresses used to mean 'comment'.  Removed.
  - Replaced "|" in if statement with "||" inside of ConvertToQuotedPrintable(). 99.9.20
  - Added -enriched and -html options (Courtesy of James Greene)
  - Enhanced support for paths in -attach files (Bergamini Angelo <Angelo.Bergamini@sangiorgio.it>)
  - Added df/cf/bf command line options to support using email addresses from a file
    (Kenneth Massey <kmassey@mratings.com>)
  - Added header when mail is sent using cgi.

Version 1.8.4 99.02.20
  - Much improved support for use within CGI scripts (by Gilles Vollant)
    (see included blatcgi.txt file)
  - Minor change to "Date:" header generation code.  Although it was Y2K
    compliant, it wasn't obvious.  This was fixed to remove the confusion.
  - Added support for customized "X-" style headers. (-x)

Version 1.8.3 99.02.??
  - Internal development release

Version 1.8.2d 99.01.??
  - Added -debug flag

Version 1.8.2c 98.11.30
  - Added optional "Return-Receipt-To:" and "Disposition-Notification-To:"
    headers.
  - It's now possible to run blat without an install.  You must specify
    the "-server" and "-f" parameters (at a minimum) on the command line.

Version 1.8.2b 98.11.30
  - Removed all calls in gensock to MessageBox()
  - Fixed timeout when configured for retries.  (was using ms instead of s)

Version 1.8.2a 98.11.20
  - "-q" option was still printing "Attached text file: attach.txt" messages

Version 1.8.2 98.11.17
  - Oops.  was still identifying itself as 1.8
  - Added terminating boundary to any messages with attachments.
  - Autodetection of file types in MIME headers (from
    Toby Korn <tkorn@snl.com>)

Version 1.8.1 98.11.15
  - Fixed Multiline response treatment. (courtesy of Wolfgang
    Schwart <schwart@wmd.de>)
  - Fixed ability to use "-" filenames.  Ctrl-Z detection was broken.
  - Fixed use of "-noh"/"-noh2" option (also broken in 1.8).

Version 1.8     98.10.29
   1.8 is a merge of 2 different source trees which started diverging
   at version 1.5.  If you're upgrading from the prior "official"
   release, then changes include:

   - Use of "-attach *.ZIP" to send multiple files
   - Use of 'profiles' to store server/userid/n_of_tries/port in the
     registry for multiple profiles.
   - Multiple retries
   - Other changes as described below labeled version 1.6-1.7.9

   If you're upgrading from 1.7.9 at
     http://www.interlog.com/~tcharron/blat.html, then changes include:
   - gwinsock.dll/gensock.dll are no longer needed
   - UUencoding of the main file is possible
   - Graceful termination of connection to smtp server
   - hostname <hst> option to select the hostname used to send the
     message
   - Ability to specify port using "server:port" style names. (ports
     specified this way will override any specified with "-port")

Version 1.7.9   98.9.16
 - omission of a terminating boundary was causing some e-mail systems
   to not process the message properly. (Toby Korn tkorn@snl.com)

Version 1.7.8d  98.9.1
 - Added "-noh" command line option to prevent X-Mailer header from
   showing homepage of blat.  Added "-noh2" to prevent X-Mailer
   header entirely

Version 1.7.8c  98.8.25
 - Changed copyright limitations to prohibit use as a spam tool

Version 1.7.8b  98.8.11
 - Cleaned up some wild pointers that was a potential crash
   (although crashes have not been reported, better safe than sorry...)

Version 1.7.8   98.8.8
 - Support for non-standard character sets in the header fields (hopefully).

Version 1.7.7   98.8.8
 - Fixed problem with wildcards/filename parsing when compile with
   Watcom

version 1.7.6
 - Made output cleaner when wildcards used
 - Increased limit on attachments to 64 files.
 - Message Boundary is a random string instead of a fixed string
   (allows sending 2 or more blat messages as attachements to a 3rd
   message using BLAT)

version 1.7.5
 - Added a delay of 15 seconds between retries
 - Added the ability to use wildcards (*.*, ?) in "-attach" and "-attacht" options
 - Changed the way file "-" was read, removing phantom character that was appearing

version 1.7.4
 - Minor correction to return values in the event of server timeout

version 1.7.3
 - Fixed "-attach"ing of largish files (but in calculation of internal
   space required).

version 1.7.2
 - Added "-attacht" which enables attaching multiple files without base64
   encoding them.

version 1.7.1
 - Minor command line parsing bugs fixed

version 1.7
 - Added command line option "-port" to allow usage of a proxy listening on
   many ports and redirecting toward many SMTP servers.
 - Added command line option "-try" to avoid "time-out" error message when
   using phone-line connection or "Too many users connected" error message when
   using a proxy.
 - Added these options to the registry. Added profiles to the registry for ease
   in choosing server, user, retries and port.
 - Added a "-profile" to see the existing profiles.
 - Added a "-profile -delete" to delete a profile.

version 1.6.3
 - Added command line option "-attach" to allow attaching multiple
   binary files to a message.
 - "-base64" causes inclusion via 'attachment' method instead of 'inline'

version 1.6.2
 - Added command line option "-o" to provide Organization field
   in the headers of the sent message.

version 1.6
 - Added support for mailing of binary files (mime base64 encoding)
   which will make it possible to mail ANY file.
 - Fixed argument parsing for '-q' and '-mime' options (these used
   to only work if they were the last argument)

version 1.5
 - Two bugs were corrected that made Blat generate exceptions with certain
   CC or BCC adresses.
 - Blat now generates descriptive error messages, rather than the infamous
   "gensock error 4017" type errors
 - Blat now returns an error code when the SMTP session fails
 - A new option -mime was added. This implements the possibility to use
   the MIME quoted-printable transfer encoding. The assumptions are:
        1: The file is a text file
        2: The charset is ISO 8859/1


USAGE:

syntax:
  Blat <filename> -to <recipient> [optional switches (see below)]
  Blat -install <server addr> <sender's addr> [<try>[<port>[<profile>]]] [-q]
  Blat -profile [-delete | "<default>"] [profile1] [profileN] [-q]
  Blat -h [-q]

-install <server addr> <sender's addr> [<try n times> [<port> [<profile>]]]
     : set's SMTP server, sender, number of tries and port for profile
       (<try n times> and <port> may be replaced by '-').

<filename>     : file with the message body ('-' for console input, end with ^Z)
-to <recipient> : recipient list (also -t) (comma separated)
-tf <recipient> : recipient list filename
-subject <subj>: subject line (also -s)
-f <sender>    : overrides the default sender address (must be known to server)
-i <addr>      : a 'From:' address, not necessarily known to the SMTP server.
-cc <recipient>: carbon copy recipient list (also -c) (comma separated)
-cf <file>     : cc recipient list filename
-bcc <recipient>: blind carbon copy recipient list (also -bcc) (comma separated)
-bf <file>     : bcc recipient list filename
-organization <organization>: Organization field (also -o and -org)
-body <text>   : Message body
-x <X-Header: detail>: Custom 'X-' header.  eg: -x "X-INFO: Blat is Great!"
-r             : Request return receipt.
-d             : Request disposition notification.
-h             : displays this help.
-q             : supresses *all* output.
-debug         : Echoes server communications to screen (disables '-q').
-noh           : prevent X-Mailer header from showing homepage of blat
-noh2          : prevent X-Mailer header entirely
-p <profile>   : send with SMTP server, user and port defined in <profile>.
-server <addr> : Specify SMTP server to be used. (optionally, addr:port)
-port <port>   : port to be used on the server, defaults to SMTP (25)
-hostname <hst>: select the hostname used to send the message
-mime          : MIME Quoted-Printable Content-Transfer-Encoding.
-enriched      : Send an enriched text message (Content-Type=text/enriched)
-html          : Send an HTML message (Content-Type=text/html)
-uuencode      : Send (binary) file UUEncoded
-base64        : Send (binary) file using base64 (binary Mime)
-try <n times> : how many time blat should try to send. from '1' to 'INFINITE'
-attach <file> : attach binary file to message (may be repeated)
-attacht <file>: attach text file to message (may be repeated)
-ti <n>        : Set timeout to 'n' seconds.

Note that if the '-i' option is used, <sender> is included in 'Reply-to:'
and 'Sender:' fields in the header of the message.

Optionally, the following options can be used instead of the -f and -i
options:

-mailfrom <addr>   The RFC 821 MAIL From: statement
-from <addr>       The RFC 822 From: statement
-replyto <addr>    The RFC 822 Reply-To: statement
-returnpath <addr> The RFC 822 Return-Path: statement
-sender <addr>     The RFC 822 Sender: statement

For backward consistency, the -f and -i options have precedence over these
RFC 822 defined options. If both -f and -i options are omitted then the 
RFC 821 MAIL FROM statement will be defaulted to use the installation-defined 
default sender address

EXAMPLES:

Blat -install smtphost.bar.com foo@bar.com          // Sets host and userid
Blat -install smtphost.bar.com foo                  // Sets host and userid
Blat -install smtphost.bar.com                      // Sets host only

Blat myfile.txt -subject "A file for pedro" -to foo@bar.com
// Sends a file with subject line "A file for pedro"

Blat myfile.txt -subject "A file for pedro" -to foo@bar.com -q
// Sends a file with subject line "A file for pedro" and does not print
// informative messages on the console

Blat myfile.txt -subject "A file for mark" -to fee@fi.com -f foo@bar.com
// -f option overrides the default sender

Blat myfile.txt -subject "A file for pedro" -to foo@bar.com -i "devil@fire.hell"
// -i replaces "From:" line address (but leaves Reply-To: and Sender: lines)

Blat myfile.txt -subject "animals" -to fee@fi.com -cc "moo@grass.edu,horse@meadow.hill"
// -c mails carbon copies to users moo@grass.edu and horse@meadow.hill

Blat.exe BLAT.ZIP -subject "file to gil" -to foo@bar.com -base64
// Sends the binary file BLAT.ZIP to Gilles in MIME Base 64 format

Blat.exe BLAT.ZIP -subject "file to gil" -to foo@bar.com -uuencode
// Sends the binary file BLAT.ZIP to Gilles in the old UUEncode format

Blat myfile.txt -subject "oumpla" -to foo@bar.com -attach c:\myfolder\*.txt
// Sends a file with subject line "oumpla", attach all files with extension "TXT"
// in folder "myfolder" on drive "C:". Note: you must specify a filename/pattern,
// "-attach .\*.*" will send all files in current folder.
// "-attach ." will not send anything AND WILL NOT GIVE YOU ANY WARNING.

Blat myfile.txt -to fee@fi.com -server smtp.domain.com -port 6000
// sends the message through SMTP server smtp.domain.com at port 6000

Blat myfile.txt -to fee@fi.com -hostname friend
// tells the SMTP that this computer is called "friend"


COPYRIGHT

License to use Blat

The authors of Blat have placed it in the public domain.  This means you
can use it free of charge for any purpose you like, with nearly no
conditions being placed on its use by us.  The source code is also
available free of charge and under the same conditions as the executables.

You have permission to modify, redistribute, hoard, or even sell Blat in its
executable or source form. If you do sell Blat, though, we'd appreciate it if
you'd provide your own support (and send us a free copy).  We cannot take any
support load for Blat (we've got better things to do).

The only limitation we impose is that blat not be used to send unsolicited
commercial email.  Use of this software to send unsolicited commercial
email constitutes an agreement to pay the authors $10,000.

Various bits of the source code are copyright by other people/organizations.
Look in the source code for copyright ownership.

The authors and contributors of the package are not responsible for any damage
or losses that the usage of Blat may cause. We are especially not responsible
for the misuse of the SMTP (or other) mail system.


AUTHORS

Mark Neal    (mjn@aber.ac.uk)
Pedro Mendes (prm@aber.ac.uk)
Gilles Vollant (info@winimage.com)
Tim Charron (tcharron@interlog.com)


CONTRIBUTORS

We'd like to thank:

the WinVN team                       - wrote gensock.dll and the excellent WinVN
                                       newsreader from which we copied most of
                                       Blat's code (PD apps are nice!)
Beverly Brown (beverly@datacube.com) - fixed the argument parsing
Bob Beck (rbk@ibeam.intel.com)       - added console input (even though we did
                                       not use his code in the end...)
Axel Skough (axel.skough@scb.se)     - added the MIME code, fixed the CC/BCC
                                       bug, added the return code, added
                                       RFC compliant header support
Tim Charron (tcharron@interlog.com)  - Added base64 encoding code
                                     - Added multiple binary "-attach"ments
                                     - Misc. features and fixes
Christophe Henquin (ch@innocent.com) - Added -port, -try and -profile support
Toby Korn (tkorn@snl.com)            - Bugfixes, MIME type autodetection
Stephen Gun (GUNS@banksa.com.au)     - Bugfixes

