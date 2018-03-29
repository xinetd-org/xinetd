/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#ifndef NO_RPC
  #ifdef __sun
   #include <rpc/types.h>
   #include <rpc/auth.h>
  #endif
  #include <rpc/types.h>
  #include <rpc/xdr.h>
  #include <rpc/auth.h>
  #include <rpc/clnt.h>
  #include <rpc/pmap_clnt.h>
 #include <rpc/rpc.h>
#endif

#include "reconfig.h"
#include "msg.h"
#include "sconf.h"
#include "conf.h"
#include "confparse.h"
#include "state.h"
#include "main.h"
#include "retry.h"
#include "logctl.h"
#include "options.h"


static status_e readjust(struct service *sp, 
		struct service_config **new_conf_ptr) ;
static void swap_defaults(struct configuration *new_conf) ;
static void close_default_log(struct configuration *confp, xlog_h def_log);

#define SWAP( x, y, temp )         (temp) = (x), (x) = (y), (y) = (temp)


/*
 * Reconfigure the server by rereading the configuration file.
 * Services may be added, deleted or have their attributes changed.
 * All syslog output uses the LOG_NOTICE priority level (except for
 * errors).
 */
void hard_reconfig( void )
{
   struct service            *osp ;
   struct service_config     *nscp ;
   struct configuration      new_conf ;
   psi_h                     iter ;
   unsigned                  new_services ;
   unsigned                  old_services      = 0 ;
   unsigned                  dropped_services   = 0 ;
   xlog_h		     def_log = DEFAULT_LOG( ps );
   const char               *func               = "hard_reconfig" ;


   msg( LOG_NOTICE, func, "Starting reconfiguration" ) ;

   if ( cnf_get( &new_conf ) == FAILED )
   {
      msg( LOG_WARNING, func, "reconfiguration failed" ) ;
      return ;
   }

   iter = psi_create( SERVICES( ps ) ) ;
   if ( iter == NULL )
   {
      out_of_memory( func ) ;
      cnf_free( &new_conf ) ;
      return ;
   }

   /* After this call, new_conf's defaults point to the old one's defaults */
   msg( LOG_NOTICE, func, "Swapping defaults" ) ;
   swap_defaults( &new_conf ) ;

   /*
    * Glossary:
    *      Sconf: service configuration
    *      Lconf: list of service configurations
    *
    * Iterate over all existing services. If the service is included in the 
    * new Lconf, readjust its attributes (as a side-effect, the new service 
    * Sconf is removed from the new Lconf).
    *   Services not in the new Lconf are deactivated.
    */
   for ( osp = SP( psi_start( iter ) ) ; osp ; osp = SP( psi_next( iter ) ) )
   {
      char *sid = SVC_ID( osp ) ;
      boolean_e drop_service ;

      /*
       * Check if this service is in the new Lconf
       * Notice that the service Sconf is removed from the new Lconf
       * if it is found there.
       */
      if (  (nscp = cnf_extract( &new_conf, SVC_CONF( osp ) )) )
      {
         /*
          * The first action of readjust is to swap the service configuration
          * with nscp. This is the reason for passing the address of nscp
          * (so that on return nscp will *always* point to the old service
          * configuration).
          */
         if ( readjust( osp, &nscp ) == OK )
         {
            old_services++ ;
            drop_service = NO ;
         }
         else   /* the readjustment failed */
            drop_service = YES ;
         sc_free( nscp ) ;
      }
      else
         drop_service = YES ;

      if ( drop_service == YES )
      {
         /*
          * Procedure for disabling a service:
          *
          *      a. Deactivate the service to prevent new connections
          *      b. Terminate running servers and cancel retry attempts, in case
          *         of reconfiguration
          */
         terminate_servers( osp ) ;
         svc_deactivate( osp ) ;
         cancel_service_retries( osp ) ;

         /*
          * Deactivate the service; the service will be deleted only
          * if its reference count drops to 0.
          */
         /* Always remove the service, even if not all the children
          * have been killed, or there are other references.
          */
         psi_remove( iter ) ;
         msg( LOG_NOTICE, func, "service %s deactivated", sid ) ;
	 /*
	  * If the refcount is > 0 release also all the members
	  */
         if ( SVC_RELE( osp ) > 0 ) {
            svc_release( osp );
         }
         dropped_services++ ;
      }
   }

   psi_destroy( iter ) ;

   /*
    * All services have terminated by now, so close the old common logfile.
    * remember that swap_defaults put the old defaults section in new_conf.
    */
   close_default_log( &new_conf, def_log ) ;

   /*
    * At this point the new Lconf only contains services that were not
    * in the old Lconf.
    */
   new_services = cnf_start_services( &new_conf ) ;
   msg( LOG_NOTICE, func,
      "Reconfigured: new=%d old=%d dropped=%d (services)",
         new_services, old_services, dropped_services ) ;

   if ( stayalive_option == 0 ) {
      if ( ps.rws.available_services == 0 )
      {
         msg( LOG_CRIT, func, "No available services. Exiting" );
         if ( ps.ros.pid_file ) {
            unlink(ps.ros.pid_file);
         }
         exit( 1 ) ;
      }
   }

   cnf_free( &new_conf ) ; 
}


