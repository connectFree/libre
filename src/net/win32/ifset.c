/**
 * @file win32/ifset.c
 *
 * Copyright (C) 2017 kristopher tate & connectFree Corporation
 */

#include <re_types.h>
#include <re_fmt.h>
#include <re_sa.h>
#include <re_net.h>

#include <winsock2.h>
#include <windows.h>

#include <ws2ipdef.h>
#include <naptypes.h>
#include <ntddndis.h>
#include <string.h>
#include <ws2def.h>
#include <iprtrmib.h>
#include <ifdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

#define DEBUG_MODULE "ifset"
#define DEBUG_LEVEL 5
#include <re_dbg.h>

#if 0
#define NET_LUID misalligned_NET_LUID
#define PNET_LUID misalligned_PNET_LUID
#define IF_LUID misalligned_IF_LUID
#define PIF_LUID misalligned_PIF_LUID
#include <ifdef.h>
#undef NET_LUID
#undef PNET_LUID
#undef IF_LUID
#undef PIF_LUID

typedef union NET_LUID {
  ULONG64 Value;
  __C89_NAMELESS struct {
    ULONG64 Reserved  :24;
    ULONG64 NetLuidIndex  :24;
    ULONG64 IfType  :16;
  } Info;
} NET_LUID, *PNET_LUID;
#endif

static int _toluid(const char* name, PNET_LUID luid)
{
  NET_LUID out;
  uint16_t ifName[IF_MAX_STRING_SIZE + 1] = {0};
  if (0 == MultiByteToWideChar(CP_ACP
                              , 0
                              , name
                              , strlen(name)
                              , ifName
                              , IF_MAX_STRING_SIZE + 1)) {
    return EINVAL;    
}
  if (NO_ERROR != ConvertInterfaceAliasToLuid(ifName, &out))
    return EINVAL;
  memcpy(luid, &out, sizeof(NET_LUID));
  return 0;
}

static LONG _flushaddrs(NET_LUID luid, MIB_UNICASTIPADDRESS_TABLE *table)
{
  LONG err = NO_ERROR;
  for (int i = 0; i < (int)table->NumEntries; i++) {
    if (table->Table[i].InterfaceLuid.Value == luid.Value) {
      if ((err = DeleteUnicastIpAddressEntry(&table->Table[i]))) {
        return err;
      }
    }
  }
  return err;
}

static int _device_flushaddr(const char *ifname)
{
  LONG ret;
  NET_LUID luid;
  MIB_UNICASTIPADDRESS_TABLE* table;

  if (_toluid(ifname, &luid))
    return EINVAL;

  if (!GetUnicastIpAddressTable(AF_INET, &table))
    return EINVAL;

  ret = _flushaddrs(luid, table);
  FreeMibTable(table);
  if (ret) {
    DEBUG_WARNING("DeleteUnicastIpAddressEntry(&table->Table[i])");
    return EINVAL;
  }

  if (!GetUnicastIpAddressTable(AF_INET6, &table)) {
    return EINVAL;
  }
  ret = _flushaddrs(luid, table);
  FreeMibTable(table);
  if (ret) {
    DEBUG_WARNING("DeleteUnicastIpAddressEntry(&table->Table[i])");
    return EINVAL;
  }
  return 0;
}

int net_if_setmtu(const char *ifname, uint32_t mtu)
{
  return 0;
}

int net_if_setaddr( const char *ifname
                  , const struct sa *ip
                  , int prefix_len )
{
  MIB_UNICASTIPADDRESS_ROW uip = {
      .PrefixOrigin = IpPrefixOriginUnchanged,
      .SuffixOrigin = IpSuffixOriginUnchanged,
      .ValidLifetime = 0xFFFFFFFF,
      .PreferredLifetime = 0xFFFFFFFF,
      .OnLinkPrefixLength = 0xFF
  };

  _device_flushaddr(ifname);

  if (_toluid(ifname, &uip.InterfaceLuid))
    return EINVAL;

  uip.Address.si_family = ip->u.sa.sa_family;

  switch (ip->u.sa.sa_family) {
    case AF_INET6:
      memcpy(&uip.Address.Ipv6.sin6_addr, ip->u.in6.sin6_addr.s6_addr, 16);
      break;
    case AF_INET:
      memcpy(&uip.Address.Ipv4.sin_addr, &ip->u.in.sin_addr.s_addr, 4);
      break;
    default:
      return EINVAL;
  }

  uip.OnLinkPrefixLength = prefix_len;

  if (!CreateUnicastIpAddressEntry(&uip)){
    return EINVAL;
  }

  return 0;
}
