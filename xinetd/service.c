/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#ifdef HAVE_MDNS
#include "xmdns.h"
#endif
#ifndef NO_RPC
 #ifdef HAVE_RPC_PMAP_CLNT_H
  #ifdef __sun
   #include <rpc/types.h>
   #include <rpc/auth.h>
  #endif
  #include <rpc/types.h>
  #include <rpc/xdr.h>
  #include <rpc/auth.h>
  #include <rpc/clnt.h>
  #include <rpc/pmap_clnt.h>
 #endif
 #include <rpc/rpc.h>
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include "sio.h"
#include "service.h"
#include "util.h"
#include "main.h"
#include "sconf.h"
#include "msg.h"
#include "logctl.h"
#include "xconfig.h"
#include "special.h"


#define NEW_SVC()              NEW( struct service )
#define FREE_SVC( sp )         FREE( sp )

#define DISABLE( sp )          SVC_STATE((sp)) = SVC_DISABLED

static void deactivate( const struct service *sp );
static int banner_always( const struct service *sp, const connection_s *cp );

static const struct name_value service_states[] =
   {
      { "Not started",        (int) SVC_NOT_STARTED    },
      { "Active",             (int) SVC_ACTIVE         },
      { "Disabled",           (int) SVC_DISABLED       },
      { "Suspended",          (int) SVC_SUSPENDED      },
      { NULL,                 1                        },
      { "BAD STATE",          0                        }
   } ;



/*
 * Allocate a new struct service and initialize it from scp 
 */
struct service *svc_new( struct service_config *scp )
{
   struct service *sp ;
   const char *func = "svc_new" ;

   sp = NEW_SVC() ;
   if ( sp == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   CLEAR( *sp ) ;

   SVC_CONF(sp) = scp ;
   return( sp ) ;
}


struct service *svc_make_special( struct service_config *scp )
{
   struct service      *sp ;
   const char          *func = "svc_make_special" ;

   if ( ( sp = svc_new( scp ) ) == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }

