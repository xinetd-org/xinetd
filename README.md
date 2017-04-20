# xinetd #

[![Build Status](https://travis-ci.org/openSUSE/xinetd.svg?branch=master)](https://travis-ci.org/openSUSE/xinetd)

xinetd is a powerful replacement for inetd.

If you are planning to use xinetd on recent linux distribution also consider using [systemd socket activation](http://0pointer.de/blog/projects/socket-activation.html) instead.

Original site (DEAD): http://www.xinetd.org

xinetd has access control mechanisms, extensive logging capabilities,
the ability to make services available based on time, can place
limits on the number of servers that can be started, and has deployable
defence mechanisms to protect against port scanners, among other things.

There are a number of differences between xinetd and inetd. The 
largest difference to the end user is the config file.  xinetd's 
config file format is more C like, and somewhat similar to bind 8's.

# Access Control

xinetd keeps all the names you specify on the
access control directives. When a client attempts to connect to
a service, a reverse lookup is performed on the client's IP address.
The canonical name returned is compared with the specified names.
If the first character of the name being specified in the config
file is a '.', then all hosts within that domain are matched.
For example, if I put .synack.net, all hosts with a reverse mapping
that are in .synack.net domain, are matched.

# libwrap support

For libwrap access control, the access control is done by the
server name for the service.  So, if you have an entry like this:
service telnet
~~~
{
	...
	server = /usr/sbin/in.telnetd
	...
}
~~~
Your corresponding `hosts.{allow|deny}` entry would look something
like this:
~~~
in.telnetd: ALL
~~~

However, many services don't have a "server".  Internal services
and redirection services don't have a "server" line in the configuration
file.  Fma these services, the service name is used.  For example:
~~~
server telnet
{
	...
	redirect = 10.0.0.1 23
	...
}
~~~
Your `hosts.{allow|deny}` entry would look something like this:
telnet: ALL

So, in general, if a service has a "server" attribute to it, access
control is performed based on that entry.  If a service does not have
a "server" attribute, (internal and redirection services) then access
control is based on the service name.
This is only for libwrap access control.

# History

xinetd was originally written by panos@cs.colorado.edu.  At least one other
version of xinetd has been seen floating around the net.  Another version is
maintained by Rob Braun (bbraun@synack.net) and bug reports for that
version should be directed to https://github.com/xinetd-org/xinetd/.

This version is simple collection of patches contained over Rob Brauns version that were present in all major distributions.
Plans are to include fixes as required for keeping it workable in openSUSE and also to merge commits from the above github branch.

# Issues

Bug reports/comments/suggestions/flames for this version should be sent
to https://github.com/openSUSE/xinetd/issues/
