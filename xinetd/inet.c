/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#include "config.h"
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <memory.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <limits.h>

#include "str.h"
#include "inet.h"
#include "msg.h"
#include "parse.h"
#include "parsesup.h"
#include "nvlists.h"

static int get_next_inet_entry( int fd, pset_h sconfs, 
                          struct service_config *defaults);

void parse_inet_conf_file( int fd, struct configuration *confp )
{
   pset_h sconfs                         = CNF_SERVICE_CONFS( confp );
   struct service_config *default_config = CNF_DEFAULTS( confp );
   
   line_count = 0;

   for( ;; )
   {   
      if (get_next_inet_entry(fd, sconfs, default_config) == -2)
         break;
   }
}

static int get_next_inet_entry( int fd, pset_h sconfs, 
                          struct service_config *defaults)
{
   char *p;
   str_h strp;
   char *line = next_line(fd);
   struct service_config *scp;
   unsigned u, i;
   const char *func = "get_next_inet_entry";
   char *name = NULL, *rpcvers = NULL, *rpcproto = NULL;
   char *group, *proto, *stype;
   const struct name_value *nvp;
   struct protoent *pep ;
   struct passwd *pw ;
   struct group *grp ;
   const char *dot = ".";
   const char *slash = "/";
   pset_h args;
   
   if( line == CHAR_NULL )
      return -2;

   strp = str_parse( line, " \t", STR_RETURN_ERROR, INT_NULL ) ;
   if( strp == NULL )
   {
      parsemsg( LOG_CRIT, func, "inetd.conf - str_parse failed" ) ;
      return( -1 ) ;
   }

   if( (args = pset_create(10,10)) == NULL )
   {
      out_of_memory(func);
      return -1;
   }

   /* Break the line into components, based on spaces */
   while( (p = str_component( strp )) )
   {
      if( pset_add(args, p) == NULL )
      {
         parsemsg( LOG_CRIT, func, ES_NOMEM );
         pset_destroy(args);
         return -1;
      }
   }
   str_endparse( strp );

   /* get the service name */
   name = new_string((char *)pset_pointer( args, 0 ));
   if( name == NULL ) {
      parsemsg( LOG_ERR, func, "inetd.conf - Invalid service name" );
      pset_destroy(args);
      return -1;
   }

   /* Check to find the '/' for specifying RPC version numbers */
   if( (rpcvers = strstr(name, slash)) != NULL ) {
      *rpcvers = '\0';
      rpcvers++;
   }

   scp = sc_alloc( name );
   if( scp == NULL )
   {
      pset_destroy(args);
      free( name );
      return -1;
   }
   /*
    * sc_alloc makes its own copy of name. At this point, sc_alloc worked
    * so we will free our copy to avoid leaks.
    */
   free( name );

   /* Replicate inetd behavior in this regard.  Also makes sure the
    * service actually works on system where setgroups(0,NULL) doesn't
    * work.
    */
   SC_GROUPS(scp) = YES;
   SC_SPECIFY( scp, A_GROUPS );

   /* Get the socket type (stream dgram) */
   stype = (char *)pset_pointer(args, 1);
   if( stype == NULL ) {
      parsemsg( LOG_ERR, func, "inetd.conf - Invalid socket type" );
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }
   nvp = nv_find_value( socket_types, stype );
   if( nvp == NULL )
   {
      parsemsg( LOG_ERR, func, "inetd.conf - Bad socket type: %s", p);
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }

   SC_SOCKET_TYPE(scp) = nvp->value;