   SVC_NOT_GENERIC(sp) = 1 ;
   SVC_LOG(sp) = ps.rws.program_log ;
   SVC_REFCOUNT(sp) = 1 ;
   SVC_STATE(sp) = SVC_ACTIVE ;
   return( sp ) ;
}


void svc_free( struct service *sp )
{
   sc_free( SVC_CONF(sp) ) ;
   CLEAR( *sp ) ;
   FREE_SVC( sp ) ;
}


static status_e set_fd_modes( struct service *sp )
{
   int sd = SVC_FD( sp ) ;
   const char *func = "set_fd_modes" ;

   /*
    * There is a possibility of blocking on a send/write if
    *
    * the service does not require forking (==> is internal) AND
    * it does not accept connections
    *
    * To avoid this, we put the descriptor in FNDELAY mode.
    * (if the service accepts connections, we still need to put the
    * 'accepted' connection in FNDELAY mode but this is done elsewhere)
    */
   if ( ! SVC_FORKS( sp ) && ! SVC_ACCEPTS_CONNECTIONS( sp ) &&
                              fcntl( sd, F_SETFL, FNDELAY ) == -1 )
   {
      msg( LOG_ERR, func,
         "fcntl failed (%m) for FNDELAY. service = %s", SVC_ID( sp ) ) ;
      return( FAILED ) ;
   }

   /*
    * Always set the close-on-exec flag
    */
   if ( fcntl( sd, F_SETFD, FD_CLOEXEC ) == -1 )
   {
      msg( LOG_ERR, func,
         "fcntl failed (%m) for close-on-exec. service = %s", SVC_ID( sp ) ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}


#ifndef NO_RPC

static status_e activate_rpc( struct service *sp )
{
   union xsockaddr        tsin;
   socklen_t              sin_len = sizeof(tsin);
   unsigned long          vers ;
   struct service_config *scp = SVC_CONF( sp ) ;
   struct rpc_data       *rdp = SC_RPCDATA( scp ) ;
   char                  *sid = SC_ID( scp ) ;
   unsigned               registered_versions = 0 ;
   int                    sd = SVC_FD( sp ) ;
   const char            *func = "activate_rpc" ;

   if( SC_BIND_ADDR(scp) != 0 )
      memcpy( &tsin, SC_BIND_ADDR(scp), sizeof(tsin) );
   else
      memset( &tsin, 0, sizeof(tsin));

   if ( SC_PROTOVAL ( scp ) == IPPROTO_TCP ) {
      M_SET ( scp->sc_xflags, SF_NOLIBWRAP );
   }
   if( SC_IPV4( scp ) ) {
      tsin.sa_in.sin_family = AF_INET ;
      sin_len = sizeof(struct sockaddr_in);
   } else if( SC_IPV6( scp ) ) {
      tsin.sa_in6.sin6_family = AF_INET6 ;
      sin_len = sizeof(struct sockaddr_in6);
   }

   if ( bind( sd, &tsin.sa, sin_len ) == -1 )
   {
      msg( LOG_ERR, func, "bind failed (%m). service = %s", sid ) ;
      return( FAILED ) ;
   }

   /*
    * Find the port number that was assigned to the socket
    */
   if ( getsockname( sd, &tsin.sa, &sin_len ) == -1 )
   {
      msg( LOG_ERR, func,
            "getsockname failed (%m). service = %s", sid ) ;
      return( FAILED ) ;
   }
   
   if( tsin.sa.sa_family == AF_INET )
      SC_SET_PORT( scp, ntohs( tsin.sa_in.sin_port ) ) ;
   else if( tsin.sa.sa_family == AF_INET6 )
      SC_SET_PORT( scp, ntohs( tsin.sa_in6.sin6_port ) ) ;

   /*
    * Try to register as many versions as possible
    */
   for ( vers = RD_MINVERS( rdp ) ; vers <= RD_MAXVERS( rdp ) ; vers++ ) {
/*      Is this right?  For instance, if we have both tcp and udp services,
 *      this will unregister the previously registered protocol.
 *      pmap_unset(RD_PROGNUM(rdp), vers);
 */
      if ( pmap_set( RD_PROGNUM( rdp ), vers, SC_PROTOVAL( scp ), 
			      SC_PORT( scp ) ) )
         registered_versions++ ;
      else
         msg( LOG_ERR, func,
            "pmap_set failed. service=%s program=%ld version=%ld",
               sid, RD_PROGNUM( rdp ), vers ) ;
      sleep(1);
   }

   if ( debug.on )
      msg( LOG_DEBUG, func,
            "Registered %d versions of %s", registered_versions, sid ) ;

   return( ( registered_versions == 0 ) ? FAILED : OK ) ;
}

#endif   /* ! NO_RPC */

static status_e activate_normal( struct service *sp )
{
   union xsockaddr         tsin;
   int                     sd             = SVC_FD( sp ) ;
   struct service_config  *scp            = SVC_CONF( sp ) ;
   uint16_t                service_port   = SC_PORT( scp ) ;
   char                   *sid            = SC_ID( scp ) ;
   const char             *func           = "activate_normal" ;
   unsigned int            sin_len        = sizeof(tsin);
   int                     on             = 1;
#ifdef IPV6_V6ONLY
   int                     v6on           = 0;
#endif

   if( SC_BIND_ADDR(scp) != NULL )
      memcpy(&tsin, SC_BIND_ADDR(scp), sin_len);
   else
      memset(&tsin, 0, sin_len);
   
   if( SC_IPV4( scp ) ) {
      tsin.sa_in.sin_family = AF_INET ;
      tsin.sa_in.sin_port = htons( service_port ) ;
      sin_len = sizeof(struct sockaddr_in);
   } else if( SC_IPV6( scp ) ) {
      tsin.sa_in6.sin6_family = AF_INET6;
      tsin.sa_in6.sin6_port = htons( service_port );
      sin_len = sizeof(struct sockaddr_in6);
   }

#ifdef IPV6_V6ONLY
   if( SC_IPV6(scp) ) {
      if( SC_SPECIFIED(scp, A_V6ONLY) ) {
         v6on = 1;
      } else {
         v6on = 0;
      }
      if( setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&v6on, sizeof(v6on)) < 0 ) {
         msg( LOG_ERR, func, "Setting IPV6_V6ONLY option failed (%m)" );
      }
   }
#endif

   if ( setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, 
                    (char *) &on, sizeof( on ) ) == -1 )
      msg( LOG_WARNING, func, 
           "setsockopt SO_REUSEADDR failed (%m). service = %s", sid ) ;

   if( SC_NODELAY( scp ) && (SC_PROTOVAL(scp) == IPPROTO_TCP) )
   {
      if ( setsockopt( sd, IPPROTO_TCP, TCP_NODELAY, 
                       (char *) &on, sizeof( on ) ) == -1 )
         msg( LOG_WARNING, func, 
              "setsockopt TCP_NODELAY failed (%m). service = %s", sid ) ;
   }

   if( SC_KEEPALIVE( scp ) && (SC_PROTOVAL(scp) == IPPROTO_TCP) ) 
   {
      if( setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, 
                     (char *)&on, sizeof( on ) ) < 0 )
         msg( LOG_WARNING, func, 
              "setsockopt SO_KEEPALIVE failed (%m). service = %s", sid ) ;
   }

