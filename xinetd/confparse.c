/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <sys/types.h>
#include <syslog.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_RPC_RPC_H
#include <rpc/rpc.h>
#endif

#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include "str.h"
#include "sio.h"
#include "confparse.h"
#include "msg.h"
#include "xconfig.h"
#include "parse.h"
#include "special.h"
#include "sconst.h"
#include "env.h"
#include "sconf.h"
#include "sensor.h"
#include "inet.h"
#include "main.h"

extern int inetd_compat;

/*
 * Pset iterator used by functions in this file.
 * It lives only when get_configuration is called (i.e. it is created and
 * destroyed each time). This is because the pset it is iterating on
 * changes.
 */
static psi_h iter ;

static status_e fix_server_argv( struct service_config *scp )
{
   char *server_name ;
   const char *func = "fix_server_argv" ;

   if( SC_SERVER(scp) == NULL )
   {
      msg( LOG_ERR, func, 
           "Must specify a server in %s", SC_NAME(scp));
      return( FAILED );
   }
   
   if( SC_NAMEINARGS( scp ) ) {
      if( !SC_SPECIFIED(scp, A_SERVER_ARGS ) ){
         msg( LOG_ERR, func, 
              "Must specify server args if using NAMEINARGS flag");
         return( FAILED );
      }

      return ( OK );
   }

   /*
    * Check if the user specified any server arguments.
    * If not, then the server_argv has not been allocated yet,
    * so malloc it (size 2)
    * Put in argv[ 0 ] the last component of the server pathname
    */
   if ( ! SC_SPECIFIED( scp, A_SERVER_ARGS ) )
   {
      SC_SERVER_ARGV(scp) = (char **) malloc( 2 * sizeof( char * ) ) ;
      if ( SC_SERVER_ARGV(scp) == NULL )
      {
         out_of_memory( func ) ;
         return( FAILED ) ;
      }
      SC_SERVER_ARGV(scp)[ 0 ] = NULL ;
      SC_SERVER_ARGV(scp)[ 1 ] = NULL ;
      SC_PRESENT( scp, A_SERVER_ARGS ) ;
   }

   /*
    * Determine server name
    */
   server_name = strrchr( SC_SERVER(scp), '/' ) ;
   if ( server_name == NULL )
      server_name = SC_SERVER(scp) ;
   else
      server_name++ ;      /* skip the '/' */

   /*
    * Place it in argv[ 0 ]
    */
   SC_SERVER_ARGV(scp)[ 0 ] = new_string( server_name ) ;
   if ( SC_SERVER_ARGV(scp)[ 0 ] == NULL )
   {
      out_of_memory( func ) ;
      return( FAILED ) ;
   }
   return( OK ) ;
}



#define USE_DEFAULT( scp, def, attr_id )   \
         ( ! SC_SPECIFIED( scp, attr_id ) && SC_SPECIFIED( def, attr_id ) )

/*
 * Fill the service configuration with attributes that were not
 * explicitly specified. These can be:
 *      1) implied attributes (like the server name in argv[0])
 *      2) attributes from 'defaults' so that we won't need to check
 *         'defaults' anymore.
 *      3) default values (like the service instance limit)
 */
static status_e service_fill( struct service_config *scp, 
                            struct service_config *def )
{
   const char *func = "service_fill" ;

   /* Note: if the service was specified, it won't be honored. */
   if( (SC_REDIR_ADDR(scp) != NULL) ) {
       if ( SC_SPECIFIED( scp, A_SERVER ) && SC_SERVER(scp))
          free(SC_SERVER(scp));
       SC_SERVER(scp) = new_string( "/bin/true" );
       SC_SPECIFY(scp, A_SERVER);
   }

   if ( ! SC_IS_INTERNAL( scp ) && fix_server_argv( scp ) == FAILED )
      return( FAILED ) ;

   /* 
    * FIXME: Should all these set SPECIFY or PRESENT ?
    * PRESENT means that either a default or specified value.
    * SPECIFIED means that the user specified a value. 
    * PRESENT makes more sense for default values. -SG
    */
   if ( ! SC_SPECIFIED( scp, A_INSTANCES ) )
   {
      SC_INSTANCES(scp) = SC_SPECIFIED( def, A_INSTANCES ) ? SC_INSTANCES(def)
                                                     : DEFAULT_INSTANCE_LIMIT ;
      SC_PRESENT( scp, A_INSTANCES ) ;
   }

