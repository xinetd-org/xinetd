/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "str.h"
#include "access.h"
#ifdef LIBWRAP
#include <tcpd.h>
#include <syslog.h>
int deny_severity = LOG_INFO;
int allow_severity = LOG_INFO;
#endif

#ifdef HAVE_LOADAVG
#include "xgetloadavg.h"
#endif

#include "msg.h"
#include "addr.h"
#include "sconf.h"
#include "log.h"	
#include "main.h"	/* for ps */
#include "sconst.h"
#include "sensor.h"
#include "state.h"
#include "timex.h"
#include "xconfig.h"
#include "xtimer.h"

#ifdef NAME_MAX
   #define NAME_MAX 256
#endif

const struct name_value access_code_names[] =
{
   { "address",                  (int) AC_ADDRESS         },
   { "time",                     (int) AC_TIME            },
   { "fork",                     (int) AC_FORK            },
   { "service_limit",            (int) AC_SERVICE_LIMIT   },
   { "per_source_limit",         (int) AC_PER_SOURCE_LIMIT},
   { "process_limit",            (int) AC_PROCESS_LIMIT   },
   { "libwrap",                  (int) AC_LIBWRAP         },
   { "load",                     (int) AC_LOAD            },
   { "connections per second",   (int) AC_CPS             },
   { CHAR_NULL,                  1                        },
   { "UNKNOWN",                  0                        }
} ;


/* This is called by the flags processor */
static void cps_service_restart(void)
{
   unsigned int i;
   time_t nowtime;
   const char *func = "cps_service_restart";

   nowtime = time(NULL);
   for( i=0; i < pset_count( SERVICES(ps) ); i++ ) {
      struct service *sp;
      struct service_config *scp;

      sp = pset_pointer( SERVICES(ps), i);

      if( SVC_STATE(sp) == SVC_DISABLED ) {
         scp = SVC_CONF( sp );
         if ( SC_TIME_REENABLE(scp) <= nowtime ) {
            /* re-enable the service */
            if( svc_activate(sp) == OK ) {
               msg(LOG_ERR, func,
               "Activating service %s", SC_NAME(scp));
            } else {
               msg(LOG_ERR, func,
               "Error activating service %s", 
               SC_NAME(scp)) ;
            } /* else */
         }
      }
   } /* for */
}


void cps_service_stop(struct service *sp, const char *reason)
{
   struct service_config   *scp = SVC_CONF( sp ) ; 
   time_t nowtime;

   svc_deactivate( sp );
   msg(LOG_ERR, "service_stop", 
	"Deactivating service %s due to %s.  Restarting in %d seconds.", 
	SC_NAME(scp), reason, (int)SC_TIME_WAIT(scp));
   nowtime = time(NULL);
   SC_TIME_REENABLE(scp) = nowtime + SC_TIME_WAIT(scp);
   xtimer_add(cps_service_restart, SC_TIME_WAIT(scp));
}


/*
 * Returns OK if the IP address in sinp is acceptable to the access control
 * lists of the specified service.
 */
static status_e remote_address_check(const struct service *sp, 
		                     const union xsockaddr *sinp)
{
   /*
    * of means only_from, na means no_access
    */
   const char *func = "remote_addr_chk";
   bool_int   of_matched  = FALSE;
   bool_int   na_matched  = FALSE;

   /* The IP address is unknown. Nothing to reject, so accept it. */
   if (sinp == NULL )
      return OK;

   if ( SC_SENSOR( SVC_CONF(sp) ))
   {   /* They hit a sensor...return FAILED since this isn't a real service */
      process_sensor( sp, sinp ) ; 
      return FAILED ;
   } 
   /* They hit a real server...note, this is likely to be a child process. */
   else if ( check_sensor( sinp ) == FAILED )
      return FAILED ;

   /*
    * The addrlist_match function returns an offset+1 to a matching
    * entry in the supplied list. It is not a true/false answer.
    */
   if ( SC_NO_ACCESS( SVC_CONF(sp) ) != NULL )
      na_matched = addrlist_match( SC_NO_ACCESS( SVC_CONF(sp) ), CSA(sinp));

   if ( SC_ONLY_FROM( SVC_CONF(sp) ) != NULL )
      of_matched = addrlist_match( SC_ONLY_FROM( SVC_CONF(sp) ), CSA(sinp));

   /*
    * Check if the specified address is in both lists
    */
   if ( na_matched && of_matched )
   {
      /*
       * If there is a match in both lists, this is an error in the 
       * service entry and we cannot allow a server to start.
       * We do not disable the service entry (not our job).
       */
      msg( LOG_ERR, func,
"Service=%s: only_from list and no_access list match equally the address %s",
            SVC_ID( sp ), 
            xaddrname( sinp ) ) ;
      return FAILED ;
   }

   /* A no_access list was specified and the socket is on it, fail */
   if ( SC_NO_ACCESS( SVC_CONF(sp) ) != NULL && (na_matched != 0) )
      return FAILED ;

   /* A only_from list was specified and the socket wasn't on the list, fail */
   if ( SC_ONLY_FROM( SVC_CONF(sp) ) != NULL && (of_matched == 0) )
      return FAILED ;

   /* If no lists were specified, the default is to allow starting a server */
   return OK ;
}

