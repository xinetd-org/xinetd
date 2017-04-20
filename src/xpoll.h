/*
 * (c) Copyright 2009 by Red Hat Inc.
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */
#include "config.h"
#ifdef HAVE_POLL
#ifndef _X_POLL_H
#define _X_POLL_H

#include <poll.h>
#include "defs.h"


/* Field accessor methods for pollfd in defined in poll.h */
#define POLLFD_FD( pfd )           ( (pfd)->fd )
#define POLLFD_EVENTS( pfd )       ( (pfd)->events )
#define POLLFD_REVENTS( pfd )      ( (pfd)->revents )

/* TODO: write memory management stuff in xpoll.c if needed */

#endif /* _X_POLL_H */
#endif /* HAVE_POLL */
