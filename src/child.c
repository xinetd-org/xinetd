/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <grp.h>
#include <netdb.h>
#ifdef LABELED_NET
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#include "str.h"
#include "child.h"
#include "sconf.h"
#include "msg.h"
#include "main.h"
#include "xconfig.h"
#include "ident.h"
#include "sconst.h"
#include "signals.h"
#include "options.h"
#include "redirect.h"

/* Local declarations */
#ifdef LABELED_NET
static int set_context_from_socket( const struct service_config *scp, int fd );
#endif


/*
 * This function is running in the new process
 */
void exec_server( const struct server *serp )
{
   const struct service_config *scp = SVC_CONF( SERVER_SERVICE( serp ) ) ;
   struct rlimit rl ;
   int fd ;
   int descriptor = SERVER_FD( serp ) ;
   const char *server = SC_SERVER( scp ) ;
   const char *func = "exec_server" ;

   /*
    * The following code solves a problem with post-version-4.3
    * Ultrix systems (the bug was reported, and a fix was provided by
    * doug@seas.smu.edu; a slightly modified version of this
    * fix is included here).
    *
    * If this is a 'nowait' service, we pass the service descriptor
    * to the server. Note that we have set the close-on-exec flag
    * on all service descriptors. It is unclear whether the dup2()
    * will create a descriptor with the close-on-exec flag set,
    * so we explicitly clear the flag (since we are doing this
    * after the fork, it does not affect the descriptor of the
    * parent process).
    */
   if ( fcntl( descriptor, F_SETFD, 0 ) == -1 )
      msg( LOG_WARNING, func,
         "fcntl( %d, clear close-on-exec ) failed: %m", descriptor ) ;

   if ( debug.on )
      msg( LOG_DEBUG, func, "duping %d", descriptor ) ;

   /*
    * If server_loguser flag is on, then syslog may have opened fd 0, 1, or
    * 2. We call msg_suspend() now so that the logging system doesn't use 
    * the dup'ed descriptor.
    */

   msg_suspend() ;

   for ( fd = 0 ; fd <= MAX_PASS_FD ; fd++ )
   {
      if ( dup2( descriptor, fd ) == -1 )
      {
   	 msg_resume();
         msg( LOG_ERR, func,
               "dup2( %d, %d ) failed: %m", descriptor, fd ) ;
         _exit( 1 ) ;
      }
   }


#ifdef RLIMIT_NOFILE
   if ( SC_RLIM_FILES( scp ))
   {
      ps.ros.max_descriptors = SC_RLIM_FILES( scp );
   }
   rl.rlim_max = rl.rlim_cur = ps.ros.max_descriptors ;
   (void) setrlimit( RLIMIT_NOFILE, &rl ) ;
#endif
#ifdef RLIMIT_AS
   if (SC_RLIM_AS (scp))
   {
      rl.rlim_cur = SC_RLIM_AS( scp );
      rl.rlim_max = SC_RLIM_AS( scp );
      (void) setrlimit( RLIMIT_AS, &rl );
   }
#endif
#ifdef RLIMIT_CPU
   if (SC_RLIM_CPU (scp))
   {
      rl.rlim_cur = SC_RLIM_CPU( scp );
      rl.rlim_max = SC_RLIM_CPU( scp );
      (void) setrlimit( RLIMIT_CPU, &rl );
   }
#endif
#ifdef RLIMIT_DATA
   if (SC_RLIM_DATA (scp))
   {
      rl.rlim_cur = SC_RLIM_DATA( scp );
      rl.rlim_max = SC_RLIM_DATA( scp );
      (void) setrlimit( RLIMIT_DATA, &rl );
   }
#endif
#ifdef RLIMIT_RSS
   if (SC_RLIM_RSS (scp))
   {
      rl.rlim_cur = SC_RLIM_RSS( scp );
      rl.rlim_max = SC_RLIM_RSS( scp );
      (void) setrlimit( RLIMIT_RSS, &rl );
   }
#endif
#ifdef RLIMIT_STACK
   if (SC_RLIM_STACK (scp))
   {
      rl.rlim_cur = SC_RLIM_STACK( scp );
      rl.rlim_max = SC_RLIM_STACK( scp );
      (void) setrlimit( RLIMIT_STACK, &rl );
   }
#endif

   /*
      Set the context if the option was given
   */
#ifdef LABELED_NET
   if (SC_LABELED_NET(scp))
   {
      if (set_context_from_socket( scp, descriptor ) < 0) {
         msg( LOG_ERR, func,
             "Changing process context failed for %s", SC_ID( scp )) ;
         _exit( 1 ) ;
      }
   }
#endif

   (void) Sclose( descriptor ) ;

#ifndef solaris
   no_control_tty() ;
#endif

   (void) execve( server, SC_SERVER_ARGV( scp ),
             env_getvars( SC_ENV( scp )->env_handle ) ) ;

   /*
    * The exec failed. Log the error and exit.
    */
   msg_resume() ;
   msg( LOG_ERR, func, "execv( %s ) failed: %m", server ) ;
   _exit( 0 ) ;
}