   if ( (! SC_SPECIFIED( scp, A_UMASK )) && SC_SPECIFIED( def, A_UMASK ) ) 
   {
      SC_UMASK(scp) = SC_UMASK(def);
      SC_SPECIFY( scp, A_UMASK );
   }

   if ( ! SC_SPECIFIED( scp, A_PER_SOURCE ) )
   {
      SC_PER_SOURCE(scp) = SC_SPECIFIED( def, A_PER_SOURCE ) ? 
         SC_PER_SOURCE(def) : DEFAULT_INSTANCE_LIMIT ;
      SC_SPECIFY( scp, A_PER_SOURCE ) ;
   }

#ifdef HAVE_MDNS
   if ( ! SC_SPECIFIED( scp, A_MDNS ) )
   {
      SC_MDNS(scp) = SC_SPECIFIED( def, A_MDNS ) ? SC_MDNS(def) : YES;
      SC_SPECIFY( scp, A_MDNS );
   }
#endif

   if ( ! SC_SPECIFIED( scp, A_GROUPS ) )
   {
      SC_GROUPS(scp) = SC_SPECIFIED( def, A_GROUPS ) ? SC_GROUPS(def) : NO;
      SC_SPECIFY( scp, A_GROUPS );
   }

   if ( ! SC_SPECIFIED( scp, A_CPS ) ) 
   {
      SC_TIME_CONN_MAX(scp) = SC_SPECIFIED( def, A_CPS ) ? 
         SC_TIME_CONN_MAX(def) : DEFAULT_LOOP_RATE;
      SC_TIME_WAIT(scp) = SC_SPECIFIED( def, A_CPS ) ? 
         SC_TIME_WAIT(def) : DEFAULT_LOOP_TIME;
      SC_TIME_REENABLE(scp) = 0;
   }

#ifdef HAVE_LOADAVG
   if ( ! SC_SPECIFIED( scp, A_MAX_LOAD ) ) {
      SC_MAX_LOAD(scp) = SC_SPECIFIED( def, A_MAX_LOAD ) ? SC_MAX_LOAD(def) : 0;
      SC_SPECIFY( scp, A_MAX_LOAD ) ;
   }
#endif

   /* 
    * we need to check a few things. A_BIND can be specified & sc_bind_addr
    * is NULL. This means the address couldn't be determined in bind_parser
    * and it was stored into sc_orig_bind_addr. We unset the attribute
    * so that its processed correctly.
    */
   if (SC_SPECIFIED( scp, A_BIND ) && SC_BIND_ADDR(scp) == NULL)
      M_CLEAR( scp->sc_specified_attributes, A_BIND ) ;
   
   if ( (! SC_SPECIFIED( scp, A_BIND )) && (SC_ORIG_BIND_ADDR(scp) == 0) ) {
      if ( SC_SPECIFIED( def, A_BIND ) ) {
         SC_BIND_ADDR(scp) = (union xsockaddr *)malloc(sizeof(union xsockaddr));
         if( SC_BIND_ADDR(scp) == NULL ) {
            msg(LOG_ERR, func, "can't allocate space for bind addr");
            return( FAILED );
         }
         memcpy(SC_BIND_ADDR(scp), SC_BIND_ADDR(def), sizeof(union xsockaddr));
         SC_SPECIFY( scp, A_BIND ) ;
      }
      else if ( SC_ORIG_BIND_ADDR(def) )
         SC_ORIG_BIND_ADDR(scp) = new_string( SC_ORIG_BIND_ADDR(def) );
   }
   
   if ( ! SC_SPECIFIED( scp, A_V6ONLY ) ) {
      SC_V6ONLY(scp) = SC_SPECIFIED( def, A_V6ONLY ) ? SC_V6ONLY(def) : NO;
   }

   if ( ! SC_SPECIFIED( scp, A_DENY_TIME ) )
   {
      SC_DENY_TIME(scp) = SC_SPECIFIED( def, A_DENY_TIME ) ? 
         SC_DENY_TIME(def)  : 0 ;
      SC_SPECIFY( scp, A_DENY_TIME ) ;
   }

   if ( (!SC_IPV4( scp )) && (!SC_IPV6( scp )) )
   {
      /*
       * If bind is specified, check the address and see what family is
       * available. If not, then use default.
       */
      if ( SC_SPECIFIED( scp, A_BIND ) && !SC_ORIG_BIND_ADDR(scp) ) 
      {
	  if ( SAIN(SC_BIND_ADDR(scp))->sin_family == AF_INET )
             M_SET(SC_XFLAGS(scp), SF_IPV4);
	  else
             M_SET(SC_XFLAGS(scp), SF_IPV6);
      }
      else
         M_SET(SC_XFLAGS(scp), SF_IPV4);
   }