   if ( bind( sd, &tsin.sa, sin_len ) == -1 )
   {
      msg( LOG_ERR, func, "bind failed (%m). service = %s", sid ) ;
      return( FAILED ) ;
   }

#ifdef IN_MULTICAST
   if( SC_IPV4(scp) && IN_MULTICAST(tsin.sa_in.sin_addr.s_addr) ) {
      struct ip_mreq mreq;
      mreq.imr_multiaddr.s_addr = tsin.sa_in.sin_addr.s_addr;
      mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
      if ( debug.on )
         msg( LOG_DEBUG, func, "Adding multicast membership." );
   }
#endif

   return( OK ) ;
}


/*
 * Activate a service. 
 */
status_e svc_activate( struct service *sp )
{
   struct service_config    *scp = SVC_CONF( sp ) ;
   status_e                  status ;
   const char                     *func = "svc_activate" ;

   /*  No activation for MUXCLIENTS.
    */

   if (SC_IS_MUXCLIENT( scp ))
   {
      return( OK );
   }

   if( SC_IPV4( scp ) ) {
      SVC_FD(sp) = socket( AF_INET, 
                           SC_SOCKET_TYPE( scp ), SC_PROTOVAL( scp ) ) ;
   } else if( SC_IPV6( scp ) ) {
      SVC_FD(sp) = socket( AF_INET6, 
                           SC_SOCKET_TYPE( scp ), SC_PROTOVAL( scp ) ) ;
   }

   if ( SVC_FD(sp) == -1 )
   {
      msg( LOG_ERR, func,
                  "socket creation failed (%m). service = %s", SC_ID( scp ) ) ;
      return( FAILED ) ;
   }

   if ( set_fd_modes( sp ) == FAILED )
   {
      (void) Sclose( SVC_FD(sp) ) ;
      return( FAILED ) ;
   }

#ifndef NO_RPC
   if ( SC_IS_RPC( scp ) )
      status = activate_rpc( sp ) ;
   else
#endif   /* ! NO_RPC */
      status = activate_normal( sp ) ;
   
   if ( status == FAILED )
   {
      (void) Sclose( SVC_FD(sp) ) ;
      return( FAILED ) ;
   }

#ifdef HAVE_MDNS
   xinetd_mdns_register(scp);
#endif

   if ( log_start( sp, &SVC_LOG(sp) ) == FAILED )
   {
      deactivate( sp ) ;
      return( FAILED ) ;
   }

   /*
    * Initialize the service data
    */
   SVC_RUNNING_SERVERS(sp)   = SVC_RETRIES(sp) = 0 ;

   if ( SC_MUST_LISTEN( scp ) )
      (void) listen( SVC_FD(sp), LISTEN_BACKLOG ) ;

   ps.rws.descriptors_free-- ;

   SVC_STATE(sp) = SVC_ACTIVE ;

   FD_SET( SVC_FD(sp), &ps.rws.socket_mask ) ;
   if ( SVC_FD(sp) > ps.rws.mask_max )
      ps.rws.mask_max = SVC_FD(sp) ;

   ps.rws.active_services++ ;
   ps.rws.available_services++ ;

   return( OK ) ;
}