/*
 * Rename this process by changing the ps.ros.Argv vector
 * Try to put the name of the service in ps.ros.Argv[0], Argv[1]
 * until either the service name is exhausted or we run out
 * of ps.ros.Argv's. 
 * The rest of ps.ros.Argv is cleared to spaces
 */
static void rename_process( const char *name )
{
   const char *from = name ;
   char *to = ps.ros.Argv[ 0 ] ;
   int tmp_index = 1 ;

   while ( *from != NUL )
   {
      if ( *to != NUL )
         *to++ = *from++ ;
      else
         if ( tmp_index < ps.ros.Argc )
            to = ps.ros.Argv[ tmp_index++ ] ;
         else
            break ;
   }
   str_fill( to, ' ' ) ;
   while ( tmp_index < ps.ros.Argc )
      str_fill( ps.ros.Argv[ tmp_index++ ], ' ' ) ;
}


static void set_credentials( const struct service_config *scp )
{
   const char *func = "set_credentials" ;

   if ( SC_SPECIFIED( scp, A_GROUP ) || SC_SPECIFIED( scp, A_USER ) ) {
      if ( ps.ros.is_superuser )
      {
         gid_t gid = SC_GETGID( scp ) ;

         if ( setgid( gid ) == -1 )
         {
            msg( LOG_ERR, func, "setgid failed: %m" ) ;
            _exit( 1 ) ;
         }

#ifndef NO_INITGROUPS
         /*
          * Bug discovered by maf+@osu.edu (a bug fix was also provided;
          * a slightly modified version is included here):
          *      initgroups was not being invoked to set the remaining
          *      groups appropriately
          */
         /* Solar Designer's groups fix */
         if ( SC_SPECIFIED( scp, A_USER ) && SC_SPECIFIED( scp, A_GROUPS ) &&
            SC_GROUPS(scp) == YES )
         {
            struct passwd *pwd ;

            /*
             * Invoke getpwuid() to get the user's name.
             *
             * XXX:   we should not need to invoke getpwuid(); we should
             *         remember the user name in the configuration file.
             */
            if ( ( pwd = getpwuid( SC_UID( scp ) ) ) == NULL )
            {
               msg( LOG_ERR, func, "getpwuid( %d ) (service=%s) failed: %m",
                  SC_UID( scp ), SC_ID( scp ) ) ;
               _exit( 1 ) ;
            }
            str_fill( pwd->pw_passwd, ' ' );

            if ( initgroups( pwd->pw_name, pwd->pw_gid ) == -1 )
            {
               msg( LOG_ERR, func, "initgroups( %s, %d ) failed: %m",
                  pwd->pw_name, pwd->pw_gid ) ;
               _exit( 1 ) ;
            }
         }
         else
         {
            if ( setgroups( 0, NULL ) )
            {
               msg( LOG_ERR, func, "setgroups( 0, NULL ) failed: %m" ) ;
               msg( LOG_ERR, func, "Your system may require that 'groups = yes' be defined for this service: %s", SC_NAME(scp));
               _exit( 1 ) ;
            }
         }
#endif   /* ! NO_INITGROUPS */
      }
   }

   if ( SC_SPECIFIED( scp, A_USER ) ) {
         if ( setuid( SC_UID( scp ) ) == -1 )
         {
            msg( LOG_ERR, func, "setuid failed: %m" ) ;
            _exit( 1 ) ;
         }
      }

   if ( SC_SPECIFIED( scp, A_UMASK ) ) 
      umask(SC_UMASK(scp));
}