static void swap_defaults( struct configuration *new_conf )
{
   struct service_config *temp ;

   DEFAULT_LOG_ERROR( ps ) = FALSE ;
   DEFAULT_LOG( ps ) = NULL ;
   SWAP( DEFAULTS( ps ), CNF_DEFAULTS( new_conf ), temp ) ;
}


static void close_default_log(struct configuration *confp, xlog_h def_log)
{
   /* Close the common log file, if one was specified */
   if ( def_log != NULL )
      log_end( SC_LOG( CNF_DEFAULTS( confp ) ), def_log) ;
}


static void sendsig( struct server *serp, int sig )
{
   char      *sid   = SVC_ID( SERVER_SERVICE( serp ) ) ;
   pid_t      pid   = SERVER_PID( serp ) ;
   const char *func = "sendsig" ;

   /*
    * Always use a positive pid, because of the semantics of kill(2)
    */
   if ( pid > 0 )
   {
      msg( LOG_WARNING, func, "Sending signal %d to %s server %d",
                                    sig, sid, pid ) ;
      kill( pid, sig ) ;
      if ((sig == SIGTERM) || (sig == SIGKILL))
      {
         int i, killed = 0;
         struct timeval tv;

         /*
          * We will try 4 seconds to TERM or KILL it. If it hasn't
          * responded by 2.5 seconds, we will send a KILL to hasten
          * its demise.
          */

         tv.tv_sec = 0;
         tv.tv_usec = 500000; /* half a second */
         for (i=0; i<8; i++)
         {
            if( server_lookup(pid) == NULL )
	    {
	       killed = 1;
	       break;
	    } else {
               int wret = waitpid(pid, NULL, WNOHANG);
	       if (wret == pid) {
	          killed = 1;
	          break;
               }
               server_end(serp);
            }
	 
	    /* May not have responded to TERM, send a KILL */
	    if ( i == 5)
               kill( pid, SIGKILL ) ;
		 
            /* Not dead yet, give some time. */
            select(0, NULL, NULL, NULL, &tv);
         }

	 /*
	  * If it didn't die, expect problems rebinding to this port if
	  * a hard_reconfig is in process.
	  */
         if (!killed)
            msg( LOG_ERR, func, "Server %d did not exit after SIGKILL", 
	          pid ) ;
         /* no need to server_end() here.  The killed process will generate
          * a sigchld, which will invoke the signal handler, and clean things
          * up there.
          */
      }
   } 
   else if ( pid != 0 )
      msg( LOG_ERR, func, "Negative server pid = %d. Service %s", pid, sid ) ;
}


/*
 * Send signal sig to all running servers of service sp
 */