static void deactivate( const struct service *sp )
{
   (void) Sclose( SVC_FD( sp ) ) ;

#ifdef HAVE_MDNS
   xinetd_mdns_deregister(SVC_CONF(sp));
#endif

   if (debug.on)
      msg(LOG_DEBUG, "deactivate", "%d Service %s deactivated", 
          getpid(), SC_NAME( SVC_CONF(sp) ) );

#ifndef NO_RPC
   if ( SC_IS_RPC( SVC_CONF( sp ) ) )
   {
      unsigned long vers ;
      const struct rpc_data *rdp = SC_RPCDATA( SVC_CONF( sp ) ) ;

      for ( vers = RD_MINVERS( rdp ) ; vers <= RD_MAXVERS( rdp ) ; vers++ ) {
         (void) pmap_unset( RD_PROGNUM( rdp ), vers ) ;
      }
   }
#endif   /* ! NO_RPC */
}


/*
 * Close the service descriptor.
 * If this is an RPC service, deregister it.
 * Close the log.
 */
void svc_deactivate( struct service *sp )
{
   if ( ! SVC_IS_AVAILABLE( sp ) )
      return ;

   deactivate( sp ) ;
   ps.rws.descriptors_free++ ;

   if ( SVC_IS_ACTIVE( sp ) )
   {
      FD_CLR( SVC_FD( sp ), &ps.rws.socket_mask ) ;
      ps.rws.active_services-- ;
   }

   ps.rws.available_services-- ;

   DISABLE( sp ) ;
}


/*
 * Suspend a service
 */
void svc_suspend( struct service *sp )
{
   const char *func = "svc_suspend" ;

   if ( ! SVC_IS_ACTIVE( sp ) )
   {
      msg( LOG_ERR, func, "service %s is not active", SVC_ID( sp ) ) ;
      return ;
   }

   FD_CLR( SVC_FD( sp ), &ps.rws.socket_mask ) ;
   ps.rws.active_services-- ;
   if ( debug.on )
      msg( LOG_DEBUG, func, "Suspended service %s", SVC_ID( sp ) ) ;
   
   SUSPEND( sp ) ;
}


/*
 * Resume a suspended service.
 */
void svc_resume( struct service *sp )
{
   const char *func = "svc_resume" ;

   FD_SET( SVC_FD( sp ), &ps.rws.socket_mask ) ;
   ps.rws.active_services++ ;
   if ( debug.on )
      msg( LOG_DEBUG, func, "Resumed service %s", SVC_ID( sp ) ) ;
   RESUME( sp ) ;
}


/*
 * Steps:
 *      1. Deactivate the service
 *      2. Free all memory used by the service and free the service itself
 *
 * Since this function may free all memory associated with the service as
 * well as the memory pointed by sp, only the value of sp should be used
 * after this call if the return value is 0 (i.e. no dereferencing of sp).
 *
 * Special services are never deactivated.
 */
int svc_release( struct service *sp )
{
   char *sid = SVC_ID( sp ) ;
   const char *func = "svc_release" ;

   if ( SVC_REFCOUNT(sp) == 0 )
   {
      msg( LOG_ERR, func, "%s: svc_release with 0 count", sid ) ;
      return( 0 ) ;
   }
   
   SVC_REFCOUNT(sp)-- ;
   if ( SVC_REFCOUNT(sp) == 0 )
   {
      if ( debug.on )
         msg( LOG_DEBUG, func, "ref count of service %s dropped to 0", sid ) ;
      if ( ! SC_IS_SPECIAL( SVC_CONF( sp ) ) )
      {
         if ( SVC_LOG(sp) )
            log_end( SC_LOG( SVC_CONF( sp ) ), SVC_LOG(sp) ) ;
         svc_deactivate( sp ) ;
         svc_free( sp ) ;
         sp = NULL;
      }
      else      /* this shouldn't happen */
         msg( LOG_WARNING, func,
            "ref count of special service %s dropped to 0", sid ) ;
      return( 0 ) ;
   }
   else
      return( SVC_REFCOUNT(sp) ) ;
}