/*
 * This function is invoked in a forked process to run a server. 
 * If the service is internal the appropriate function is invoked
 * otherwise the server program is exec'ed.
 * This function also logs the remote user id if appropriate
 */
void child_process( struct server *serp )
{
   struct service          *sp  = SERVER_SERVICE( serp ) ;
   connection_s            *cp  = SERVER_CONNECTION( serp ) ;
   struct service_config   *scp = SVC_CONF( sp ) ;
   const char              *func = "child_process" ;

   signal_default_state();

   if ((signals_pending[0] >= 0 && Sclose(signals_pending[0])) ||
       (signals_pending[1] >= 0 && Sclose(signals_pending[1])))
   {
      msg(LOG_ERR, func, "Failed to close the signal pipe: %m");
      _exit(1);
   }
   signals_pending[0] = -1;
   signals_pending[1] = -1;

#ifdef DEBUG_SERVER
   if ( debug.on )
   {
      msg( LOG_DEBUG, func, "Process %d is sleeping", getpid() ) ;
      sleep( 10 ) ;
   }
#endif

   if ( ! SC_IS_INTERCEPTED( scp ) )
   {
      set_credentials( scp ) ;
      if ( SC_SPECIFIED( scp, A_NICE ) )
         (void) nice( SC_NICE( scp ) ) ;
   }

   if ( svc_child_access_control(sp, cp) != OK )
      exit(0);

   if ( SERVER_LOGUSER( serp ) )
   {
      unsigned   timeout ;
      idresult_e result ;
      
      /*
       * We use LOGUSER_SUCCESS_TIMEOUT unless the service requires
       * identification, in which case we use an infinite timeout
       */
      timeout = SC_MUST_IDENTIFY( scp ) ? 0 : LOGUSER_SUCCESS_TIMEOUT ;
      result = log_remote_user( serp, timeout ) ;

      if ( result != IDR_OK && SC_MUST_IDENTIFY( scp ) )
      {
         svc_logprint( sp, NOID_ENTRY, "%s %s",
                  conn_addrstr( SERVER_CONNECTION( serp ) ),
                     idresult_explain( result ) ) ;
         _exit( 0 ) ;
      }
   }


   /* this is where the server gets executed  -bbraun */
   if ( ! SC_IS_INTERNAL( scp ) )
   {
      if( SC_REDIR_ADDR(scp) != NULL )
      {
         redir_handler( serp );
      }
      else
      {
         char buff[1024];

         strx_sprint(buff, sizeof(buff)-1, "REMOTE_HOST=%s", conn_addrstr(cp));
         if( env_addstr(SC_ENV(scp)->env_handle, buff) != ENV_OK ) {
            msg( LOG_ERR, func, "Error adding REMOTE_HOST variable for %s: %m", SC_NAME(scp) );
            _exit( 1 ) ;
         }
         exec_server( serp ) ;
      }
   }
   else
   {
      char name[ 180 ] ;
      /*
       * We don't bother to disassociate from the controlling terminal
       *   (we have a controlling terminal only if debug.on is TRUE)
       *
       * Also, for interceptor processes, we give them the name:
       *            <program_name> <service-id> interceptor
       */
      if ( SC_IS_INTERCEPTED( scp ) )
         strx_print( INT_NULL, name, sizeof( name ) - 1,
                           "%s %s interceptor", program_name, SC_ID( scp ) ) ;
      else
      {
         int namelen = sizeof( name ) - 1 ;      /* leave space for the NUL */
         char host[NI_MAXHOST];
         size_t hostlen = NI_MAXHOST;
         socklen_t addrlen = 0;
         union xsockaddr *sinp = CONN_XADDRESS(SERVER_CONNECTION(serp));
         int len;

         if( sinp == NULL )
            exit(0);

         if( SC_IPV6(scp) ) addrlen = sizeof(struct sockaddr_in6);
         else if( SC_IPV4(scp) ) addrlen = sizeof(struct sockaddr_in);

         len = strx_nprint(name, namelen, "(%s service) %s", program_name,
            SC_ID( scp ) ) ;

         if( getnameinfo( SA(sinp), addrlen, host, hostlen, NULL, 0, 0) != 0 )
               strcpy(host, "unknown");

         if ( SC_IPV6(scp) && SC_ACCEPTS_CONNECTIONS( scp ) && 
               !IN6_IS_ADDR_UNSPECIFIED(&sinp->sa_in6.sin6_addr) )
            strx_print( INT_NULL, &name[ len ], namelen - len, " %s" , host ) ;
         if ( SC_IPV4(scp) && SC_ACCEPTS_CONNECTIONS( scp ) )
            strx_print( INT_NULL, &name[ len ], namelen - len, " %s", host ) ;
      }
      rename_process( name ) ;
      SVC_INTERNAL( sp, serp ) ;
   }
   _exit( 0 ) ;
   /* NOTREACHED */
}