   if (SC_ORIG_BIND_ADDR(scp))
   {
      /*
       * If we are here, we have a dual stack machine with multiple
       * entries for a domain name. We can finally use the flags for
       * a hint to see which one to use.
       */
      struct addrinfo hints, *res;

      memset(&hints, 0, sizeof(hints));
      hints.ai_flags = AI_CANONNAME;
      if (SC_IPV6(scp))
         hints.ai_family = AF_INET6;
      else
         hints.ai_family = AF_INET;

      if( getaddrinfo(SC_ORIG_BIND_ADDR(scp), NULL, &hints, &res) < 0 ) 
      {
         msg(LOG_ERR, func, "bad address given for: %s", SC_NAME(scp));
         return( FAILED );
      }

      if( (res == NULL) || (res->ai_addr == NULL) ) 
      {
         msg(LOG_ERR, func, "no addresses returned for: %s", SC_NAME(scp));
	 return( FAILED );
      }

      if( (res->ai_family == AF_INET) || (res->ai_family == AF_INET6) )
      {
         SC_BIND_ADDR(scp) = (union xsockaddr *)
            malloc(sizeof(union xsockaddr));
         if( SC_BIND_ADDR(scp) == NULL )
         {
            msg(LOG_ERR, func, "can't allocate space for bind addr of: %s",
                SC_NAME(scp));
            return( FAILED );
         }
         memset(SC_BIND_ADDR(scp), 0, sizeof(union xsockaddr));
         memcpy(SC_BIND_ADDR(scp), res->ai_addr, res->ai_addrlen);
         free(SC_ORIG_BIND_ADDR(scp));
         SC_ORIG_BIND_ADDR(scp) = 0;
 	 SC_SPECIFY( scp, A_BIND );
      }
      freeaddrinfo(res);
   }

   /* This should be removed if sock_stream is ever something other than TCP,
    * or sock_dgram is ever something other than UDP.
    */
   if ( (! SC_SPECIFIED( scp, A_PROTOCOL )) && 
	( SC_SPECIFIED( scp, A_SOCKET_TYPE ) ) )
   {
      struct protoent *pep ;

      if( SC_SOCKET_TYPE(scp) == SOCK_STREAM ) {
         if( (pep = getprotobyname( "tcp" )) != NULL ) {
            SC_PROTONAME(scp) = new_string ( "tcp" );
            if( SC_PROTONAME(scp) == NULL )
               return( FAILED );
            SC_PROTOVAL(scp) = pep->p_proto ;
            SC_SPECIFY(scp, A_PROTOCOL);
         }
      }

      if( SC_SOCKET_TYPE(scp) == SOCK_DGRAM ) {
         if( (pep = getprotobyname( "udp" )) != NULL ) {
            SC_PROTONAME(scp) = new_string ( "udp" );
            if( SC_PROTONAME(scp) == NULL )
               return( FAILED );
            SC_PROTOVAL(scp) = pep->p_proto ;
            SC_SPECIFY(scp, A_PROTOCOL);
         }
      }
   }
   if ( ( SC_SPECIFIED( scp, A_PROTOCOL )) && 
        (! SC_SPECIFIED( scp, A_SOCKET_TYPE ) ) )
   {
      if( (SC_PROTONAME(scp) != NULL) && EQ("tcp", SC_PROTONAME(scp)) )
      {
            SC_SOCKET_TYPE(scp) = SOCK_STREAM;
            SC_SPECIFY(scp, A_SOCKET_TYPE);
      }

      if( (SC_PROTONAME(scp) != NULL) && EQ("udp", SC_PROTONAME(scp)) )
      {
            SC_SOCKET_TYPE(scp) = SOCK_DGRAM;
            SC_SPECIFY(scp, A_SOCKET_TYPE);
      }
   }