void svc_dump( const struct service *sp, int fd )
{
   tabprint( fd, 0, "Service = %s\n", SC_NAME( SVC_CONF( sp ) ) ) ;
   tabprint( fd, 1, "State = %s\n",
                        nv_get_name( service_states, (int) SVC_STATE(sp) ) ) ;

   sc_dump( SVC_CONF( sp ), fd, 1, FALSE ) ;

   if ( SVC_IS_ACTIVE(sp) )
   {
      tabprint( fd, 1, "running servers = %d\n", SVC_RUNNING_SERVERS(sp) ) ;
      tabprint( fd, 1, "retry servers = %d\n", SVC_RETRIES(sp) ) ;
      tabprint( fd, 1, "attempts = %d\n", SVC_ATTEMPTS(sp) ) ;
      tabprint( fd, 1, "service fd = %d\n", SVC_FD(sp) ) ;
   }
   Sputchar( fd, '\n' ) ;
}


void svc_request( struct service *sp )
{
   connection_s *cp ;
   status_e ret_code;

   cp = conn_new( sp ) ;
   if ( cp == CONN_NULL )
      return ;

   /*
    * Output the banner now that the connection is established. The
    * other banners come later.
    */
   banner_always(sp, cp);

   if (SVC_NOT_GENERIC(sp))
      ret_code = spec_service_handler(sp, cp);
   else 
      ret_code = svc_generic_handler(sp, cp);

   if( (SVC_SOCKET_TYPE( sp ) == SOCK_DGRAM) && (SVC_IS_ACTIVE( sp )) ) 
      drain( cp->co_descriptor ) ; /* Prevents looping next time */
   
   if ( ret_code != OK ) 
   {
      if ( SVC_LOGS_USERID_ON_FAILURE( sp ) ) {
         if( spec_service_handler( LOG_SERVICE( ps ), cp ) == FAILED ) 
	    conn_free( cp, 1 ) ;
         else if (!SC_WAITS( SVC_CONF( sp ) ) ) {
	 /* The logging service will gen SIGCHLD thus freeing connection */
	    CONN_CLOSE(cp) ; 
	 }
	 return;
      }
      if (!SC_WAITS( SVC_CONF( sp ) )) 
	 conn_free( cp, 1 );
      else { 
         if( (SVC_SOCKET_TYPE( sp ) == SOCK_DGRAM) && (SVC_IS_ACTIVE( sp )) ) 
            drain( cp->co_descriptor ) ; /* Prevents looping next time */
	 free( cp );
      }
   }
   else if ((SVC_NOT_GENERIC(sp)) || (!SC_FORKS( SVC_CONF( sp ) ) ) )
     free( cp );
}


status_e svc_generic_handler( struct service *sp, connection_s *cp )
{
   if ( svc_parent_access_control( sp, cp ) == OK ) {
      return( server_run( sp, cp ) ) ;
   }

   return( FAILED ) ;
}

#define TMPSIZE 1024
/* Print the banner that is supposed to always be printed */
static int banner_always( const struct service *sp, const connection_s *cp )
{
   const char *func = "banner_always";
   const struct service_config *scp = SVC_CONF( sp ) ;

   /* print the banner regardless of access control */
   if ( SC_BANNER(scp) != NULL ) {
      char tmpbuf[TMPSIZE];
      ssize_t retval;
      int bannerfd = open(SC_BANNER(scp), O_RDONLY);

      if( bannerfd < 0 ) {
         msg( LOG_ERR, func, "service = %s, open of banner %s failed", 
			 SVC_ID( sp ), SC_BANNER(scp));
         return(-1);
      }

      while( (retval = read(bannerfd, tmpbuf, sizeof(tmpbuf))) ) {
         if (retval == (ssize_t)-1)
         {
            if (errno == EINTR)
               continue;
            else
            {
               msg(LOG_ERR, func, "service %s, Error %m reading banner %s", 
			       SVC_ID( sp ), SC_BANNER(scp));
               break;
            }
         }
         Swrite(cp->co_descriptor, tmpbuf, retval);
      }

      Sclose(bannerfd);
      Sflush ( cp->co_descriptor );
   }

   return(0);
}