static void deliver_signal( struct service *sp, int sig )
{
   unsigned u ;

   for ( u = 0 ; u < pset_count( SERVERS( ps ) ) ; u++ )
   {
      struct server *serp ;

      serp = SERP( pset_pointer( SERVERS( ps ), u ) ) ;
      if ( SERVER_SERVICE( serp ) == sp ) {
         sendsig( serp, sig ) ;
         if ( (sig == SIGTERM) || (sig == SIGKILL) )
            u--;
      }
   }
}


/*
 * Terminate all servers of the specified service
 */
void terminate_servers( struct service *sp )
{
   int sig = SC_IS_INTERNAL( SVC_CONF( sp ) ) ? SIGTERM : SIGKILL ;

   deliver_signal( sp, sig ) ;
}


static void stop_interception( struct service *sp )
{
   deliver_signal( sp, INTERCEPT_SIG ) ;
}

/*
 * Stop logging. svc_activate starts logging and will leak a file
 * descriptor and memory if this is not called prior.
 */
static void stop_log( struct service *sp, 
                              struct service_config *old_conf )
{
   struct log *lp = SC_LOG( old_conf ) ;

   if ( LOG_GET_TYPE( lp ) != L_NONE && SVC_IS_LOGGING( sp ) )
      log_end( lp, SVC_LOG( sp ) ) ;
   SVC_LOG( sp ) = NULL ;
}

/*
 * Stop any logging and restart if necessary.
 * Note that this has the side-effect of using the new common log
 * handle as it should.
 */
static status_e restart_log( struct service *sp, 
                              struct service_config *old_conf )
{
   stop_log( sp, old_conf ); 
   return( log_start( sp, &SVC_LOG( sp ) ) ) ;
}


/*
 * Unregister past versions, register new ones
 * We do it the dumb way: first unregister; then register
 * We try to be a little smart by checking if there has
 * been any change in version numbers (if not, we do nothing).
 * Also, we save the port number
 */
static status_e readjust_rpc_service( struct service_config *old_scp, 
                                       struct service_config *new_scp )
{
   unsigned long      vers ;
   uint16_t           port                  = SC_PORT( old_scp ) ;
   struct rpc_data   *new_rdp               = SC_RPCDATA( new_scp ) ;
   struct rpc_data   *old_rdp               = SC_RPCDATA( old_scp ) ;
   unsigned           registered_versions   = 0 ;
   const char        *func                  = "readjust_rpc_service" ;

#ifndef NO_RPC
   SC_PORT( new_scp ) = SC_PORT( old_scp ) ;

   if ( RD_MINVERS( old_rdp ) == RD_MINVERS( new_rdp ) &&
            RD_MAXVERS( old_rdp ) == RD_MAXVERS( new_rdp ) )
      return( OK ) ;

   for ( vers = RD_MINVERS( old_rdp ) ; vers <= RD_MAXVERS( old_rdp ) ; vers++ )
       (void) pmap_unset( RD_PROGNUM( old_rdp ), vers ) ;

   for ( vers = RD_MINVERS( new_rdp ) ; vers <= RD_MAXVERS( new_rdp ) ; vers++ )
      if ( pmap_set( RD_PROGNUM( new_rdp ),
                              vers, SC_PROTOVAL( new_scp ), port ) )
         registered_versions++ ;
      else
         msg( LOG_ERR, func, 
            "pmap_set failed. service=%s, program=%ld, version = %ld",
               SC_ID( new_scp ), RD_PROGNUM( new_rdp ), vers ) ;

   if ( registered_versions == 0 )
   {
      msg( LOG_ERR, func,
            "No versions registered for RPC service %s", SC_ID( new_scp ) ) ;
      /*
       * Avoid the pmap_unset
       */
      RD_MINVERS( new_rdp ) = RD_MAXVERS( new_rdp ) + 1 ;
      return( FAILED ) ;
   }
#endif   /* ! NO_RPC */
   return( OK ) ;
}


/*
 * Readjust service attributes. 
 *
 * We assume that the following attributes are the same:
 *         wait
 *         socket_type
 *         type
 *         protocol
 *
 * Readjustment happens in 3 steps:
 *      1) We swap the svc_conf fields
 *            This has the side-effect of free'ing the memory associated
 *            with the old service configuration when the new configuration
 *            is destroyed.
 *      2) We readjust the fields that require some action to be taken:
 *            RPC mapping
 *            log file open
 *      3) We update the address control fields.
 */