   /*
    * Next assign a port based on service name if not specified. Based
    * on the code immediately before this, if either a socket_type or a
    * protocol is specied, the other gets set appropriately. We will only
    * use protocol for this code.
    */
   if (! SC_SPECIFIED( scp, A_PORT ) && ! SC_IS_MUXCLIENT( scp ) && 
                                        ! SC_IS_RPC( scp )) {
       if ( SC_IS_UNLISTED( scp ) ) {
          msg(LOG_ERR, func, "Unlisted service: %s must have a port entry",
              SC_NAME(scp));
          return(FAILED);
       }
       if ( SC_SPECIFIED( scp, A_PROTOCOL ) ) {
          /*
           * Look up the service based on the protocol and service name.
	   * If not found, don't worry. Message will be emitted in
	   * check_entry().
           */
         struct servent *sep = getservbyname( SC_NAME(scp), 
                                           SC_PROTONAME(scp) ) ;
         if ( sep != NULL ) {
            /* s_port is in network-byte-order */
            SC_PORT(scp) = ntohs(sep->s_port);
            SC_SPECIFY(scp, A_PORT);
         }
         else {
            msg(LOG_ERR, func, 
             "Port not specified and can't find service: %s with getservbyname",
               SC_NAME(scp));
            return(FAILED);
         }
      }
      else {
         msg(LOG_ERR, func, 
             "Port not specified for service: %s and no protocol given", 
             SC_NAME(scp));
         return(FAILED);
      }
   }
   
   if ( USE_DEFAULT( scp, def, A_LOG_ON_SUCCESS ) )
   {
      SC_LOG_ON_SUCCESS(scp) = SC_LOG_ON_SUCCESS(def) ;
      SC_SPECIFY( scp, A_LOG_ON_SUCCESS ) ;
   }

   if ( USE_DEFAULT( scp, def, A_LOG_ON_FAILURE ) )
   {
      SC_LOG_ON_FAILURE(scp) = SC_LOG_ON_FAILURE(def) ;
      SC_SPECIFY( scp, A_LOG_ON_FAILURE ) ;
   }

   if ( USE_DEFAULT( scp, def, A_LOG_TYPE ) )
   {
      struct log *dlp = SC_LOG( def ) ;
      struct log *slp = SC_LOG( scp ) ;

      switch ( LOG_GET_TYPE( dlp ) )
      {
         case L_NONE:
            LOG_SET_TYPE( slp, L_NONE ) ;
            break ;
         
         case L_SYSLOG:
            *slp = *dlp ;
            break ;
         
         case L_FILE:
            LOG_SET_TYPE( slp, L_COMMON_FILE ) ;
            break ;

         default:
            msg( LOG_ERR, func,
                        "bad log type: %d", (int) LOG_GET_TYPE( dlp ) ) ;
            return( FAILED ) ;
      }
      SC_SPECIFY( scp, A_LOG_TYPE ) ;
   }
   if ( setup_environ( scp, def ) == FAILED )
      return( FAILED ) ;
   return( OK ) ;
}


static void remove_disabled_services( struct configuration *confp )
{
   pset_h disabled_services ;
   pset_h enabled_services ;
   struct service_config *scp ;
   struct service_config *defaults = confp->cnf_defaults ;

   if( SC_SPECIFIED( defaults, A_ENABLED ) ) {
      enabled_services = SC_ENABLED(defaults) ;
      

      /* Mark all the services disabled */
      for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter) ) )
         SC_DISABLE( scp );

      /* Enable the selected services */
      for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter) ) )
      {
         register char *sid = SC_ID( scp ) ;
         register unsigned u ;

         for ( u = 0 ; u < pset_count( enabled_services ) ; u++ ) {
            if ( EQ( sid, (char *) pset_pointer( enabled_services, u ) ) ) {
               SC_ENABLE( scp );
               break;
            }
         }
      }
   }

   /* Remove any services that are left marked disabled */
   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next(iter)) ){
      if( SC_IS_DISABLED( scp ) ) {
         msg(LOG_DEBUG, "remove_disabled_services", "removing %s", SC_NAME(scp));
         SC_DISABLE( scp );
         sc_free(scp);
         psi_remove(iter);
      }
   }

   if ( ! SC_SPECIFIED( defaults, A_DISABLED ) )
      return ;
   
   disabled_services = SC_DISABLED(defaults) ;

   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next( iter ) ) )
   {
      register char *sid = SC_ID( scp ) ;
      register unsigned u ;

      for ( u = 0 ; u < pset_count( disabled_services ) ; u++ )
         if ( EQ( sid, (char *) pset_pointer( disabled_services, u ) ) )
         {
            sc_free( scp ) ;
            psi_remove( iter ) ;
            break ;
         }
   }
}


/*
 * Check if all required attributes have been specified
 */