static int banner_fail( const struct service *sp, const connection_s *cp )
{
   const char *func = "banner_fail";
   const struct service_config *scp = SVC_CONF( sp ) ;


   if ( SC_BANNER_FAIL(scp) != NULL )
   {
      char tmpbuf[TMPSIZE];
      int retval;
      int bannerfd = open(SC_BANNER_FAIL(scp), O_RDONLY);

      if( bannerfd < 0 )
      {
         msg( LOG_ERR, func, "service = %s, open of banner %s failed", 
            SVC_ID( sp ), SC_BANNER_FAIL(scp));
         return(-1);
      }

      while( (retval = read(bannerfd, tmpbuf, sizeof(tmpbuf))) ) {
         if (retval == -1)
         {
            if (errno == EINTR)
               continue;
            else
            {
               msg(LOG_ERR, func, "service %s, Error %m reading banner %s", 
			       SVC_ID( sp ), SC_BANNER(scp));
               break;
            }
         }
         Swrite(cp->co_descriptor, tmpbuf, retval);
      }

      Sclose(bannerfd);
      Sflush ( cp->co_descriptor );
   }

   return(0);
}

static int banner_success( const struct service *sp, const connection_s *cp )
{
   const char *func = "banner_success";
   const struct service_config *scp = SVC_CONF( sp ) ;

   /* print the access granted banner */
   if ( SC_BANNER_SUCCESS(scp) != NULL ) {
      char tmpbuf[TMPSIZE];
      int retval;
      int bannerfd = open(SC_BANNER_SUCCESS(scp), O_RDONLY);

      if( bannerfd < 0 ) {
         msg( LOG_ERR, func, "service = %s, open of banner %s failed", 
			 SVC_ID( sp ), SC_BANNER_SUCCESS(scp));
         return(-1);
      }

      while( (retval = read(bannerfd, tmpbuf, sizeof(tmpbuf))) ) {
         if (retval == -1)
         {
            if (errno == EINTR)
               continue;
            else
            {
               msg(LOG_ERR, func, "service %s, Error %m reading banner %s", 
			       SVC_ID( sp ), SC_BANNER(scp));
               break;
            }
         }
         Swrite(cp->co_descriptor, tmpbuf, retval);
      }

      Sclose(bannerfd);
      Sflush ( cp->co_descriptor );
   }
   return(0);
}

static status_e failed_service(struct service *sp, 
                                connection_s *cp, 
                                access_e result)
{
   struct service_config *scp = SVC_CONF( sp ) ;

   if ( result != AC_OK )
   {
      bool_int report_failure = TRUE ;

      /*
       * Try to avoid reporting multiple times a failed attempt to access
       * a datagram-based service from a bad address. We do this because
       * the clients of such services usually send multiple datagrams 
       * before reporting a timeout (we have no way of telling them that
       * their request has been denied).
       */
      if ( result == AC_ADDRESS && SVC_SOCKET_TYPE( sp ) == SOCK_DGRAM )
      {
         if( SC_IPV4( scp ) ) {
            struct sockaddr_in *sinp = SAIN(CONN_ADDRESS( cp )) ;
            struct sockaddr_in *last = SAIN(SVC_LAST_DGRAM_ADDR(sp)) ;
            time_t current_time ;

            if (sinp == NULL )
               return FAILED;

            if ( last == NULL ) {
               last = SAIN( calloc( 1, sizeof(union xsockaddr) ) );
	       SVC_LAST_DGRAM_ADDR(sp) = (union xsockaddr *)last;
            }

            (void) time( &current_time ) ;
            if ( sinp->sin_addr.s_addr == last->sin_addr.s_addr &&
                                          sinp->sin_port == last->sin_port )
            {
               if( current_time - SVC_LAST_DGRAM_TIME(sp) <= DGRAM_IGNORE_TIME )
                  report_failure = FALSE ;
               else
                  SVC_LAST_DGRAM_TIME(sp) = current_time ;
            }
            else
            {
               memcpy(SVC_LAST_DGRAM_ADDR(sp), sinp,sizeof(struct sockaddr_in));
               SVC_LAST_DGRAM_TIME(sp) = current_time ;
            }
         } else if( SC_IPV6( scp ) ) {
            struct sockaddr_in6 *sinp = SAIN6(CONN_ADDRESS( cp )) ;
            struct sockaddr_in6 *last = SAIN6(SVC_LAST_DGRAM_ADDR(sp)) ;
            time_t current_time ;

	    if (sinp == NULL )
               return FAILED;

	    if( last == NULL ) {
               last = SAIN6(calloc( 1, sizeof(union xsockaddr) ) );
	       SVC_LAST_DGRAM_ADDR( sp ) = (union xsockaddr *)last;
            }

            (void) time( &current_time ) ;
            if ( IN6_ARE_ADDR_EQUAL(&(sinp->sin6_addr), &(last->sin6_addr)) && 
                 sinp->sin6_port == last->sin6_port )
            {
               if((current_time - SVC_LAST_DGRAM_TIME(sp)) <= DGRAM_IGNORE_TIME)
                  report_failure = FALSE ;
               else
                  SVC_LAST_DGRAM_TIME(sp) = current_time ;
            }
            else
            {
               memcpy(SVC_LAST_DGRAM_ADDR(sp),sinp,sizeof(struct sockaddr_in6));
               SVC_LAST_DGRAM_TIME(sp) = current_time ;
            }
         }
      }

      if ( report_failure )
         svc_log_failure( sp, cp, result ) ;

      banner_fail(sp, cp);

      return( FAILED ) ;
   }

   return( OK );
}

