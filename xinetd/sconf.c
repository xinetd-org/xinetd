/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "sio.h"
#include "sconf.h"
#include "timex.h"
#include "addr.h"
#include "nvlists.h"
#include "xmdns.h"


#define NEW_SCONF()               NEW( struct service_config )
#define FREE_SCONF( scp )         FREE( scp )

/*
 * Conditional free; checks if the pointer is NULL
 */
#define COND_FREE( x )           if ( x )                   \
                                 {                          \
                                    *x = NUL ;              \
                                    free( (char *) x ) ;    \
                                 }


/*
 * Allocate a new service_config and initialize the service name field
 * with 'name'; the rest of the fields are set to 0 which gives them
 * their default values.
 */
struct service_config *sc_alloc( const char *name )
{
   struct service_config *scp ;
   const char *func = "sc_alloc" ;

   scp = NEW_SCONF() ;
   if ( scp == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   CLEAR( *scp ) ;
   SC_NAME(scp) = new_string( name ) ;
#ifdef HAVE_MDNS
   xinetd_mdns_svc_init(scp);
#endif
   return( scp ) ;
}


static void release_string_pset( pset_h pset )
{
   pset_apply( pset, free, NULL ) ;
   pset_destroy( pset ) ;
}


/*
 * Free all malloc'ed memory for the specified service
 */
void sc_free( struct service_config *scp )
{
#ifdef HAVE_MDNS
   COND_FREE( SC_MDNS_NAME(scp) );
   xinetd_mdns_svc_free(scp);
#endif
#ifdef LIBWRAP
   COND_FREE( SC_LIBWRAP(scp) );
#endif
   COND_FREE( SC_NAME(scp) ) ;
   COND_FREE( SC_ID(scp) ) ;
   COND_FREE( SC_PROTONAME(scp) ) ;
   COND_FREE( SC_SERVER(scp) ) ;
   COND_FREE( (char *)SC_REDIR_ADDR(scp) ) ;
   COND_FREE( (char *)SC_BIND_ADDR(scp) ) ;
   COND_FREE( (char *)SC_ORIG_BIND_ADDR(scp) ) ;
   COND_FREE( (char *)SC_BANNER(scp) ) ;
   COND_FREE( (char *)SC_BANNER_SUCCESS(scp) ) ;
   COND_FREE( (char *)SC_BANNER_FAIL(scp) ) ;
   if ( SC_SERVER_ARGV(scp) )
   {
      char **pp ;

      /*
       * argv[ 0 ] is a special case because it may not have been allocated yet
       */
      if ( SC_SERVER_ARGV(scp)[ 0 ] != NULL)
         free( SC_SERVER_ARGV(scp)[ 0 ] ) ;
      for ( pp = &SC_SERVER_ARGV(scp)[ 1 ] ; *pp != NULL ; pp++ )
         free( *pp ) ;
      free( (char *) SC_SERVER_ARGV(scp) ) ;
   }
   COND_FREE( LOG_GET_FILELOG( SC_LOG( scp ) )->fl_filename ) ;

   if ( SC_ACCESS_TIMES(scp) != NULL )
   {
      ti_free( SC_ACCESS_TIMES(scp) ) ;
      pset_destroy( SC_ACCESS_TIMES(scp) ) ;
   }

   if ( SC_ONLY_FROM(scp) != NULL )
   {
      addrlist_free( SC_ONLY_FROM(scp) ) ;
      pset_destroy( SC_ONLY_FROM(scp) ) ;
   }

   if ( SC_NO_ACCESS(scp) != NULL )
   {
      addrlist_free( SC_NO_ACCESS(scp) ) ;
      pset_destroy( SC_NO_ACCESS(scp) ) ;
   }

   if ( SC_ENV_VAR_DEFS(scp) != NULL )
      release_string_pset( SC_ENV_VAR_DEFS(scp) ) ;
   if ( SC_PASS_ENV_VARS(scp) != NULL )
      release_string_pset( SC_PASS_ENV_VARS(scp) ) ;
   if ( SC_ENV( scp )->env_type == CUSTOM_ENV && 
                                    SC_ENV( scp )->env_handle != ENV_NULL )
      env_destroy( SC_ENV( scp )->env_handle ) ;
   if (SC_DISABLED(scp) ) 
      release_string_pset( SC_DISABLED(scp) ) ;
   if (SC_ENABLED(scp) ) 
      release_string_pset( SC_ENABLED(scp) ) ;
   
   CLEAR( *scp ) ;
   FREE_SCONF( scp ) ;
}


/*
 * Create a configuration for one of the special services
 */
struct service_config *sc_make_special( const char *service_name, 
                                        const builtin_s *bp, 
                                        int instances )
{
   struct service_config *scp ;
   const char *func = "sc_make" ;

   if ( ( scp = sc_alloc( service_name ) ) == NULL )
      return( NULL ) ;

   SC_ID(scp) = new_string( SC_NAME(scp) ) ;
   if ( SC_ID(scp) == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   SC_SPECIFY( scp, A_ID ) ;

   /*
    * All special services are internal
    */
   M_SET( SC_TYPE(scp), ST_SPECIAL ) ;
   M_SET( SC_TYPE(scp), ST_INTERNAL ) ;
   SC_BUILTIN(scp) = bp ;
   SC_SPECIFY( scp, A_TYPE ) ;

   M_SET( SC_XFLAGS(scp), SF_NORETRY ) ;
   SC_SPECIFY( scp, A_FLAGS ) ;

   SC_INSTANCES(scp) = instances ;
   SC_SPECIFY( scp, A_INSTANCES ) ;

   SC_WAIT(scp) = NO ;
   SC_SPECIFY( scp, A_WAIT ) ;

   return( scp ) ;
}


static void dump_log_data( int fd, struct service_config *scp, int tab_level )
{
   struct log *lp = SC_LOG( scp ) ;
   struct filelog *flp ;
   int i ;

   switch ( LOG_GET_TYPE( lp ) )
   {
      case L_NONE:
         tabprint( fd, tab_level, "No logging\n" ) ;
         return ;

      case L_COMMON_FILE:
         tabprint( fd, tab_level, "Logging to common log file\n" ) ;
         break ;

      case L_FILE:
         flp = LOG_GET_FILELOG( lp ) ;
         tabprint( fd, tab_level, "Logging to file: %s", flp->fl_filename ) ;

         if ( FILELOG_SIZE_CONTROL( flp ) )
            Sprint( fd, " (soft=%d hard=%d)\n",
                        flp->fl_soft_limit, flp->fl_hard_limit ) ;
         else
            Sprint( fd, " (no limits)\n" ) ;
         break ;
      
      case L_SYSLOG:
         tabprint( fd, tab_level,
            "Logging to syslog. Facility = %s, level = %s\n",
               nv_get_name( syslog_facilities, lp->l_sl.sl_facility ),
               nv_get_name( syslog_levels, lp->l_sl.sl_level ) ) ;
         break ;
   }

   tabprint( fd, tab_level, "Log_on_success flags =" ) ;
   for ( i = 0 ; success_log_options[ i ].name != NULL ; i++ )
      if ( M_IS_SET( SC_LOG_ON_SUCCESS(scp), success_log_options[ i ].value ) )
         Sprint( fd, " %s", success_log_options[ i ].name ) ;
   Sputchar( fd, '\n' ) ;

   tabprint( fd, tab_level, "Log_on_failure flags =" ) ;
   for ( i = 0 ; failure_log_options[ i ].name != NULL ; i++ )
      if ( M_IS_SET( SC_LOG_ON_FAILURE(scp), failure_log_options[ i ].value ) )
         Sprint( fd, " %s", failure_log_options[ i ].name ) ;
   Sputchar( fd, '\n' ) ;
}


/*
 * Print info about service scp to file descriptor fd
 */
void sc_dump( struct service_config *scp, 
              int fd, 
              int tab_level, 
              bool_int is_defaults )
{
   const struct name_value    *nvp ;
   unsigned             u ;
   char                 **pp ;

   if ( is_defaults )
      tabprint( fd, tab_level, "Service defaults\n" ) ;
   else
      tabprint( fd, tab_level, "Service configuration: %s\n", SC_NAME(scp) ) ;

   if ( ! is_defaults )
   {
      tabprint( fd, tab_level+1, "id = %s\n", SC_ID(scp) ) ;

      if ( ! M_ARE_ALL_CLEAR( SC_XFLAGS(scp) ) )
      {
         tabprint( fd, tab_level+1, "flags =" ) ;
         for ( nvp = &service_flags[ 0 ] ; nvp->name != NULL ; nvp++ )
            if ( M_IS_SET( SC_XFLAGS(scp), nvp->value ) )
               Sprint( fd, " %s", nvp->name ) ;
         Sputchar( fd, '\n' ) ;
      }

      if ( ! M_ARE_ALL_CLEAR( SC_TYPE(scp) ) )
      {
         tabprint( fd, tab_level+1, "type =" ) ;
         for ( nvp = &service_types[ 0 ] ; nvp->name != NULL ; nvp++ )
            if ( M_IS_SET( SC_TYPE(scp), nvp->value ) )
               Sprint( fd, " %s", nvp->name ) ;
         Sputchar( fd, '\n' ) ;
      }

      tabprint( fd, tab_level+1, "socket_type = %s\n",
         nv_get_name( socket_types, SC_SOCKET_TYPE(scp) ) ) ;

      tabprint( fd, tab_level+1, "Protocol (name,number) = (%s,%d)\n",
            SC_PROTONAME(scp), SC_PROTOVAL(scp) ) ;
      
      if ( SC_SPECIFIED( scp, A_PORT ) )
         tabprint( fd, tab_level+1, "port = %d\n", SC_PORT(scp) ) ;
   }

   if ( SC_SPECIFIED( scp, A_INSTANCES ) ) {
      if ( SC_INSTANCES(scp) == UNLIMITED )
         tabprint( fd, tab_level+1, "Instances = UNLIMITED\n" ) ;
      else
         tabprint( fd, tab_level+1, "Instances = %d\n", SC_INSTANCES(scp) ) ;
   }

   if ( SC_SPECIFIED( scp, A_WAIT ) ) {
      if ( SC_WAIT(scp) )
         tabprint( fd, tab_level+1, "wait = yes\n" ) ;
      else
         tabprint( fd, tab_level+1, "wait = no\n" ) ;
   }
      
   if ( SC_SPECIFIED( scp, A_USER ) )
      tabprint( fd, tab_level+1, "user = %d\n", SC_UID(scp) ) ;
      
   if ( SC_SPECIFIED( scp, A_GROUP ) )
      tabprint( fd, tab_level+1, "group = %d\n", SC_GID(scp) ) ;
      
   if ( SC_SPECIFIED( scp, A_GROUPS ) )
   {
      if (SC_GROUPS(scp) == 1)
         tabprint( fd, tab_level+1, "Groups = yes\n" );
      else
         tabprint( fd, tab_level+1, "Groups = no\n" );
   }

   if ( SC_SPECIFIED( scp, A_UMASK ) )
      tabprint( fd, tab_level+1, "umask = %o\n", SC_UMASK(scp) ) ;
      
   if ( SC_SPECIFIED( scp, A_NICE ) )
      tabprint( fd, tab_level+1, "Nice = %d\n", SC_NICE(scp) ) ;

   if ( SC_SPECIFIED( scp, A_CPS ) )
      tabprint( fd, tab_level+1, "CPS = max conn:%lu wait:%lu\n", 
         SC_TIME_CONN_MAX(scp), SC_TIME_WAIT(scp) );

   if ( SC_SPECIFIED( scp, A_PER_SOURCE ) )
      tabprint( fd, tab_level+1, "PER_SOURCE = %d\n", 
         SC_PER_SOURCE(scp) );

   if ( SC_SPECIFIED( scp, A_BIND ) ) {
	   if (  SC_BIND_ADDR(scp) ) {
		  char bindname[NI_MAXHOST];
		  unsigned int len = 0;
		  if( SC_BIND_ADDR(scp)->sa.sa_family == AF_INET ) 
			 len = sizeof(struct sockaddr_in);
		  else  
			 len = sizeof(struct sockaddr_in6);
		  memset(bindname, 0, sizeof(bindname));
		  if( getnameinfo(&SC_BIND_ADDR(scp)->sa, len, bindname, 
                                  NI_MAXHOST, NULL, 0, 0) != 0 ) 
			 strcpy(bindname, "unknown");
		  tabprint( fd, tab_level+1, "Bind = %s\n", bindname );
	   }
	   else if ( SC_ORIG_BIND_ADDR(scp) ) {
		  tabprint( fd, tab_level+1, "Bind = %s\n", 
                            SC_ORIG_BIND_ADDR(scp) );
	   }
	   else { /* This should NEVER happen */
		msg(LOG_ERR, "sc_dump", "bad configuration for %s:", 
                    SC_NAME(scp));
	   }
   }
   else
      tabprint( fd, tab_level+1, "Bind = All addresses.\n" );

   if ( ! is_defaults )
   {
      if ( (! SC_IS_INTERNAL( scp )) && (SC_REDIR_ADDR(scp) == NULL) )
      {
         tabprint( fd, tab_level+1, "Server = %s\n", SC_SERVER(scp) ) ;
         tabprint( fd, tab_level+1, "Server argv =" ) ;
	 if ( SC_SERVER_ARGV(scp) )
	 {
            for ( pp = SC_SERVER_ARGV(scp) ; *pp ; pp++ )
               Sprint( fd, " %s", *pp ) ;
	 }
	 else
	    Sprint( fd, " (NULL)");
         Sputchar( fd, '\n' ) ;
      } 

#ifdef LIBWRAP
      if ( SC_LIBWRAP(scp) != NULL )
      {
         tabprint( fd, tab_level + 1, "Libwrap = %s\n", SC_LIBWRAP(scp) );
      }
#endif

      if ( SC_REDIR_ADDR(scp) != NULL ) 
      {
         char redirname[NI_MAXHOST];
         unsigned int len = 0;
         if( SC_REDIR_ADDR(scp)->sa.sa_family == AF_INET ) 
            len = sizeof(struct sockaddr_in);
         if( SC_REDIR_ADDR(scp)->sa.sa_family == AF_INET6 ) 
            len = sizeof(struct sockaddr_in6);
         memset(redirname, 0, sizeof(redirname));
         if( getnameinfo(&SC_REDIR_ADDR(scp)->sa, len,  redirname, NI_MAXHOST, 
               NULL, 0, 0) != 0 ) 
            strcpy(redirname, "unknown");
         tabprint( fd, tab_level+1, "Redirect = %s:%d\n", redirname, 
	    SC_REDIR_ADDR(scp)->sa_in.sin_port );
      }

      if ( SC_IS_RPC( scp ) )
      {
         struct rpc_data *rdp = SC_RPCDATA( scp ) ;

         tabprint( fd, tab_level+1, "RPC data\n" ) ;
         tabprint( fd, tab_level+2,
                           "program number = %ld\n", rdp->rd_program_number ) ;
         tabprint( fd, tab_level+2, "rpc_version = " ) ;
         if ( rdp->rd_min_version == rdp->rd_max_version )
            Sprint( fd, "%ld\n", rdp->rd_min_version ) ;
         else
            Sprint( fd, "%ld-%ld\n",
                           rdp->rd_min_version, rdp->rd_max_version ) ;
      }

      if ( SC_SPECIFIED( scp, A_ACCESS_TIMES ) )
      {
         tabprint( fd, tab_level+1, "Access times =" ) ;
         ti_dump( SC_ACCESS_TIMES(scp), fd ) ;
         Sputchar ( fd, '\n' ) ;
      }
   }

   /* This is important enough that each service should list it. */
   tabprint( fd, tab_level+1, "Only from: " ) ;
   if ( SC_ONLY_FROM(scp) )
   {  /* Next check is done since -= doesn't zero out lists. */
      if ( pset_count(SC_ONLY_FROM(scp)) == 0)
         Sprint( fd, "All sites" );
      else
         addrlist_dump( SC_ONLY_FROM(scp), fd ) ;
   }
   else
      Sprint( fd, "All sites" );
   Sputchar( fd, '\n' ) ;

   /* This is important enough that each service should list it. */
   tabprint( fd, tab_level+1, "No access: " ) ;
   if ( SC_NO_ACCESS(scp) )
   {  /* Next check is done since -= doesn't zero out lists. */
      if ( pset_count(SC_NO_ACCESS(scp)) == 0)
         Sprint( fd, "No blocked sites" );
      else
         addrlist_dump( SC_NO_ACCESS(scp), fd ) ;
   }
   else
      Sprint( fd, "No blocked sites" );
   Sputchar( fd, '\n' ) ;

   if ( SC_SENSOR(scp) )
   {
      tabprint( fd, tab_level+1, "Deny Time: " ) ;
      Sprint( fd, "%d\n", SC_DENY_TIME(scp));
   }
   
   dump_log_data( fd, scp, tab_level+1 ) ;

   if ( SC_IS_PRESENT( scp, A_PASSENV ) )
   {
      tabprint( fd, tab_level+1, "Passenv =" ) ;
      for ( u = 0 ; u < pset_count( SC_PASS_ENV_VARS(scp) ) ; u++ )
         Sprint( fd, " %s",
                  (char *) pset_pointer( SC_PASS_ENV_VARS(scp), u ) ) ;
      Sputchar ( fd, '\n' ) ;
   }

   if ( ! is_defaults )
      if ( SC_SPECIFIED( scp, A_ENV ) )
      {
         tabprint( fd, tab_level+1, "Environment additions:\n" ) ;
         for ( u = 0 ; u < pset_count( SC_ENV_VAR_DEFS(scp) ) ; u++ )
            tabprint( fd, tab_level+2,
                  "%s\n", (char *) pset_pointer( SC_ENV_VAR_DEFS(scp), u ) ) ;
      }
   
   if ( SC_ENV( scp )->env_type == CUSTOM_ENV )
   {
      tabprint( fd, tab_level+1, "Environment strings:\n" ) ;
      for ( pp = env_getvars( SC_ENV( scp )->env_handle ) ; *pp ; pp++ )
         tabprint( fd, tab_level+2, "%s\n", *pp ) ;
   }
   Sflush( fd ) ;
}


#define SC_RPCPROGNUM( s )    RD_PROGNUM( SC_RPCDATA( s ) )
#define SAME_RPC( s1, s2 )    ( SC_RPCPROGNUM( s1 ) == SC_RPCPROGNUM( s2 ) )
#define SAME_NONRPC( s1, s2 ) ( SC_SOCKET_TYPE((s1)) == SC_SOCKET_TYPE((s2)) \
                                 && SC_PORT((s1)) == SC_PORT((s2)) )

/*
 * Two service configurations are considered different if any of the
 * following is TRUE:
 *      1) only one is unlisted
 *      2) only one is internal
 *      3) only one is RPC
 *      4) they have different values for the 'wait' attribute
 *      5) they use different protocols
 *      6) they are both RPC services but have different program numbers
 *      7) neither is an RPC service and they have different socket_types or
 *         use diffent ports
 *
 * This function returns TRUE if the specified configurations are different.
 *
 * Note that this function is closely related to the 'readjust' function
 * that is invoked on reconfiguration; that function will not change 
 * attributes that this function checks to determine if two configurations
 * are different.
 */
bool_int sc_different_confs( struct service_config *scp1, 
                             struct service_config *scp2 )
{
   if ( SC_IS_UNLISTED( scp1 ) != SC_IS_UNLISTED( scp2 ) ||
            SC_IS_INTERNAL( scp1 ) != SC_IS_INTERNAL( scp2 ) ||
               SC_IS_RPC( scp1 ) != SC_IS_RPC( scp2 ) )
      return( TRUE ) ;

   if ( SC_WAIT(scp1) != SC_WAIT(scp2) )
      return( TRUE ) ;
  
   if ( SC_PROTOVAL(scp1) != SC_PROTOVAL(scp2) )
      return( TRUE ) ;

   if ( SC_IS_RPC( scp1 ) )
   {
      if ( ! SAME_RPC( scp1, scp2 ) )
         return( TRUE ) ;
   }
   else
   {
      if ( ! SAME_NONRPC( scp1, scp2 ) )
         return( TRUE ) ;
   }
   return( FALSE ) ;
}

