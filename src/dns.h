#ifndef __DNS_H__
#define __DNS_H__

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef void (*DnsCallback_t)(int status, Dlist *addr_list, void *data);

void a_Dns_init (void);
void a_Dns_freeall(void);
void a_Dns_resolve(const char *hostname, DnsCallback_t cb_func, void *cb_data);

#ifdef ENABLE_IPV6
#  define DILLO_ADDR_MAX sizeof(struct in6_addr)
#else
#  define DILLO_ADDR_MAX sizeof(struct in_addr)
#endif

typedef struct {
  int af;
  int alen;
  char data[DILLO_ADDR_MAX];
} DilloHost;

void a_Dns_dillohost_to_string(DilloHost *host, char *dst, size_t size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DNS_H__ */
