#include "config.h"
#ifdef HAVE_MDNS
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "state.h"
#include "service.h"
#include "server.h"
#include "sconf.h"
#include "pset.h"

#ifdef HAVE_DNSREGISTRATION
#include <DNSServiceDiscovery/DNSServiceDiscovery.h>
#endif

#ifdef HAVE_HOWL
#include <howl.h>
#endif

extern struct program_state ps ;

#ifdef HAVE_DNSREGISTRATION
static void mdns_callback(DNSServiceRegistrationReplyErrorType err, void *d)
{
   return;
}
#endif

#ifdef HAVE_HOWL
static sw_result howl_callback(sw_discovery discovery, sw_discovery_oid oid, sw_discovery_publish_status status, sw_opaque extra) {
   if( debug.on ) {
      const char *name = NULL;
      unsigned u;
      for ( u = 0 ; u < pset_count( SERVICES( ps ) ) ; u++ ) {
         struct service *sp;
         struct service_config *scp;
   
         sp = SP( pset_pointer( SERVICES( ps ), u ) ) ;
         scp = SVC_CONF(sp);
         if( scp->mdns_state && (*(sw_discovery_oid *)scp->mdns_state == oid) ) {
            name = SC_ID(scp);
            break;
         }
      }
      msg(LOG_DEBUG, "howl_callback", "callback status: %d for service %s (oid: %d)", status, name, oid);
   }
   return SW_OKAY;
}
#endif

int xinetd_mdns_deregister(struct service_config *scp) {
   if( (!scp) || (scp->mdns_state == NULL) )
      return 0;

   if( debug.on )
      msg(LOG_DEBUG, "xinetd_mdns_deregister", "Deregistering service: %s", SC_ID(scp));

#ifdef HAVE_DNSREGISTRATION
   DNSServiceDiscoveryDeallocate(  (dns_service_discovery_ref)scp->mdns_state );
   return 0;
#endif

#ifdef HAVE_HOWL
   if( !ps.rws.mdns_state ) return 0;
   if( sw_discovery_cancel(*(sw_discovery *)ps.rws.mdns_state, *(sw_discovery_oid *)scp->mdns_state) != SW_OKAY )
      return -1;
   return 0;
#endif
}

int xinetd_mdns_register(struct service_config *scp) {
   if( ps.rws.mdns_state == NULL )
      return -1;
   xinetd_mdns_deregister(scp);

   if( SC_MDNS_NAME(scp) )
      free(SC_MDNS_NAME(scp));
   if( asprintf(&SC_MDNS_NAME(scp), "_%s._%s", SC_NAME(scp), SC_PROTONAME(scp)) < 0 )
       return -1;

   if( debug.on )
      msg(LOG_DEBUG, "xinetd_mdns_register", "Registering service: %s (%s)", SC_MDNS_NAME(scp), SC_ID(scp));

#ifdef HAVE_DNSREGISTRATION
   scp->mdns_state = DNSServiceRegistrationCreate("", SC_MDNS_NAME(scp), "", htons(SC_PORT(scp)), "", mdns_callback, NULL);
#endif

#ifdef HAVE_HOWL
   sw_discovery_publish(*(sw_discovery *)ps.rws.mdns_state, 0, SC_ID(scp), SC_MDNS_NAME(scp), NULL, NULL, SC_PORT(scp), NULL, 0, howl_callback, NULL, (sw_discovery_oid *)scp->mdns_state);
   return 0;
#endif

   return 0;
}

int xinetd_mdns_init(void) {
#ifdef HAVE_DNSREGISTRATION
   ps.rws.mdns_state = (char *)1;
   return 0;
#endif

#ifdef HAVE_HOWL
   ps.rws.mdns_state = malloc(sizeof(sw_discovery));
   if( !ps.rws.mdns_state )
      return -1;
   if( sw_discovery_init(ps.rws.mdns_state) != SW_OKAY ) {
      free(ps.rws.mdns_state);
      ps.rws.mdns_state = NULL;
      return -1;
   }
   FD_SET( sw_discovery_socket(*(sw_discovery *)ps.rws.mdns_state), &ps.rws.socket_mask ) ;
   return 0;
#endif
}

int xinetd_mdns_svc_init(struct service_config *scp) {
#ifdef HAVE_DNSREGISTRATION
//   scp->mdns_state = malloc(sizeof(dns_service_discovery_ref));
#endif

#ifdef HAVE_HOWL
   scp->mdns_state = malloc(sizeof(sw_discovery_oid));
#endif

   if( !scp->mdns_state )
      return -1;
   return 0;
}

int xinetd_mdns_svc_free(struct service_config *scp) {
#ifndef HAVE_DNSREGISTRATION
   if(scp->mdns_state)
      free(scp->mdns_state);
   scp->mdns_state = NULL;
#endif
   return 0;
}

int xinetd_mdns_poll(void) {
#ifdef HAVE_HOWL
   if( sw_discovery_read_socket(*(sw_discovery *)ps.rws.mdns_state) == SW_OKAY )
      return 0;
#endif
   return -1;
}
#endif /* HAVE_MDNS */
