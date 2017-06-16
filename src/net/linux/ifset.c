/**
 * @file linux/ifset.c
 *
 * Copyright (C) 2017 kristopher tate & connectFree Corporation
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/route.h>
#include <net/if.h>

#include <re_types.h>
#include <re_fmt.h>
#include <re_sa.h>
#include <re_net.h>

#if !defined(_LINUX_IN6_H) && !defined(_UAPI_LINUX_IN6_H)
    struct in6_ifreq {
        struct in6_addr ifr6_addr;
        uint32_t ifr6_prefixlen;
        int ifr6_ifindex;
    };
#endif

#define DEBUG_MODULE "ifset"
#define DEBUG_LEVEL 5
#include <re_dbg.h>

#define _IFNAME_SOCK(af_type, ifr, ifname) {\
    if ((s = socket(af_type, SOCK_DGRAM, 0)) < 0) { \
    	err = errno; \
    	goto err; \
    } \
    memset(&ifr, 0, sizeof(struct ifreq)); \
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ); \
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) { \
        err = errno; \
        goto err; \
        DEBUG_WARNING("ioctl(SIOCGIFINDEX) [%s]", strerror(err)); \
    } \
}


int net_if_setmtu(const char *ifname, uint32_t mtu)
{
	int err = 0;
    struct ifreq ifr;
    int s = -1;

    _IFNAME_SOCK(AF_INET6, ifr, ifname);

    ifr.ifr_mtu = mtu;
    if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
        err = errno;
        goto err;
        DEBUG_WARNING("ioctl(SIOCSIFMTU) [%s]", strerror(err));
    }

err:
	if (s > 0)
		close(s);
	return err;
}

int net_if_setaddr( const char *ifname
                  , const struct sa *ip
                  , int prefix_len )
{
	int _idx;
	int s = -1;
	int err = 0;
	struct ifreq ifr;

	_IFNAME_SOCK(sa_af(ip), ifr, ifname);
	_idx = ifr.ifr_ifindex;

/* BRING UP */
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
        err = errno;
        goto err;
        DEBUG_WARNING("ioctl(SIOCGIFFLAGS) [%s]", strerror(err));
    }

    if (!(ifr.ifr_flags & IFF_UP & IFF_RUNNING)) {
	    DEBUG_INFO("[%s] GOING UP", ifr.ifr_name);
	    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
	        err = errno;
	        goto err;
	        DEBUG_WARNING("ioctl(SIOCSIFFLAGS) [%s]", strerror(err));
	    }
	}

    if (ip->u.sa.sa_family == AF_INET6) {
        struct in6_ifreq ifr6 = {
            .ifr6_ifindex = _idx,
            .ifr6_prefixlen = prefix_len
        };

        memcpy(&ifr6.ifr6_addr, (void *)(&ip->u.in6.sin6_addr), 16);

        if (ioctl(s, SIOCSIFADDR, &ifr6) < 0) {
	        err = errno;
	        goto err;
            DEBUG_WARNING("ioctl(SIOCSIFADDR) [%s]", strerror(err));
        }
    }

err:
	if (s > 0)
		close(s);
	return err;
}
