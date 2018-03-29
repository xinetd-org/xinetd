/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
/*
 * The following ifdef is for TIOCNOTTY
 */
#ifndef NO_TERMIOS
#include <sys/termios.h>
#include <termios.h>
#else
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <memory.h>
#include <syslog.h>
#include <errno.h>

#include "sio.h"
#include "str.h"
#include "util.h"
#include "msg.h"

void out_of_memory( const char *func )
{
   msg( LOG_CRIT, func, ES_NOMEM ) ;
}


const struct name_value *nv_find_value( const struct name_value nv_array[], const char *name )
{
   const struct name_value *nvp ;

   for ( nvp = nv_array ; nvp->name ; nvp++ )
   {
      if ( EQ( name, nvp->name ) )
         return( nvp ) ;
   }
   return( NULL ) ;
}


const struct name_value *nv_find_name( const struct name_value nv_array[], int value )
{
   const struct name_value *nvp ;

   for ( nvp = nv_array ; nvp->name ; nvp++ )
   {
      if ( value == nvp->value )
         return( nvp ) ;
   }
   return( NULL ) ;
}


/*
 * A name-value list is exactly what its name says.
 * The functions nv_get_name() and nv_get_value() return a pointer to
 * the entry with the specified value or name respectively.
 *
 * The list ends when an antry with a NULL name is encountered.
 * The value field of that entry is treated in a special manner: if it
 * is non-zero, it is assumed that there exists one more entry whose
 * name field will be returned by the nv_get_name function if it can't
 * find an entry whose value field is equal to its 2nd parameter.
 * If the value field of the NULL entry is 0, then nv_get_name() will
 * return NULL.
 */
const char *nv_get_name( const struct name_value nv_array[], int value )
{
   const struct name_value *nvp ;

   for ( nvp = nv_array ; nvp->name ; nvp++ )
   {
      if ( value == nvp->value )
         return( nvp->name ) ;
   }
   return( nvp->value ? (nvp+1)->name : NULL ) ;
}



char **argv_alloc( unsigned count )
{
   unsigned argv_size = (count + 1) * sizeof( char *) ;
   char **argv ;
   const char *func = "new_argv" ;

   argv = (char **) malloc( argv_size ) ;
   if ( argv == NULL )
   {
      out_of_memory( func ) ;
      return( NULL ) ;
   }
   (void) memset( (char *)argv, 0, argv_size ) ;
   return( argv ) ;
}


/*
 * If size is 0, the pset holds strings
 */
status_e copy_pset( const pset_h from, pset_h *to, unsigned size )
{
   unsigned u ;
   const char *func = "copy_pset" ;

   if ( *to == NULL )
   {
      *to = pset_create( pset_count( from ), 0 ) ;
      if ( *to == NULL )
      {
         out_of_memory( func ) ;
         return( FAILED ) ;
      }
   }

   for ( u = 0 ; u < pset_count( from ) ; u++ )
   {
      char *p = (char *) pset_pointer( from, u ) ;
      char *new_s ;
      
      if ( size == 0 )
         new_s = new_string( p ) ;
      else
         new_s = (char *)malloc( size ) ;

      if ( new_s == NULL )
      {
         out_of_memory( func ) ;
         return( FAILED ) ;
      }

      if ( size != 0 )
         (void) memcpy( new_s, p, size ) ;

      if ( pset_add( *to, new_s ) == NULL )
      {
         free( new_s ) ;
         out_of_memory( func ) ;
         return( FAILED ) ;
      }
   }
   return( OK ) ;
}


/*
 * Disassociate from controlling terminal
 */
void no_control_tty(void)
{
   (void) setsid() ;
}


/*
 * Write the whole buffer to the given file descriptor ignoring interrupts
 */
status_e write_buf( int fd, const char *buf, int len )
{
   int i ;
   ssize_t cc;

   for ( i = 0 ; len > 0 ; i += cc, len -= cc )
   {
      cc = write( fd, buf+i, len ) ;
      if ( cc == -1 )
      {
         if ( errno != EINTR )
            return( FAILED ) ;
         cc = 0 ;
      }
   }
   return( OK ) ;
}


void tabprint( int fd, int tab_level, const char *fmt, ...)
{
   va_list ap ;
   int i ;

   for ( i = 0 ; i < tab_level ; i++ )
      Sputchar( fd, '\t' ) ;

   va_start( ap, fmt ) ;
   Sprintv( fd, fmt, ap ) ;
   va_end( ap ) ;
}


/*
 * Empty the socket receive buffers of all data.
 */
void drain( int sd )
{
   char buf[ 256 ] ; /* This size is arbitrarily chosen */
   ssize_t ret ;
   int old_val ;

   /* Put in non-blocking mode so we don't hang. */
   old_val = fcntl( sd, F_GETFL, FNDELAY );
   if ( fcntl( sd, F_SETFL, FNDELAY ) < 0 )
   {
      if ( debug.on )
         msg( LOG_DEBUG, "drain",
              "UDP socket could not be made non-blocking: %m" ) ;
      return;
   }

   do {
      ret = recv( sd, buf, sizeof( buf ), 0 ) ;
   } while (ret > 0);

   /* Restore the value since the connection will be freed, not closed. */
   if (old_val >= 0)
      fcntl( sd, F_SETFL, old_val );

   if ( debug.on )
      msg( LOG_DEBUG, "drain", "UDP socket should be empty" ) ;
}

/*
 * Convert string to an int detecting errors.
 */
int parse_int(const char *str, int base, int term, int *res)
{
	char *endptr;
	long strtol_res;

/* SUSv2 says:
 * "Because 0, LONG_MIN and LONG_MAX are returned on error and are also
 * valid returns on success, an application wishing to check for error
 * situations should set errno to 0, then call strtol(), then check errno." */
	errno = 0;
	strtol_res = strtol(str, (char **)&endptr, base);

	if (errno == 0 && *str != NUL) {
		/* Special case: -1 means allow trailing whitespace */
		if (term == -1) {
			while (*endptr != NUL && isspace(*endptr))
				endptr++;
			term = NUL;
		}

		if (*endptr == term) {
			*res = strtol_res;
			return 0;
		}
	}

	*res = 0;
	return -1;
}

int parse_uint(const char *str, int base, int term, unsigned int *res)
{
	unsigned long long tmp;
	int ret;
	ret = parse_ull(str, base, term, &tmp);
	*res = (unsigned int)tmp;
	return ret;
}

int parse_ull(const char *str, int base, int term, unsigned long long *res)
{
	char *endptr;
	unsigned long long strtol_res;

/* SUSv2 says:
 * "Because 0, LONG_MIN and LONG_MAX are returned on error and are also
 * valid returns on success, an application wishing to check for error
 * situations should set errno to 0, then call strtol(), then check errno." */
	errno = 0;
	strtol_res = strtoull(str, (char **)&endptr, base);

	if (errno == 0 && *str != NUL) {
		/* Special case: -1 means allow trailing whitespace */
		if (term == -1) {
			while (*endptr != NUL && isspace(*endptr))
				endptr++;
			term = NUL;
		}

		if (*endptr == term) {
			*res = strtol_res;
			return 0;
		}
	}

	*res = 0;
	return -1;
}

int parse_ubase10(const char *str, unsigned int *res)
{
	return parse_uint(str, 10, -1, res);
}

int parse_base10(const char *str, int *res)
{
	return parse_int(str, 10, -1, res);
}

bool_int parse_all_digits(const char *ptr)
{
	size_t num=0, len = strlen(ptr);

	while (isdigit(*ptr++))
		num++;
	if (num == len)
		return TRUE;
	else
		return FALSE;
}