   /* Get the protocol type */
   proto = (char *)pset_pointer(args,2);
   if( strstr(proto, "rpc") != NULL )
   {
      int rpcmin, rpcmax;
      struct rpc_data *rdp = SC_RPCDATA( scp ) ;

      if( rpcvers == NULL ) {
         pset_destroy(args);
         sc_free(scp);
         return -1;
         /* uh oh */
      }

      p = strchr(rpcvers, '-');
      if( p && parse_int(rpcvers, 10, '-', &rpcmin) == 0 ) {
         if( parse_base10(p + 1, &rpcmax) || rpcmin > rpcmax ) {
            pset_destroy(args);
            sc_free(scp);
            return -1;
         }
      } else {
         if( parse_base10(rpcvers, &rpcmin) ) {
            pset_destroy(args);
            sc_free(scp);
            return -1;
         }

         rpcmax = rpcmin;
      }

      /* now have min and max rpc versions */
      rdp->rd_min_version = rpcmin;      
      rdp->rd_max_version = rpcmax;      

      rpcproto = strstr(proto, slash);
      if( rpcproto == NULL ) {
         parsemsg( LOG_ERR, func, "inetd.conf - bad rpc version numbers" );
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
      *rpcproto = '\0';
      rpcproto++;
      proto = rpcproto;

      /* Set the RPC type field */
      nvp = nv_find_value( service_types, "RPC" );
      if ( nvp == NULL )
      {
         parsemsg( LOG_WARNING, func, "inetd.conf - Bad foo %s", name ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }

      M_SET(SC_TYPE(scp), nvp->value);
   }
   if ( ( pep = getprotobyname( proto ) ) == NULL )
   {
      parsemsg( LOG_ERR, func, "inetd.conf - Protocol %s not in /etc/protocols",
	        proto ) ;
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }

   SC_PROTONAME(scp) = new_string( proto ) ;
   if ( SC_PROTONAME(scp) == NULL )
   {
      out_of_memory( func ) ;
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }
   SC_PROTOVAL(scp) = pep->p_proto;
   SC_SPECIFY(scp, A_PROTOCOL);

   /* Get the wait attribute */
   p = (char *)pset_pointer(args, 3);
   if ( p == NULL ) {
      parsemsg( LOG_ERR, func, "inetd.conf - No value specified for wait" );
      sc_free(scp);
      return -1;
   }
   if ( EQ( p, "wait" ) )
      SC_WAIT(scp) = YES ;
   else if ( EQ( p, "nowait" ) )
      SC_WAIT(scp) = NO ;
   else
      parsemsg( LOG_ERR, func, "inetd.conf - Bad value for wait: %s", p ) ;

   /* Get the user to run as */
   p = (char *)pset_pointer(args, 4);
   if ( p == NULL ) {
      parsemsg( LOG_ERR, func, "inetd.conf - No value specified for user" );
      sc_free(scp);
      return -1;
   }
   if( (group = strstr(p, dot)) )
   {
      *group = '\0';
      group++;
   
      grp = (struct group *)getgrnam( (char *)group ) ;
      if ( grp == NULL )
      {
         parsemsg( LOG_ERR, func, "inetd.conf - Unknown group: %s", group ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }   

      SC_GID(scp) = ((struct group *)grp)->gr_gid;
      SC_SPECIFY( scp, A_GROUP );
   }

   pw = getpwnam( p );
   if ( pw == NULL )
   {
      parsemsg( LOG_ERR, func, "inetd.conf - Unknown user: %s", p ) ;
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }
   str_fill( pw->pw_passwd, ' ' );
   SC_UID(scp) = pw->pw_uid;
   SC_USER_GID(scp) = pw->pw_gid;

   /* Get server name, or flag as internal */
   p = (char *)pset_pointer(args, 5);
   if ( p == NULL ) {
      parsemsg( LOG_ERR, func, "inetd.conf - No value specified for user" );
      sc_free(scp);
      return -1;
   }
   if( EQ( p, "internal" ) ) 
   {
      nvp = nv_find_value( service_types, "INTERNAL" );
      if ( nvp == NULL )
      {
         parsemsg( LOG_WARNING, func, "inetd.conf - Bad foo %s", name ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }

      M_SET(SC_TYPE(scp), nvp->value);

      if( EQ( SC_NAME(scp), "time" ) ) {
         if( EQ( proto, "stream" ) )
            SC_ID(scp) = new_string("time-stream");
         else
            SC_ID(scp) = new_string("time-dgram");
      }

      if( EQ( SC_NAME(scp), "daytime" ) ) {
         if( EQ( proto, "stream" ) )
            SC_ID(scp) = new_string("daytime-stream");
         else
            SC_ID(scp) = new_string("daytime-dgram");
      }

      if( EQ( SC_NAME(scp), "chargen" ) ) {
         if( EQ( proto, "stream" ) )
            SC_ID(scp) = new_string("chargen-stream");
         else
            SC_ID(scp) = new_string("chargen-dgram");
      }

      if( EQ( SC_NAME(scp), "echo" ) ) {
         if( EQ( proto, "stream" ) )
            SC_ID(scp) = new_string("echo-stream");
         else
            SC_ID(scp) = new_string("echo-dgram");
      }

      if( EQ( SC_NAME(scp), "discard" ) ) 
      {
         parsemsg(LOG_WARNING, func, 
		  "inetd.conf - service discard not supported");
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
   }
   else
   {
      SC_SERVER(scp) = new_string( p );
      if ( SC_SERVER(scp) == NULL )
      {
         out_of_memory( func ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
      SC_SPECIFY( scp, A_SERVER);

      /* Get argv */ 
      SC_SERVER_ARGV(scp) = (char **)argv_alloc(pset_count(args)+1);

      for( u = 0; u < pset_count(args)-6 ; u++ )
      {
         p = new_string((char *)pset_pointer(args, u+6));
         if( p == NULL )
         {
            for ( i = 1 ; i < u ; i++ )
               free( SC_SERVER_ARGV(scp)[i] );
            free( SC_SERVER_ARGV(scp) );
            pset_destroy(args);
            sc_free(scp);
            return -1;
         }
         SC_SERVER_ARGV(scp)[u] = p;
      }
      /* Set the reuse flag, as this is the default for inetd */
      nvp = nv_find_value( service_flags, "REUSE" );
      if ( nvp == NULL )
      {
         parsemsg( LOG_WARNING, func, "inetd.conf - Bad foo %s", name ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
      M_SET(SC_XFLAGS(scp), nvp->value);

      /* Set the NOLIBWRAP flag, since inetd doesn't have libwrap built in */
      nvp = nv_find_value( service_flags, "NOLIBWRAP" );
      if ( nvp == NULL )
      {
         parsemsg( LOG_WARNING, func, "inetd.conf - Bad foo %s", name ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
      M_SET(SC_XFLAGS(scp), nvp->value);
   
      /* Set the NAMEINARGS flag, as that's the default for inetd */
      nvp = nv_find_value( service_flags, "NAMEINARGS" );
      if ( nvp == NULL )
      {
         parsemsg( LOG_WARNING, func, "inetd.conf - Bad foo %s", name ) ;
         pset_destroy(args);
         sc_free(scp);
         return (-1);
      }
      M_SET(SC_XFLAGS(scp), nvp->value);
      SC_SPECIFY( scp, A_SERVER_ARGS );

      if ( (SC_ID(scp) = new_string( SC_NAME(scp) )) )
         SC_PRESENT( scp, A_ID ) ;
      else
      {
         out_of_memory( func ) ;
         pset_destroy(args);
         sc_free(scp);
         return -1;
      }
   }
   
   SC_SPECIFY( scp, A_PROTOCOL );
   SC_SPECIFY( scp, A_USER );
   SC_SPECIFY( scp, A_SOCKET_TYPE );
   SC_SPECIFY( scp, A_WAIT );

   if( ! pset_add(sconfs, scp) )
   {
      out_of_memory( func );
      pset_destroy(args);
      sc_free(scp);
      return -1;
   }

   pset_destroy(args);
   parsemsg( LOG_DEBUG, func, "added service %s", SC_NAME(scp));
   return 0;
}