static status_e service_attr_check( struct service_config *scp )
{
   mask_t         necessary_and_specified ;
   mask_t         necessary_and_missing ;
   mask_t         must_specify = NECESSARY_ATTRS ; /* socket_type & wait */
   unsigned int   attr_id ;
   const char    *attr_name ;
   const char    *func = "service_attr_check" ;

   /*
    * Determine what attributes must be specified
    */
   if ( ! SC_IS_INTERNAL( scp ) ) 
   {  /* user & server */
      M_OR( must_specify, must_specify, NECESSARY_ATTRS_EXTERNAL ) ;
      if ( SC_IS_UNLISTED( scp ) ) 
      {
         if ( ! SC_IS_MUXCLIENT( scp ) ) /* protocol, & port */
         {
            M_OR( must_specify, must_specify, NECESSARY_ATTRS_UNLISTED ) ;
         }
         else  /* Don't need port for TCPMUX CLIENT */
         {
           M_OR( must_specify, must_specify, NECESSARY_ATTRS_UNLISTED_MUX ) ;
         }
      }
   }

   if ( SC_IS_RPC( scp ) ) 
   {
      M_CLEAR( must_specify, A_PORT ); /* port is already known for RPC */
      /* protocol & rpc_version */
      M_OR( must_specify, must_specify, NECESSARY_ATTRS_RPC ) ;
      if ( SC_IS_UNLISTED( scp ) ) /* rpc_number */
         M_OR( must_specify, must_specify, NECESSARY_ATTRS_RPC_UNLISTED ) ;
   }
   else
   {
      if ( SC_SPECIFIED( scp, A_REDIR ) )
         M_CLEAR( must_specify, A_SERVER ); /* server isn't used */
   }

   if( SC_IPV4( scp ) && SC_IPV6( scp ) ) {
      msg( LOG_ERR, func, 
         "Service %s specified as both IPv4 and IPv6 - DISABLING", 
	 SC_NAME(scp));
      return FAILED ;
   }
   
   /*
    * Check if all necessary attributes have been specified
    *
    * NOTE: None of the necessary attributes can belong to "defaults"
    *         This is why we use the sc_attributes_specified mask instead
    *         of the sc_attributes_present mask.
    */

   M_AND( necessary_and_specified,
                  scp->sc_specified_attributes, must_specify ) ;
   M_XOR( necessary_and_missing, necessary_and_specified, must_specify ) ;

   if ( M_ARE_ALL_CLEAR( necessary_and_missing) )
      return OK ;

   /*
    * Print names of missing attributes
    */
   for ( attr_id = 0 ; attr_id < SERVICE_ATTRIBUTES ; attr_id++ )
      if ( M_IS_SET( necessary_and_missing, attr_id ) && 
                  ( attr_name = attr_name_lookup( attr_id ) ) != NULL )
      {
         msg( LOG_ERR, func,
            "Service %s missing attribute %s - DISABLING", 
	    SC_ID(scp), attr_name ) ;
      }
   return FAILED ;
}


/*
 * Perform validity checks on the whole entry. At this point, all
 * attributes have been read and we can do an integrated check that
 * all parameters make sense.
 *
 * Also does the following:
 *      1. If this is an internal service, it finds the function that
 *         implements it
 *      2. For RPC services, it finds the program number
 *      3. For non-RPC services, it finds the port number.
 */
