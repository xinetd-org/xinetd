#ifndef _FAKE_GETADDRINFO_H
#define _FAKE_GETADDRINFO_H

#include "config.h"

/*
#include "fake-gai-errnos.h"
*/

#ifndef AI_PASSIVE
# define AI_PASSIVE        1
# define AI_CANONNAME      2
#endif

#ifndef NI_NUMERICHOST
# define NI_NUMERICHOST    2
# define NI_NAMEREQD       4
# define NI_NUMERICSERV    8
#endif

#ifndef EAI_NODATA
#define EAI_NODATA        -5
#endif

#ifndef EAI_MEMORY
#define EAI_MEMORY       -10
#endif

#ifndef HAVE_GETADDRINFO
int getaddrinfo(const char *hostname, const char *servname, 
                const struct addrinfo *hints, struct addrinfo **res);
#endif /* !HAVE_GETADDRINFO */

#ifndef HAVE_GAI_STRERROR
char *gai_strerror(int ecode);
#endif /* !HAVE_GAI_STRERROR */

#ifndef HAVE_FREEADDRINFO
void freeaddrinfo(struct addrinfo *ai);
#endif /* !HAVE_FREEADDRINFO */

#endif /* _FAKE_GETADDRINFO_H */