/*
 * mp is the mask pointer, t is the check type
 */
#define CHECK( mp, t )      ( ( (mp) == NULL ) || M_IS_SET( *(mp), t ) )

/*
 * Perform the access controls specified by check_mask.
 * If check_mask is NULL, perform all access controls
 */
access_e access_control( struct service *sp, 
                         const connection_s *cp, 
                         const mask_t *check_mask )
{
   struct service_config   *scp = SVC_CONF( sp ) ;
#ifdef LIBWRAP
   const char *func = "access_control";
#endif

   /* make sure it's not one of the special pseudo services */
   if( (strncmp(SC_NAME( scp ), INTERCEPT_SERVICE_NAME, sizeof(INTERCEPT_SERVICE_NAME)) == 0) || (strncmp(SC_NAME( scp ), LOG_SERVICE_NAME, sizeof(LOG_SERVICE_NAME)) == 0) ) {
      return (AC_OK);
   }

   /* This has to be before the TCP_WRAPPERS stuff to make sure that
      the sensor gets a chance to see the address */
   if ( CHECK( check_mask, CF_ADDRESS ) &&
         remote_address_check( sp, CONN_XADDRESS( cp ) ) == FAILED )
      return( AC_ADDRESS ) ;

   if( ! SC_NOLIBWRAP( scp ) ) 
   { /* LIBWRAP code block */
#ifdef LIBWRAP
      struct request_info req;
      char *server = NULL;

      /* get the server name to pass to libwrap */
      if( SC_NAMEINARGS( scp ) )
      {
         if ( SC_SERVER_ARGV(scp) )
            server = strrchr( SC_SERVER_ARGV(scp)[0], '/' );
      }
      else {
         if( SC_SERVER(scp) == NULL ) {
            /* probably an internal server, use the service id instead */
            server = SC_ID(scp);
            server--;  /* nasty.  we increment it later... */
         } else {
            server = strrchr( SC_SERVER(scp), '/' );
         }
      }

      /* If this is a redirection or internal , go by the service name,
       * since the server name will be bogus.
       */
      if( (SC_REDIR_ADDR(scp) != NULL) || (SC_IS_INTERNAL(scp) )) {
         server = SC_NAME(scp);
         server--; /* nasty but ok. */
      }

      if( server == NULL )
      {
         if ( SC_SERVER_ARGV(scp))
            server = SC_SERVER_ARGV(scp)[0];
      }
      else
         server++;

      if ( scp->sc_libwrap != NULL )
      {
         server = SC_LIBWRAP(scp);
      }

      if ( server == NULL )
      {
         msg(deny_severity, func, 
            "server param not provided to libwrap refusing connection to %s from %s", 
            SC_ID(scp), conn_addrstr(cp));
         return(AC_LIBWRAP);
      }
      request_init(&req, RQ_DAEMON, server, RQ_FILE, cp->co_descriptor, 0);
      fromhost(&req);
      if (!hosts_access(&req)) {
         msg(deny_severity, func, 
            "libwrap refused connection to %s (libwrap=%s) from %s",
            SC_ID(scp), server, conn_addrstr(cp));
         return(AC_LIBWRAP);
      }
#endif
   } /* LIBWRAP code block */

   return( AC_OK ) ;
}