static status_e check_entry( struct service_config *scp, 
                             const struct configuration *confp )
{
   const char *func = "check_entry" ;
   unsigned int u;
   const pset_h sconfs = CNF_SERVICE_CONFS( confp ) ;

   /*
    * Make sure the service id is unique
    */
   for ( u = 0 ; u < pset_count( sconfs ) ; u++ ) 
   {
      int diff = 1;
      const struct service_config *tmp_scp = SCP( pset_pointer( sconfs, u ) );
      if (tmp_scp == scp)
         break; /* Don't check ourselves, or anything after us */
      if ( EQ( SC_ID(tmp_scp), SC_ID(scp) ) )
      { 
         diff = 0;
      }
      if( SC_BIND_ADDR(tmp_scp) == NULL)
         continue; /* problem entry, skip it */
      if ( (SC_PORT(scp) != SC_PORT(tmp_scp)) || 
           (SC_PROTOVAL(scp) != SC_PROTOVAL(tmp_scp)) )
         continue; /* if port or protocol are different, its OK */
      if (SC_BIND_ADDR(scp) != NULL)
      {
         if (SC_BIND_ADDR(scp)->sa.sa_family != 
             SC_BIND_ADDR(tmp_scp)->sa.sa_family)
            continue;
         if (SC_BIND_ADDR(scp)->sa.sa_family == AF_INET)
         {
            if (memcmp(&SC_BIND_ADDR(scp)->sa_in.sin_addr, 
                       &SC_BIND_ADDR(tmp_scp)->sa_in.sin_addr, 
                       sizeof(struct in_addr) ) )
               continue;
         }
         else /* We assume that all bad address families are weeded out */
         {
            if (memcmp(&SC_BIND_ADDR(scp)->sa_in6.sin6_addr, 
                       &SC_BIND_ADDR(tmp_scp)->sa_in6.sin6_addr, 
                       sizeof(struct in6_addr) ) )
               continue;
         }
      } 
      if( SC_IS_DISABLED( tmp_scp ) ||
               SC_IS_DISABLED(scp) ) 
      {
         /* 
          * Allow multiple configs, as long as all but one are
          * disabled.
          */
         continue;
      }
#if defined(HAVE_RPC_RPCENT_H) || defined(HAVE_NETDB_H)
      if ( SC_IS_RPC( scp ) && SC_IS_RPC ( tmp_scp ) )
      {
         const struct rpc_data *rdp1 = SC_RPCDATA( scp ) ;
         const struct rpc_data *rdp2 = SC_RPCDATA( tmp_scp ) ;
         if ( rdp1->rd_program_number != rdp2->rd_program_number )
           continue;
        if ( rdp1->rd_min_version > rdp2->rd_max_version ||
             rdp1->rd_max_version < rdp2->rd_min_version )
          continue;
      }
#endif
      if (diff) 
         msg( LOG_ERR, func, 
         "service: %s id: %s is unique but its identical to "
		"service: %s id: %s - DISABLING",
           SC_NAME(scp), SC_ID(scp), SC_NAME(tmp_scp), SC_ID(tmp_scp) ) ;
      else
         msg( LOG_ERR, func, 
           "service: %s id: %s not unique or is a duplicate - DISABLING",
           SC_NAME(scp), SC_ID(scp) ) ;
      return FAILED ;
   } /* for */
   
   /*
    * Currently, we cannot intercept:
    *      1) internal services
    *      2) multi-threaded services
    * We clear the INTERCEPT flag without disabling the service.
    */
   if ( SC_IS_INTERCEPTED( scp ) )
   {
      if ( SC_IS_INTERNAL( scp ) )
      {
         msg( LOG_ERR, func,
            "Internal services cannot be intercepted: %s ", SC_ID(scp) ) ;
         M_CLEAR( SC_XFLAGS(scp), SF_INTERCEPT ) ;
      }
      if ( SC_WAIT(scp) == NO )
      {
         msg( LOG_ERR, func,
            "Multi-threaded services cannot be intercepted: %s", SC_ID(scp) ) ;
         M_CLEAR( SC_XFLAGS(scp), SF_INTERCEPT ) ;
      }
   }
   
   /* Steer the lost sheep home */
   if ( SC_SENSOR( scp ) )
      M_SET( SC_TYPE(scp), ST_INTERNAL );

   if ( SC_IS_INTERNAL( scp ) )
   {   /* If SENSOR flagged redirect to internal builtin function. */ 
      if ( SC_SENSOR( scp ) )
      {
	 init_sensor();
         SC_BUILTIN(scp) =
            builtin_find( "sensor", SC_SOCKET_TYPE(scp) );
      }
      else
         SC_BUILTIN(scp) =
            builtin_find( SC_NAME(scp), SC_SOCKET_TYPE(scp) );
      if (SC_BUILTIN(scp) == NULL )
         return( FAILED ) ;
   }

#ifdef LABELED_NET
      if (SC_LABELED_NET(scp)) {
         if ( SC_IS_INTERNAL( scp ) ) {
            msg( LOG_ERR, func,
               "Internal services cannot support labeled networking: %s",
               SC_ID(scp) ) ;
            return( FAILED ) ;
         }
         if ( SC_SOCKET_TYPE(scp) != SOCK_STREAM ) {
            msg( LOG_ERR, func,
               "Non-stream socket types cannot support labeled networking: %s",
               SC_ID(scp) ) ;
            return( FAILED ) ;
         }
         if ( SC_WAITS( scp ) ) {
            msg( LOG_ERR, func,
               "Tcp wait services cannot support labeled networking: %s",
               SC_ID(scp) ) ;
            return( FAILED ) ;
         }
         if ( SC_REDIR_ADDR( scp ) != NULL) {
            msg( LOG_ERR, func,
               "Redirected services cannot support labeled networking: %s",
               SC_ID(scp) ) ;
            return( FAILED ) ;
         }
      }
#endif

   if ( SC_IS_MUXCLIENT( scp ) ) 
   {
	   if ( !SC_IS_UNLISTED( scp ) )
	   {
               msg(LOG_ERR, func, 
                   "Service: %s (tcpmux) should have UNLISTED in type.",
		   SC_NAME(scp));
	       return( FAILED );
	   }
	   
	   if (!EQ("tcp", SC_PROTONAME(scp)))
	   {
               msg(LOG_ERR, func, 
                   "Service: %s (tcpmux) should have tcp in protocol.",
		   SC_NAME(scp));
	       return( FAILED );
	   }
   }

/* #ifndef NO_RPC */
#if defined(HAVE_RPC_RPCENT_H) || defined(HAVE_NETDB_H)
   if ( SC_IS_RPC( scp ) && !SC_IS_UNLISTED( scp ) )
   {
      struct rpcent *rep = (struct rpcent *)getrpcbyname( SC_NAME(scp) ) ;

      if ( rep == NULL )
      {
         msg( LOG_ERR, func, "unknown RPC service: %s", SC_NAME(scp) ) ;
         return( FAILED ) ;
      }
      SC_RPCDATA( scp )->rd_program_number = rep->r_number ;
   }
   else
#endif   /* ! NO_RPC */
   {
       if ( !SC_IS_UNLISTED( scp ) ) 
       { 
          uint16_t service_port ;
          struct servent *sep ;
  
          /*
           * Check if a protocol was specified. Based on the code in 
	   * service_fill, if either socket_type or protocol is specified,
	   * the other one is filled in. Protocol should therefore always
	   * be filled in unless they made a mistake. Then verify it is the
	   * proper protocol for the given service. 
           * We don't need to check MUXCLIENTs - they aren't in /etc/services.
           */
          if ( SC_SPECIFIED( scp, A_PROTOCOL ) )
          {
             sep = getservbyname( SC_NAME(scp), SC_PROTONAME(scp) ) ;
             if ( (sep == NULL) )
             {
                msg( LOG_ERR, func, 
                   "service/protocol combination not in /etc/services: %s/%s",
                   SC_NAME(scp), SC_PROTONAME(scp) ) ;
                return( FAILED ) ;
             }
          }
          else
          {
             msg( LOG_ERR, func,
               "A protocol or a socket_type must be specified for service: %s.",
                SC_NAME(scp) ) ;
             return( FAILED ) ;
          }
 
          /* s_port is in network-byte-order */
          service_port = ntohs(sep->s_port);
 
          /*
           * If a port was specified, it must be the right one
           */
          if ( SC_SPECIFIED( scp, A_PORT ) && 
               SC_PORT(scp) != service_port )
          {
             msg( LOG_ERR, func, "Service %s expects port %d, not %d",
                  SC_NAME(scp), service_port, SC_PORT(scp) ) ;
             return( FAILED ) ;
          }
      } /* if not unlisted */
    }
    if ( SC_SPECIFIED( scp, A_REDIR ))
    {
       if ( SC_SOCKET_TYPE( scp ) != SOCK_STREAM )
       {
          msg( LOG_ERR, func, 
 	      "Only tcp sockets are supported for redirected service %s",
 	      SC_NAME(scp));
          return FAILED;
       }
       if ( SC_WAITS( scp ) )
       {
          msg( LOG_ERR, func, 
 	      "Redirected service %s must not wait", SC_NAME(scp));
          return FAILED;
       }
       if ( SC_NAMEINARGS( scp ) )
       {
          msg( LOG_ERR, func, 
 	      "Redirected service %s should not have NAMEINARGS flag set", 
	      SC_NAME(scp));
          return FAILED;
       }
    }
    else /* Not a redirected service */
    {
       if( M_IS_SET( SC_LOG_ON_SUCCESS(scp), LO_TRAFFIC ) )
       {
          msg( LOG_ERR, func,
             "Service %s should not have TRAFFIC flag set since its"
             " not redirected", SC_NAME(scp));
          return FAILED;
       }
    }
    
   if ( SC_NAMEINARGS(scp) )
   {
      if (SC_IS_INTERNAL( scp ) )
      {
         msg( LOG_ERR, func, 
              "Service %s is INTERNAL and has NAMEINARGS flag set", 
	      SC_NAME(scp) );
         return FAILED;
      }
      else if (!SC_SPECIFIED( scp, A_SERVER_ARGS) )
      {
         msg( LOG_ERR, func, 
              "Service %s has NAMEINARGS flag set and no server_args", 
	      SC_NAME(scp) );
         return FAILED;
      }
   }

   if ( service_attr_check( scp ) == FAILED )
      return( FAILED ) ;

   return( OK ) ;
}