/*
 * This function is invoked when a SIGCLD is received
 */
void child_exit(void)
{
   const char *func = "child_exit" ;

   for ( ;; )         /* Find all children that exited */
   {
      int status ;
      pid_t pid ;
      struct server *serp ;
      
      pid = waitpid( -1, &status, WNOHANG ) ;

      if ( debug.on )
         msg( LOG_DEBUG, func, "waitpid returned = %d", pid ) ;
      
      if ( pid == -1 ) {
         if ( errno == EINTR )
            continue ;
         else
            break ;
      }

      if ( pid == 0 )
         break ;
      
      if ( ( serp = server_lookup( pid ) ) != NULL )
      {
         SERVER_EXITSTATUS(serp) = status ;
         server_end( serp ) ;
      }
      else
         msg( LOG_NOTICE, func, "unknown child process %d %s", pid,
            PROC_STOPPED( status ) ? "stopped" : "died" ) ;
   }
}

#ifdef LABELED_NET
static int set_context( security_context_t cntx )
{
   const char *func = "set_context" ;

   int retval = setexeccon(cntx);

   if (debug.on)
   {
      security_context_t current_exec_context;
      if ( getexeccon( &current_exec_context ) == 0 ) {

         msg( LOG_DEBUG, func, 
	   "current security exec context now: %s", 
	   current_exec_context ? current_exec_context : "unknown" );

         freecon( current_exec_context );
      } 
      else
         msg( LOG_DEBUG, func, "Error calling getexeccon: %m" );
   }

   return retval;
}

static int set_context_from_socket( const struct service_config *scp, int fd )
{
   security_context_t curr_context = NULL;
   security_context_t peer_context = NULL;
   security_context_t exec_context = NULL;
   context_t bcon = NULL;
   context_t pcon = NULL;
   security_context_t new_context = NULL;
   security_context_t new_exec_context = NULL;
   int retval = -1;
   const char *exepath = NULL;

   if (getcon(&curr_context) < 0)
     goto fail;
   
   if (getpeercon(fd, &peer_context) < 0)
     goto fail;

   exepath = SC_SERVER_ARGV( scp )[0];
   if (getfilecon(exepath, &exec_context) < 0)
     goto fail;

   if (!(bcon = context_new(curr_context)))
     goto fail;

   if (!(pcon = context_new(peer_context)))
     goto fail;

   if (!context_range_get(pcon))
     goto fail;
   
   if (context_range_set(bcon, context_range_get(pcon)))
     goto fail;

   if (!(new_context = context_str(bcon)))
     goto fail;
   
   if (security_compute_create(new_context, exec_context, string_to_security_class ("process"),
                               &new_exec_context) < 0)
     goto fail;

   retval = set_context(new_exec_context);

   freecon(new_exec_context);

 fail:
   context_free(pcon);
   context_free(bcon);
   freecon(exec_context);   
   freecon(peer_context);
   freecon(curr_context);

   return retval;
}
#endif