/* Do the "light weight" access control here */
access_e parent_access_control( struct service *sp, const connection_s *cp )
{
   struct service_config *scp = SVC_CONF( sp ) ;
   int n;
   time_t nowtime;

   /* make sure it's not one of the special pseudo services */
   if( (strncmp(SC_NAME( scp ), INTERCEPT_SERVICE_NAME, sizeof(INTERCEPT_SERVICE_NAME)) == 0) || (strncmp(SC_NAME( scp ), LOG_SERVICE_NAME, sizeof(LOG_SERVICE_NAME)) == 0) ) 
      return (AC_OK);

   /* CPS handler */
   if( SC_TIME_CONN_MAX(scp) != 0 ) {
      int time_diff;
      nowtime = time(NULL);
      time_diff = nowtime - SC_TIME_LIMIT(scp) ;

      if( SC_TIME_CONN(scp) == 0 ) {
         SC_TIME_CONN(scp)++;
         SC_TIME_LIMIT(scp) = nowtime;
      } else if( time_diff < SC_TIME_CONN_MAX(scp) ) {
         SC_TIME_CONN(scp)++;
         if( time_diff == 0 ) time_diff = 1;
         if( SC_TIME_CONN(scp)/time_diff > SC_TIME_CONN_MAX(scp) ) {
            cps_service_stop(sp, "excessive incoming connections");
            return(AC_CPS);
         }
      } else {
         SC_TIME_LIMIT(scp) = nowtime;
         SC_TIME_CONN(scp) = 1;
      }
   }

#ifdef HAVE_LOADAVG
   if ( SC_MAX_LOAD(scp) != 0 ) {
      if ( xgetloadavg() >= SC_MAX_LOAD(scp) ) {
         msg(LOG_ERR, "xinetd", 
            "refused connect from %s due to excessive load", 
            conn_addrstr(cp));
         return( AC_LOAD );
      }
   }
#endif

   if ( SC_ACCESS_TIMES( scp ) != NULL && 
         ! ti_current_time_check( SC_ACCESS_TIMES( scp ) ) )
      return( AC_TIME ) ;

   if ( SC_INSTANCES( scp ) != UNLIMITED &&
        SVC_RUNNING_SERVERS( sp ) >= (unsigned)SC_INSTANCES( scp ) )
      return( AC_SERVICE_LIMIT ) ;

   if( SC_PER_SOURCE(scp) != UNLIMITED ) {
      if ( CONN_XADDRESS(cp) != NULL ) {
         unsigned int u ; 
         n = 0 ;
         for ( u = 0 ; u < pset_count( SERVERS( ps ) ) ; u++ ) {
            struct server *serp = NULL;
            connection_s *cop = NULL;
            serp = SERP( pset_pointer( SERVERS( ps ), u ) ) ;
            if ( (SERVER_SERVICE( serp ) == sp) &&
               ( cop = SERVER_CONNECTION( serp ) ) ) {

               if ( SC_IPV6( scp ) && 
		 IN6_ARE_ADDR_EQUAL( &(cop->co_remote_address.sa_in6.sin6_addr),
		   &((CONN_XADDRESS(cp))->sa_in6.sin6_addr)) )
                  n++;
               if ( SC_IPV4( scp ) && 
                     (cop->co_remote_address.sa_in.sin_addr.s_addr == 
		     (CONN_XADDRESS(cp))->sa_in.sin_addr.s_addr) )
                  n++;
            }
         }

         if ( n >= SC_PER_SOURCE(scp) )
            return( AC_PER_SOURCE_LIMIT ) ;
      }
   }

   if ( ps.ros.process_limit ) {
      unsigned processes_to_create = SC_IS_INTERCEPTED( scp ) ? 2 : 1 ;

      if ( pset_count( SERVERS( ps ) ) + processes_to_create > 
         ps.ros.process_limit ) {
         return( AC_PROCESS_LIMIT ) ;
      }
   }

   return (AC_OK);
}

