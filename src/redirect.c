/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/signal.h>

#include "redirect.h"
#include "service.h"
#include "log.h"
#include "sconf.h"
#include "msg.h"

#define NET_BUFFER 1500

static int RedirServerFd = -1;

/* Theoretically, this gets invoked when the remote side is no
 * longer available for reading or writing.
 * So, we send a HUP to the child process, wait(), then exit.
 */
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
static void redir_sigpipe( int signum ) 
{
   Sclose(RedirServerFd);
   _exit(0);
}

/* Do the redirection of a service */
/* This function gets called from child.c after we have been forked */
void redir_handler( struct server *serp )
{
   struct service *sp = SERVER_SERVICE( serp );
   struct service_config *scp = SVC_CONF( sp );
   int RedirDescrip = SERVER_FD( serp );
   ssize_t num_read, num_wrote=0, ret=0;
   unsigned int sin_len = 0;
   unsigned long bytes_in = 0, bytes_out = 0;
   int no_to_nagle = 1;
   int on = 1, v6on;
   char buff[NET_BUFFER];
#ifdef HAVE_POLL
   struct pollfd *pfd_array;
   int            pfds_last = 0;
#else
   fd_set rdfd, msfd;
   int maxfd;
   struct timeval *timep = NULL;
#endif
   const char *func = "redir_handler";
   union xsockaddr serveraddr ;

   if( signal(SIGPIPE, redir_sigpipe) == SIG_ERR ) 
      msg(LOG_ERR, func, "unable to setup signal handler");

   close_all_svc_descriptors();

   /* If it's a tcp service we are redirecting */
   if( SC_PROTOVAL(scp) == IPPROTO_TCP )
   {
      memcpy(&serveraddr, SC_REDIR_ADDR(scp), sizeof(serveraddr));
      if( serveraddr.sa_in.sin_family == AF_INET ) {
         sin_len = sizeof( struct sockaddr_in );
         RedirServerFd = socket(AF_INET, SOCK_STREAM, 0);
       } else if( serveraddr.sa_in.sin_family == AF_INET6 ) {
         sin_len = sizeof( struct sockaddr_in6 );
         RedirServerFd = socket(AF_INET6, SOCK_STREAM, 0);
      } else {
         msg(LOG_ERR, func, "not a valid protocol. Use IPv4 or IPv6.");
         exit(0);
      }

      if( RedirServerFd < 0 )
      {
         msg(LOG_ERR, func, "cannot create socket: %m");
         exit(0);
      }

      if( SC_IPV6( scp ) ) {
         if( SC_V6ONLY( scp ) ) {
            v6on = 1;
         } else {
            v6on = 0;
         }
#ifdef IPV6_V6ONLY
         if( setsockopt(RedirServerFd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6on, sizeof(v6on)) < 0 ) { 
            msg( LOG_ERR, func, "Setting IPV6_V6ONLY option failed (%m)" );
         }
#endif

      }
      if( SC_KEEPALIVE( scp ) )
         if (setsockopt(RedirServerFd, SOL_SOCKET, SO_KEEPALIVE, 
                        (char *)&on, sizeof( on ) ) < 0 )
            msg(LOG_ERR, func, 
                "setsockopt SO_KEEPALIVE RedirServerFd failed: %m");
      
      if( serveraddr.sa_in.sin_family == AF_INET )
         serveraddr.sa_in.sin_port = htons(serveraddr.sa_in.sin_port);
      if( serveraddr.sa_in.sin_family == AF_INET6 )
         serveraddr.sa_in6.sin6_port = htons(serveraddr.sa_in6.sin6_port);

      if( connect(RedirServerFd, &serveraddr.sa, sin_len) < 0 )
      {
         msg(LOG_ERR, func, "can't connect to remote host %s: %m",
            xaddrname( &serveraddr ) );
         exit(0);
      }

      /* connection now established */

      if (setsockopt(RedirServerFd, IPPROTO_TCP, TCP_NODELAY, 
         (char *) &no_to_nagle, sizeof( on ) ) < 0) {

         msg(LOG_ERR, func, "setsockopt RedirServerFd failed: %m");
      }

      if (setsockopt(RedirDescrip, IPPROTO_TCP, TCP_NODELAY, 
         (char *) &no_to_nagle, sizeof( on ) ) < 0) {

         msg(LOG_ERR, func, "setsockopt RedirDescrip failed: %m");
      }

#ifdef HAVE_POLL
#define REDIR_DESCRIP_INDEX 0
#define REDIR_SERVER_INDEX 1
      pfd_array = (struct pollfd *)calloc(sizeof(struct pollfd),MAX_FDS);
      if (pfd_array == NULL)
      {
         msg( LOG_ERR, func, "Cannot allocate memory for file descriptors!\n");
         exit( 1 );
      }
      pfd_array[ REDIR_DESCRIP_INDEX ].fd = RedirDescrip;
      pfd_array[ REDIR_DESCRIP_INDEX ].events = POLLIN;
      pfd_array[ REDIR_SERVER_INDEX ].fd = RedirServerFd;
      pfd_array[ REDIR_SERVER_INDEX ].events = POLLIN;
      pfds_last += 2;
#else
      maxfd = (RedirServerFd > RedirDescrip)?RedirServerFd:RedirDescrip;
      FD_ZERO(&msfd);
      FD_SET(RedirDescrip, &msfd);
      FD_SET(RedirServerFd, &msfd);
#endif

      while(1) {
#ifdef HAVE_POLL
         if ( poll( pfd_array, pfds_last, -1 ) <= 0 ) {
#else
         memcpy(&rdfd, &msfd, sizeof(rdfd));
         if (select(maxfd + 1, &rdfd, (fd_set *)0, (fd_set *)0, timep) <= 0) {
#endif
            /* place for timeout code, currently does not time out */
            break;
         }

#ifdef HAVE_POLL
         if ( pfd_array[REDIR_DESCRIP_INDEX].revents ) {
#else
         if (FD_ISSET(RedirDescrip, &rdfd)) {
#endif
            do {
               num_read = read(RedirDescrip,
                  buff, sizeof(buff));
               if (num_read == (ssize_t)-1 && errno == EINTR)
                  continue;
               if (num_read <= 0)
                  goto REDIROUT;
               bytes_in += num_read;
            } while (num_read < 0);

            /* Loop until we have written everything
             * that was read */
            num_wrote = 0;
            while( num_wrote < num_read ) {
               ret = write(RedirServerFd,
                  buff + num_wrote,
                  num_read - num_wrote);
               if (ret == -1 && errno == EINTR)
                  continue;
               if (ret <= 0)
                  goto REDIROUT;
               num_wrote += ret;
            }
         }

#ifdef HAVE_POLL
         if ( pfd_array[REDIR_SERVER_INDEX].revents ) {
#else
         if (FD_ISSET(RedirServerFd, &rdfd)) {
#endif
            do {
               num_read = read(RedirServerFd,
                  buff, sizeof(buff));
               if (num_read == -1 && errno == EINTR)
                  continue;
               if (num_read <= 0)
                  goto REDIROUT;
               bytes_out += num_read;
            } while (num_read < 0);

            /* Loop until we have written everything
             * that was read */
            num_wrote = 0;
            while( num_wrote < num_read ) {
               ret = write(RedirDescrip,
                  buff + num_wrote,
                  num_read - num_wrote);
               if (ret == -1 && errno == EINTR)
                  continue;
               if (ret <= 0)
                  goto REDIROUT;
               num_wrote += ret;
            }
         }
      }
REDIROUT:
      if( M_IS_SET( SC_LOG_ON_SUCCESS(scp), LO_TRAFFIC ) ) {
         svc_logprint( SERVER_CONNSERVICE( serp ), "TRAFFIC",
                       "in=%lu(bytes) out=%lu(bytes)", bytes_in, bytes_out );
      }

      exit(0);
   }

   msg(LOG_ERR, func, 
   "redirect with any protocol other than tcp is not supported at this time.");
   exit(0);
}