/*
 * Get a configuration from the specified file.
 */
static status_e get_conf( int fd, struct configuration *confp )
{
   parse_conf_file( fd, confp, ps.ros.config_file ) ;
   parse_end() ;
   return( OK ) ;
}


#define CHECK_AND_CLEAR( scp, mask, mask_name )                               \
   if ( M_IS_SET( mask, LO_USERID ) )                                         \
   {                                                                          \
      msg( LOG_WARNING, func,                                                 \
      "%s service: clearing USERID option from %s", SC_ID(scp), mask_name ) ; \
      M_CLEAR( mask, LO_USERID ) ;                                            \
   }

/*
 * Get a configuration by reading the configuration file.
 */
status_e cnf_get( struct configuration *confp )
{
   int config_fd ;
   struct service_config *scp ;
   const char *func = "get_configuration" ;

   if ( cnf_init( confp, &config_fd, &iter ) == FAILED )
      return( FAILED ) ;

   else if ( get_conf( config_fd, confp ) == FAILED )
   {
      Sclose( config_fd ) ;
      cnf_free( confp ) ;
      psi_destroy( iter ) ;
      return( FAILED ) ;
   }

   /* get_conf eventually calls Srdline, try Sclosing to unmmap memory. */
   Sclose( config_fd );
   if( inetd_compat ) {
      current_file = "/etc/inetd.conf";
      config_fd = open(current_file, O_RDONLY);
      if( config_fd >= 0 ) {
         parse_inet_conf_file( config_fd, confp );
         parse_end() ;
         /*
	  * parse_inet_conf eventually calls Srdline, try Sclosing to 
	  * unmmap memory. 
	  */
         Sclose(config_fd);
      }
   }

   remove_disabled_services( confp ) ;

   for ( scp = SCP( psi_start( iter ) ) ; scp ; scp = SCP( psi_next( iter ) ) )
   {
      /*
       * Fill the service configuration from the defaults.
       * We do this so that we don't have to look at the defaults any more.
       */
      if ( service_fill( scp, confp->cnf_defaults ) == FAILED )
      {
         sc_free( scp ) ;
         psi_remove( iter ) ;
         continue ;
      }

      if ( check_entry( scp, confp ) == FAILED )
      {
         sc_free( scp ) ;
         psi_remove( iter ) ;
         continue ;
      }

      /*
       * If the INTERCEPT flag is set, change this service to an internal 
       * service using the special INTERCEPT builtin.
       */
      if ( SC_IS_INTERCEPTED( scp ) )
      {
         const builtin_s *bp ;

         bp = spec_find( INTERCEPT_SERVICE_NAME, SC_SOCKET_TYPE(scp) ) ;
         if ( bp == NULL )
         {
            msg( LOG_ERR, func, "removing service %s", SC_ID( scp ) ) ;
            sc_free( scp ) ;
            psi_remove( iter ) ;
            continue ;
         }

         SC_BUILTIN(scp) = bp ;
         M_SET( SC_TYPE(scp), ST_INTERNAL ) ;
      }

      /*
       * Clear the USERID flag for the identity service because
       * it may lead to loops (for example, remote xinetd issues request,
       * local xinetd issues request to remote xinetd etc.)
       * We identify the identity service by its (protocol,port) combination.
       */
      if ( SC_PORT(scp) == IDENTITY_SERVICE_PORT && 
                                       SC_PROTOVAL(scp) == IPPROTO_TCP )
      {
         CHECK_AND_CLEAR( scp, SC_LOG_ON_SUCCESS(scp), "log_on_success" ) ;
         CHECK_AND_CLEAR( scp, SC_LOG_ON_FAILURE(scp), "log_on_failure" ) ;
      }
   }

   psi_destroy( iter ) ;

   if ( debug.on && debug.fd != -1 )
      cnf_dump( confp, debug.fd ) ;

   endservent() ;
   endprotoent() ;
#ifndef NO_RPC
   endrpcent() ;
#endif
   return( OK ) ;
}