/* Do the "light weight" access control here */
status_e svc_parent_access_control( struct service *sp, connection_s *cp )
{
   access_e result;

   result = parent_access_control( sp, cp );
   if( failed_service(sp, cp, result) == FAILED )
      return(FAILED);

   return (OK);
}

status_e svc_child_access_control( struct service *sp, connection_s *cp )
{
   access_e result ;

   result = access_control( sp, cp, MASK_NULL ) ;
   if( failed_service(sp, cp, result) == FAILED )
      return(FAILED);

   banner_success(sp, cp);

   return( OK ) ;
}

/*
 * Invoked when a server of the specified service dies
 */
void svc_postmortem( struct service *sp, struct server *serp )
{
   struct service  *co_sp   = SERVER_CONNSERVICE( serp ) ;
   connection_s    *cp      = SERVER_CONNECTION( serp ) ;
   const char      *func    = "svc_postmortem" ;

   SVC_DEC_RUNNING_SERVERS( sp ) ;

   /*
    * Log information about the server that died
    */
   if ( SVC_IS_LOGGING( sp ) )
   {
      if ( SERVER_WRITES_TO_LOG(serp) )
      {
         if ( debug.on )
            msg( LOG_DEBUG, func,
                        "Checking log size of %s service", SVC_ID( sp ) ) ;
         xlog_control( SVC_LOG( sp ), XLOG_SIZECHECK ) ;
      }
      svc_log_exit( sp, serp ) ;
   }

   /*
    * Now check if we have to check the log size of the service that owns
    * the connection
    */
   if ( co_sp != sp && SVC_IS_LOGGING( co_sp ) )
      xlog_control( SVC_LOG( co_sp ), XLOG_SIZECHECK ) ;

   if (!SVC_WAITS(sp)) {
      conn_free( cp, 1 ) ;
      cp = NULL;
   } else {
      if (cp) {
         if ( SVC_SOCKET_TYPE( sp ) == SOCK_DGRAM )
            drain( cp->co_descriptor ) ;
         free(cp);
         cp = NULL;
         if( SVC_RELE( sp ) == 0 )
            svc_release( sp ); /* shouldn't be 0, but should remove from
                                * pset if it is... */
      }
      svc_resume(sp);
   }
}

/*
 * This function closes all service descriptors. This should be called 
 * for all child processes that fork, but do not exec. This includes
 * redirect, builtins, and tcpmux. The close on exec flag takes care of
 * child processes that call exec. Without calling this, the listening 
 * fd's are not closed and reconfig will fail.
 */
void close_all_svc_descriptors(void)
{
   psi_h                     iter ;
   struct service            *osp ;

   /* Have to close all other descriptors here */
   iter = psi_create( SERVICES( ps ) ) ;
   if ( iter == NULL )
        out_of_memory( "close_all_svc_descriptors" ) ;

   for ( osp = SP( psi_start( iter ) ) ; osp ; osp = SP( psi_next( iter ) ) )
        (void) Sclose( SVC_FD( osp ) ) ;
  
   psi_destroy( iter ) ;
}

