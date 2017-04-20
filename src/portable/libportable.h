#ifndef _XINETD_LIBPORTABLE
#define _XINETD_LIBPORTABLE 1

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef IPV6_ADDRFORM
#define IPV6_ADDRFORM            1
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef APPEND
#define APPEND(a, b) APPEND2 (a, b)
#endif

#ifndef APPEND2
#define APPEND2(a, b) a##b
#endif

#ifndef FLOAT_TYPE
#define FLOAT_TYPE double
#endif

#ifndef FUNC_PREFIX
#define FUNC_PREFIX 
#endif

/* from OpenSSH's fake-socket.h */

#ifndef IN6_IS_ADDR_LOOPBACK
# define IN6_IS_ADDR_LOOPBACK(a) \
	(((uint32_t *) (a))[0] == 0 && ((uint32_t *) (a))[1] == 0 && \
	 ((uint32_t *) (a))[2] == 0 && ((uint32_t *) (a))[3] == htonl (1))
#endif /* !IN6_IS_ADDR_LOOPBACK */

#ifndef AF_INET6
/* Define it to something that should never appear */
#define AF_INET6 AF_MAX
#endif


#endif /* _XINETD_LIBPORTABLE */
