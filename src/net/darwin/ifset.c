/**
 * @file darwin/ifset.c
 *
 * Copyright (C) 2017 kristopher tate & connectFree Corporation
 */

#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h> // ND6_INFINITE_LIFETIME
#include <sys/stat.h>
#include <sys/errno.h>
#include <net/if_dl.h> // struct sockaddr_dl
#include <net/route.h> // AF_ROUTE things


#include <re_types.h>
#include <re_fmt.h>
#include <re_sa.h>
#include <re_net.h>


#define DEBUG_MODULE "ifset"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


int net_if_setmtu(const char *ifname, uint32_t mtu)
{

  int s = socket(AF_INET6, SOCK_DGRAM, 0);

  if (s < 0) {
    DEBUG_WARNING("net_if_setmtu: socket()\n");
    return EINVAL;
  }

  struct ifreq ifRequest;

  strncpy(ifRequest.ifr_name, ifname, IFNAMSIZ);
  ifRequest.ifr_mtu = mtu;

  DEBUG_INFO("net_if_setaddr: MTU for dev [%s] set to [%u", ifname, mtu);

  if (ioctl(s, SIOCSIFMTU, &ifRequest) < 0) {
    DEBUG_WARNING( "net_if_setmtu: ioctl(SIOCSIFMTU)\n");
  }

  close(s);

  return 0;
  
}

int net_if_setaddr( const char *ifname
                  , const struct sa *ip
                  , int prefix_len )
{

  struct in6_aliasreq addreq6;

  if (!ifname || !ip)
    return EINVAL;

  /* TODO: support ipv4 */
  if (ip->u.sa.sa_family != AF_INET6)
    return EINVAL;


  memset(&addreq6, 0, sizeof(struct in6_aliasreq));
  addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
  addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
  //addreq6.ifra_lifetime.ia6t_expire = ND6_INFINITE_LIFETIME;
  //addreq6.ifra_lifetime.ia6t_preferred = ND6_INFINITE_LIFETIME;

  int fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (fd < 0) {
    DEBUG_WARNING("net_if_setaddr: socket()\n");
    return EINVAL;
  }

  strncpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));

  memcpy(&addreq6.ifra_addr, &ip->u.in6, sizeof(struct sockaddr_in6));

  addreq6.ifra_addr.sin6_family = AF_INET6;
  addreq6.ifra_addr.sin6_len = sizeof(addreq6.ifra_addr);

  /* set prefix mask */
  struct sockaddr_in6* mask = &addreq6.ifra_prefixmask;

  mask->sin6_len = sizeof(*mask);
  if (prefix_len >= 128 || prefix_len <= 0) {
    memset(&mask->sin6_addr, 0xff, sizeof(struct in6_addr));
  } else {
    memset((void *)&mask->sin6_addr, 0x00, sizeof(mask->sin6_addr));
    memset((void *)&mask->sin6_addr, 0xff, prefix_len>>3);
    ((uint8_t*)&mask->sin6_addr)[prefix_len>>3] = 0xff << (8 - (prefix_len%8));
  }

  if(-1 == ioctl(fd, SIOCAIFADDR_IN6, &addreq6)) {
    DEBUG_WARNING( "net_if_setaddr: ioctl(SIOCAIFADDR_IN6)\n");
    close(fd);
    return EINVAL;
  }

  close(fd);
  return 0;
}

