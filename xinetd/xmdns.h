#ifndef _XMDNS_H_
#define _XMDNS_H_
#include "sconf.h"

int xinetd_mdns_deregister(struct service_config *scp) ;
int xinetd_mdns_register(struct service_config *scp) ;
int xinetd_mdns_init(void) ;
int xinetd_mdns_svc_init(struct service_config *scp) ;
int xinetd_mdns_svc_free(struct service_config *scp) ;
int xinetd_mdns_poll(void) ;

#endif /* _XMDNS_H_ */