static status_e readjust( struct service *sp, 
                           struct service_config **new_conf_ptr )
{
   struct service_config   *temp_conf ;
   struct service_config   *old_conf   = SVC_CONF( sp ) ;
   struct service_config   *new_conf   = *new_conf_ptr ;
   char                    *sid        = SVC_ID( sp ) ;
   const char              *func       = "readjust" ;

   msg( LOG_NOTICE, func, "readjusting service %s", sid ) ;

   SWAP( SVC_CONF( sp ), *new_conf_ptr, temp_conf ) ;

   if ( SC_IS_RPC( old_conf ) &&
                  readjust_rpc_service( old_conf, new_conf ) == FAILED )
      return( FAILED ) ;
   
   /*
    * This is what happens if the INTERCEPT flag is toggled and an
    * interceptor is running:
    *
    * Case 1: clear->set
    *      Wait until the server dies (soft reconfig) or
    *      terminate the server (hard reconfig)
    *
    * Case 2: set->clear
    *    Send a signal to the interceptor to tell it to stop intercepting
    */
   if ( SC_IS_INTERCEPTED( old_conf ) != SC_IS_INTERCEPTED( new_conf ) )
   {
      if ( SC_IS_INTERCEPTED( new_conf ) )         /* case 1 */
         terminate_servers( sp ) ;
      else                                       /* case 2 */
      {
         stop_interception( sp ) ;
         msg( LOG_NOTICE, func, "Stopping interception for %s", sid ) ;
      }
   }

   /* 
    * See if the bind address was specified in both the old and new config,
    * then if it changed, readjust the service. The algorithm is check to 
    * see if they are in the same address family, if so start a simple 
    * comparison based on the address family. If IPv4, the addresses can be 
    * compared directly, otherwise use the IPv6 macro. If they are not the 
    * same, terminate & restart the service. 
    */
   if( (SC_BIND_ADDR(old_conf) != NULL) && (SC_BIND_ADDR(new_conf) != NULL) ) {
      int same = 0;

      if ( SA(SC_BIND_ADDR(old_conf))->sa_family == 
           SA(SC_BIND_ADDR(new_conf))->sa_family ) {
         if ( SA(SC_BIND_ADDR(old_conf))->sa_family == AF_INET ) {
            if ( SAIN(SC_BIND_ADDR(old_conf))->sin_addr.s_addr == 
                 SAIN(SC_BIND_ADDR(new_conf))->sin_addr.s_addr)
               same = 1;
         }
         else if ( IN6_ARE_ADDR_EQUAL(
                  &SAIN6(SC_BIND_ADDR(old_conf))->sin6_addr, 
                  &SAIN6(SC_BIND_ADDR(new_conf))->sin6_addr) )
            same = 1;
      }
      
      if ( !same ) {
         terminate_servers( sp );
         svc_deactivate( sp );
         stop_log( sp, old_conf ); /* svc_activate re-starts logging */
         svc_activate( sp );
         return OK;
      }
   }

   /* If the service didn't have a bind address before, but does now,
    * make sure the new bind directive takes effect.
    */
   if( (SC_BIND_ADDR(old_conf) == NULL) && (SC_BIND_ADDR(new_conf) != NULL) ) {
      terminate_servers( sp );
      svc_deactivate(sp);
      stop_log( sp, old_conf ); /* svc_activate re-starts logging */
      svc_activate(sp);
      return OK;
   }

   if( (SC_IPV4(old_conf) && SC_IPV6(new_conf)) || 
         (SC_IPV6(old_conf) && SC_IPV4(new_conf)) ) {
      terminate_servers( sp );
      svc_deactivate(sp);
      stop_log( sp, old_conf ); /* svc_activate re-starts logging */
      svc_activate(sp);
      return OK;
   }

   return( restart_log( sp, old_conf ) ) ;
}

