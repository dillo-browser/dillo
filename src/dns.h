#ifndef __DNS_H__
#define __DNS_H__

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef void (*DnsCallback_t)(int Status, Dlist *addr_list, void *data);

void a_Dns_init (void);
void a_Dns_freeall(void);
void a_Dns_resolve(const char *hostname, DnsCallback_t cb_func, void *cb_data);

#define DILLO_ADDR_MAX sizeof(struct in6_addr)

typedef struct _DilloHost
{
  int af;
  int alen;
  char data[DILLO_ADDR_MAX];
} DilloHost;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DNS_H__ */
