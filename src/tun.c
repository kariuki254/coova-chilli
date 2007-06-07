/* 
 *
 * TUN interface functions.
 * Copyright (C) 2002, 2003, 2004 Mondru AB.
 * Copyright (c) 2006 Coova Technologies Ltd
 * 
 * The contents of this file may be used under the terms of the GNU
 * General Public License Version 2, provided that the above copyright
 * notice and this permission notice is included in all copies or
 * substantial portions of the software.
 * 
 */

/*
 * tun.c: Contains all TUN functionality. Is able to handle multiple
 * tunnels in the same program. Each tunnel is identified by the struct,
 * which is passed to functions.
 *
 */


#include "system.h"
#include "tun.h"
#include "ippool.h"
#include "radius.h"
#include "radius_wispr.h"
#include "radius_chillispot.h"
#include "redir.h"
#include "syserr.h"
#include "dhcp.h"
#include "cmdline.h"
#include "chilli.h"
#include "options.h"


#if defined(__linux__)

int tun_nlattr(struct nlmsghdr *n, int nsize, int type, void *d, int dlen)
{
  int len = RTA_LENGTH(dlen);
  int alen = NLMSG_ALIGN(n->nlmsg_len);
  struct rtattr *rta = (struct rtattr*) (((void*)n) + alen);
  if (alen + len > nsize)
    return -1;
  rta->rta_len = len;
  rta->rta_type = type;
  memcpy(RTA_DATA(rta), d, dlen);
  n->nlmsg_len = alen + len;
  return 0;
}

int tun_gifindex(struct tun_t *this, int *index) {
  struct ifreq ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_addr.sa_family = AF_INET;
  ifr.ifr_dstaddr.sa_family = AF_INET;
  ifr.ifr_netmask.sa_family = AF_INET;
  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno, "socket() failed");
  }
  if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
    log_err(errno,"ioctl() failed");
    close(fd);
    return -1;
  }
  close(fd);
  *index = ifr.ifr_ifindex;
  return 0;
}
#endif

int tun_sifflags(struct tun_t *this, int flags) {
  struct ifreq ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_flags = flags;
  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno, "socket() failed");
  }
  if (ioctl(fd, SIOCSIFFLAGS, &ifr)) {
    log_err(errno," ioctl(SIOCSIFFLAGS) failed");
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}


/* Currently unused 
int tun_addroute2(struct tun_t *this,
		  struct in_addr *dst,
		  struct in_addr *gateway,
		  struct in_addr *mask) {
  
  struct {
    struct nlmsghdr 	n;
    struct rtmsg 	r;
    char buf[TUN_NLBUFSIZE];
  } req;
  
  struct sockaddr_nl local;
  int addr_len;
  int fd;
  int status;
  struct sockaddr_nl nladdr;
  struct iovec iov;
  struct msghdr msg;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
  req.n.nlmsg_type = RTM_NEWROUTE;
  req.r.rtm_family = AF_INET;
  req.r.rtm_table  = RT_TABLE_MAIN;
  req.r.rtm_protocol = RTPROT_BOOT;
  req.r.rtm_scope  = RT_SCOPE_UNIVERSE;
  req.r.rtm_type  = RTN_UNICAST;
  tun_nlattr(&req.n, sizeof(req), RTA_DST, dst, 4);
  tun_nlattr(&req.n, sizeof(req), RTA_GATEWAY, gateway, 4);
  
  if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
    return -1;
  }

  memset(&local, 0, sizeof(local));
  local.nl_family = AF_NETLINK;
  local.nl_groups = 0;
  
  if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "bind() failed");
    close(fd);
    return -1;
  }

  addr_len = sizeof(local);
  if (getsockname(fd, (struct sockaddr*)&local, &addr_len) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "getsockname() failed");
    close(fd);
    return -1;
  }

  if (addr_len != sizeof(local)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address length %d", addr_len);
    close(fd);
    return -1;
  }

  if (local.nl_family != AF_NETLINK) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address family %d", local.nl_family);
    close(fd);
    return -1;
  }
  
  iov.iov_base = (void*)&req.n;
  iov.iov_len = req.n.nlmsg_len;

  msg.msg_name = (void*)&nladdr;
  msg.msg_namelen = sizeof(nladdr),
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  req.n.nlmsg_seq = 0;
  req.n.nlmsg_flags |= NLM_F_ACK;

  status = sendmsg(fd, &msg, 0);  * TODO: Error check *
  close(fd);
  return 0;
}
*/

int tun_addaddr(struct tun_t *this,
		struct in_addr *addr,
		struct in_addr *dstaddr,
		struct in_addr *netmask) {

#if defined(__linux__)
  struct {
    struct nlmsghdr 	n;
    struct ifaddrmsg 	i;
    char buf[TUN_NLBUFSIZE];
  } req;
  
  struct sockaddr_nl local;
  int addr_len;
  int fd;
  int status;
  
  struct sockaddr_nl nladdr;
  struct iovec iov;
  struct msghdr msg;

  if (!this->addrs) /* Use ioctl for first addr to make ping work */
    return tun_setaddr(this, addr, dstaddr, netmask);

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
  req.n.nlmsg_type = RTM_NEWADDR;
  req.i.ifa_family = AF_INET;
  req.i.ifa_prefixlen = 32; /* 32 FOR IPv4 */
  req.i.ifa_flags = 0;
  req.i.ifa_scope = RT_SCOPE_HOST; /* TODO or 0 */
  if (tun_gifindex(this, &req.i.ifa_index)) {
    return -1;
  }

  tun_nlattr(&req.n, sizeof(req), IFA_ADDRESS, addr, sizeof(addr));
  tun_nlattr(&req.n, sizeof(req), IFA_LOCAL, dstaddr, sizeof(dstaddr));

  if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
    log_err(errno,"socket() failed");
    return -1;
  }

  memset(&local, 0, sizeof(local));
  local.nl_family = AF_NETLINK;
  local.nl_groups = 0;
  
  if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
    log_err(errno, "bind() failed");
    close(fd);
    return -1;
  }

  addr_len = sizeof(local);
  if (getsockname(fd, (struct sockaddr*)&local, (socklen_t *) &addr_len) < 0) {
    log_err(errno,
	    "getsockname() failed");
    close(fd);
    return -1;
  }

  if (addr_len != sizeof(local)) {
    log_err(0,
	    "Wrong address length %d", addr_len);
    close(fd);
    return -1;
  }

  if (local.nl_family != AF_NETLINK) {
    log_err(0,
	    "Wrong address family %d", local.nl_family);
    close(fd);
    return -1;
  }
  
  iov.iov_base = (void*)&req.n;
  iov.iov_len = req.n.nlmsg_len;

  msg.msg_name = (void*)&nladdr;
  msg.msg_namelen = sizeof(nladdr),
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  req.n.nlmsg_seq = 0;
  req.n.nlmsg_flags |= NLM_F_ACK;

  status = sendmsg(fd, &msg, 0); /* TODO Error check */

  tun_sifflags(this, IFF_UP | IFF_RUNNING); /* TODO */
  close(fd);
  this->addrs++;
  return 0;

#elif defined (__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)

  int fd;
  struct ifaliasreq      areq;

  /* TODO: Is this needed on FreeBSD? */
  if (!this->addrs) /* Use ioctl for first addr to make ping work */
    return tun_setaddr(this, addr, dstaddr, netmask); /* TODO dstaddr */

  memset(&areq, 0, sizeof(areq));

  /* Set up interface name */
  strncpy(areq.ifra_name, this->devname, IFNAMSIZ);
  areq.ifra_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */

  ((struct sockaddr_in*) &areq.ifra_addr)->sin_family = AF_INET;
  ((struct sockaddr_in*) &areq.ifra_addr)->sin_len = sizeof(areq.ifra_addr);
  ((struct sockaddr_in*) &areq.ifra_addr)->sin_addr.s_addr = addr->s_addr;

  ((struct sockaddr_in*) &areq.ifra_mask)->sin_family = AF_INET;
  ((struct sockaddr_in*) &areq.ifra_mask)->sin_len    = sizeof(areq.ifra_mask);
  ((struct sockaddr_in*) &areq.ifra_mask)->sin_addr.s_addr = netmask->s_addr;

  /* For some reason FreeBSD uses ifra_broadcast for specifying dstaddr */
  ((struct sockaddr_in*) &areq.ifra_broadaddr)->sin_family = AF_INET;
  ((struct sockaddr_in*) &areq.ifra_broadaddr)->sin_len = 
    sizeof(areq.ifra_broadaddr);
  ((struct sockaddr_in*) &areq.ifra_broadaddr)->sin_addr.s_addr = 
    dstaddr->s_addr;

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno,
	    "socket() failed");
    return -1;
  }
  
  if (ioctl(fd, SIOCAIFADDR, (void *) &areq) < 0) {
    log_err(errno,
	    "ioctl(SIOCAIFADDR) failed");
    close(fd);
    return -1;
  }

  close(fd);
  this->addrs++;
  return 0;

#elif defined (__sun__)
  
  if (!this->addrs) /* Use ioctl for first addr to make ping work */
    return tun_setaddr(this, addr, dstaddr, netmask);
  
  log_err(errno,
	  "Setting multiple addresses not possible on Solaris");
  return -1;

#else
#error  "Unknown platform!"
#endif
  
}


int tun_setaddr(struct tun_t *this,
		struct in_addr *addr,
		struct in_addr *dstaddr,
		struct in_addr *netmask)
{
  struct ifreq   ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_addr.sa_family = AF_INET;
  ifr.ifr_dstaddr.sa_family = AF_INET;

#if defined(__linux__)
  ifr.ifr_netmask.sa_family = AF_INET;

#elif defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)
  ((struct sockaddr_in *) &ifr.ifr_addr)->sin_len = 
    sizeof (struct sockaddr_in);
  ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_len = 
    sizeof (struct sockaddr_in);
#endif

  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno,
	    "socket() failed");
    return -1;
  }

  if (addr) { /* Set the interface address */
    this->addr.s_addr = addr->s_addr;
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = addr->s_addr;
    if (ioctl(fd, SIOCSIFADDR, (void *) &ifr) < 0) {
      if (errno != EEXIST) {
	log_err(errno,
		"ioctl(SIOCSIFADDR) failed");
      }
      else {
	sys_err(LOG_WARNING, __FILE__, __LINE__, errno,
		"ioctl(SIOCSIFADDR): Address already exists");
      }
      close(fd);
      return -1;
    }
  }

  if (dstaddr) { /* Set the destination address */
    this->dstaddr.s_addr = dstaddr->s_addr;
    ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_addr.s_addr = 
      dstaddr->s_addr;
    if (ioctl(fd, SIOCSIFDSTADDR, (caddr_t) &ifr) < 0) {
      log_err(errno,
	      "ioctl(SIOCSIFDSTADDR) failed");
      close(fd);
      return -1; 
    }
  }

  if (netmask) { /* Set the netmask */
    this->netmask.s_addr = netmask->s_addr;
#if defined(__linux__)
    ((struct sockaddr_in *) &ifr.ifr_netmask)->sin_addr.s_addr = 
      netmask->s_addr;

#elif defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = 
      netmask->s_addr;

#elif defined(__sun__)
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = 
      netmask->s_addr;
#else
#error  "Unknown platform!"
#endif

    if (ioctl(fd, SIOCSIFNETMASK, (void *) &ifr) < 0) {
      log_err(errno,
	      "ioctl(SIOCSIFNETMASK) failed");
      close(fd);
      return -1;
    }
  }
  
  close(fd);
  this->addrs++;
  
  /* On linux the route to the interface is set automatically
     on FreeBSD we have to do this manually */

  /* TODO: How does it work on Solaris? */

  tun_sifflags(this, IFF_UP | IFF_RUNNING);

#if defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)
  tun_addroute(this, dstaddr, addr, netmask);
  this->routes = 1;
#endif

  return 0;
}

int tun_route(struct tun_t *this,
	      struct in_addr *dst,
	      struct in_addr *gateway,
	      struct in_addr *mask,
	      int delete)
{


  /* TODO: Learn how to set routing table on sun  */

#if defined(__linux__)

  struct rtentry r;
  int fd;

  memset (&r, '\0', sizeof (r));
  r.rt_flags = RTF_UP | RTF_GATEWAY; /* RTF_HOST not set */

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno,
	    "socket() failed");
    return -1;
  }

  r.rt_dst.sa_family     = AF_INET;
  r.rt_gateway.sa_family = AF_INET;
  r.rt_genmask.sa_family = AF_INET;
  ((struct sockaddr_in *) &r.rt_dst)->sin_addr.s_addr = dst->s_addr;
  ((struct sockaddr_in *) &r.rt_gateway)->sin_addr.s_addr = gateway->s_addr;
  ((struct sockaddr_in *) &r.rt_genmask)->sin_addr.s_addr = mask->s_addr;
  
  if (delete) {
    if (ioctl(fd, SIOCDELRT, (void *) &r) < 0) {
      log_err(errno,
	      "ioctl(SIOCDELRT) failed");
      close(fd);
      return -1;
    }
  }
  else {
    if (ioctl(fd, SIOCADDRT, (void *) &r) < 0) {
      log_err(errno,
	      "ioctl(SIOCADDRT) failed");
      close(fd);
      return -1;
    }
  }
  close(fd);
  return 0;
  
#elif defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)

struct {
  struct rt_msghdr rt;
  struct sockaddr_in dst;
  struct sockaddr_in gate;
  struct sockaddr_in mask;
} req;

 int fd;
 struct rt_msghdr *rtm;
 
 if((fd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1) {
   log_err(errno,
	   "socket() failed");
   return -1;
 }
 
 memset(&req, 0x00, sizeof(req));
 
 rtm  = &req.rt;
 
 rtm->rtm_msglen = sizeof(req);
 rtm->rtm_version = RTM_VERSION;
 if (delete) {
   rtm->rtm_type = RTM_DELETE;
 }
 else {
   rtm->rtm_type = RTM_ADD;
 }
 rtm->rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;  /* TODO */
 rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
 rtm->rtm_pid = getpid();      
 rtm->rtm_seq = 0044;                                 /* TODO */
 
 req.dst.sin_family       = AF_INET;
 req.dst.sin_len          = sizeof(req.dst);
 req.mask.sin_family      = AF_INET;
 req.mask.sin_len         = sizeof(req.mask);
 req.gate.sin_family      = AF_INET;
 req.gate.sin_len         = sizeof(req.gate);

 req.dst.sin_addr.s_addr  = dst->s_addr;
 req.mask.sin_addr.s_addr = mask->s_addr;
 req.gate.sin_addr.s_addr = gateway->s_addr;
 
 if(write(fd, rtm, rtm->rtm_msglen) < 0) {
   log_err(errno,
	   "write() failed");
   close(fd);
   return -1;
 }
 close(fd);
 return 0;

#elif defined(__sun__)
 sys_err(LOG_WARNING, __FILE__, __LINE__, errno,
	 "Could not set up routing on Solaris. Please add route manually.");
 return 0;
 
#else
#error  "Unknown platform!"
#endif

}

int tun_addroute(struct tun_t *this,
		 struct in_addr *dst,
		 struct in_addr *gateway,
		 struct in_addr *mask)
{
  return tun_route(this, dst, gateway, mask, 0);
}

int tun_delroute(struct tun_t *this,
		 struct in_addr *dst,
		 struct in_addr *gateway,
		 struct in_addr *mask)
{
  return tun_route(this, dst, gateway, mask, 1);
}


int tun_new(struct tun_t **tun, int txqlen)
{

#if defined(__linux__)
  struct ifreq ifr;

#elif defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)
  char devname[IFNAMSIZ+5]; /* "/dev/" + ifname */
  int devnum;
  struct ifaliasreq areq;
  int fd;

#elif defined(__sun__)
  int if_fd, ppa = -1;
  static int ip_fd = 0;
  int muxid;
  struct ifreq ifr;

#else
#error  "Unknown platform!"
#endif
  
  if (!(*tun = calloc(1, sizeof(struct tun_t)))) {
    log_err(errno, "calloc() failed");
    return EOF;
  }
  
  (*tun)->cb_ind = NULL;
  (*tun)->addrs = 0;
  (*tun)->routes = 0;
  
#if defined(__linux__)
  /* Open the actual tun device */
  if (((*tun)->fd  = open("/dev/net/tun", O_RDWR)) < 0) {
    log_err(errno, "open() failed");
    return -1;
  }
  
  /* Set device flags. For some weird reason this is also the method
     used to obtain the network interface name */
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = (options.tap ? IFF_TAP : IFF_TUN ) | IFF_NO_PI; /* Tun device, no packet info */
#if defined(IFF_ONE_QUEUE) && defined(SIOCSIFTXQLEN)
  ifr.ifr_flags |= IFF_ONE_QUEUE;
#endif
  if (ioctl((*tun)->fd, TUNSETIFF, (void *) &ifr) < 0) {
    log_err(errno, "ioctl() failed");
    close((*tun)->fd);
    return -1;
  } 
  
#if defined(IFF_ONE_QUEUE) && defined(SIOCSIFTXQLEN)
  {
    struct ifreq nifr;
    int nfd;
    memset(&nifr, 0, sizeof(nifr));
    if ((nfd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) {
      strncpy(nifr.ifr_name, ifr.ifr_name, IFNAMSIZ);
      nifr.ifr_qlen = txqlen;

      if (ioctl(nfd, SIOCSIFTXQLEN, (void *) &nifr) >= 0) 
	sys_err(LOG_INFO, __FILE__, __LINE__, errno, "TX queue length set to %d", txqlen);
      else 
	log_err(errno, "Cannot set tx queue length on %s", ifr.ifr_name);

      close (nfd);
    } else {
      log_err(errno, "Cannot open socket on %s", ifr.ifr_name);
    }
  }
#endif
  
  strncpy((*tun)->devname, ifr.ifr_name, IFNAMSIZ);
  (*tun)->devname[IFNAMSIZ] = 0;
  
  ioctl((*tun)->fd, TUNSETNOCSUM, 1); /* Disable checksums */

  /* Get the MAC address of our tap interface */
  if (options.tap) {
    int fd;
    if ((fd = socket (AF_INET, SOCK_DGRAM, 0)) >= 0) {
      memset(&ifr, 0, sizeof(ifr));
      strncpy(ifr.ifr_name, (*tun)->devname, IFNAMSIZ);
      if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
	log_err(errno, "ioctl(d=%d, request=%d) failed", fd, SIOCGIFHWADDR);
      }
      memcpy(options.tapmac, ifr.ifr_hwaddr.sa_data, DHCP_ETH_ALEN);
      log_dbg("tap-mac: %s %.2x:%.2x:%.2x:%.2x:%.2x:%.2x", ifr.ifr_name,
	      options.tapmac[0],options.tapmac[1],options.tapmac[2],
	      options.tapmac[3],options.tapmac[4],options.tapmac[5]);
      close(fd);
    }
  }

  return 0;
  
#elif defined(__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)

  /* Find suitable device */
  for (devnum = 0; devnum < 255; devnum++) { /* TODO 255 */ 
    snprintf(devname, sizeof(devname), "/dev/tun%d", devnum);
    devname[sizeof(devname)] = 0;
    if (((*tun)->fd = open(devname, O_RDWR)) >= 0) break;
    if (errno != EBUSY) break;
  } 
  if ((*tun)->fd < 0) {
    log_err(errno, "Can't find tunnel device");
    return -1;
  }

  snprintf((*tun)->devname, sizeof((*tun)->devname), "tun%d", devnum);
  (*tun)->devname[sizeof((*tun)->devname)] = 0;

  /* The tun device we found might have "old" IP addresses allocated */
  /* We need to delete those. This problem is not present on Linux */

  memset(&areq, 0, sizeof(areq));

  /* Set up interface name */
  strncpy(areq.ifra_name, (*tun)->devname, IFNAMSIZ);
  areq.ifra_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    log_err(errno,
	    "socket() failed");
    return -1;
  }
  
  /* Delete any IP addresses until SIOCDIFADDR fails */
  while (ioctl(fd, SIOCDIFADDR, (void *) &areq) != -1);

  close(fd);
  return 0;

#elif defined(__sun__)

  if( (ip_fd = open("/dev/udp", O_RDWR, 0)) < 0){
    log_err(errno, "Can't open /dev/udp");
    return -1;
  }
  
  if( ((*tun)->fd = open("/dev/tun", O_RDWR, 0)) < 0){
    log_err(errno, "Can't open /dev/tun");
    return -1;
  }
  
  /* Assign a new PPA and get its unit number. */
  if( (ppa = ioctl((*tun)->fd, TUNNEWPPA, -1)) < 0){
    log_err(errno, "Can't assign new interface");
    return -1;
  }
  
  if( (if_fd = open("/dev/tun", O_RDWR, 0)) < 0){
    log_err(errno, "Can't open /dev/tun (2)");
    return -1;
  }
  if(ioctl(if_fd, I_PUSH, "ip") < 0){
    log_err(errno, "Can't push IP module");
    return -1;
  }
  
  /* Assign ppa according to the unit number returned by tun device */
  if(ioctl(if_fd, IF_UNITSEL, (char *)&ppa) < 0){
    log_err(errno, "Can't set PPA %d", ppa);
    return -1;
  }

  /* Link the two streams */
  if ((muxid = ioctl(ip_fd, I_LINK, if_fd)) < 0) {
    log_err(errno, "Can't link TUN device to IP");
    return -1;
  }

  close (if_fd);
  
  snprintf((*tun)->devname, sizeof((*tun)->devname), "tun%d", ppa);
  (*tun)->devname[sizeof((*tun)->devname)] = 0;

  memset(&ifr, 0, sizeof(ifr));
  strcpy(ifr.ifr_name, (*tun)->devname);
  ifr.ifr_ip_muxid = muxid;
  
  if (ioctl(ip_fd, SIOCSIFMUXID, &ifr) < 0) {
    ioctl(ip_fd, I_PUNLINK, muxid);
    log_err(errno, "Can't set multiplexor id");
    return -1;
  }
  
  /*  if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0)
      msg (M_ERR, "Set file descriptor to non-blocking failed"); */

  return 0;

#else
#error  "Unknown platform!"
#endif

}

int tun_free(struct tun_t *tun)
{

  if (tun->routes) {
    tun_delroute(tun, &tun->dstaddr, &tun->addr, &tun->netmask);
  }

  if (close(tun->fd)) {
    log_err(errno, "close() failed");
  }

  /* TODO: For solaris we need to unlink streams */

  free(tun);
  return 0;
}


int tun_set_cb_ind(struct tun_t *this, 
  int (*cb_ind) (struct tun_t *tun, void *pack, unsigned len)) {
  this->cb_ind = cb_ind;
  return 0;
}


int tun_decaps(struct tun_t *this)
{

#if defined(__linux__) || defined (__FreeBSD__) || defined (__APPLE__) || defined (__OpenBSD__) || defined (__NetBSD__)

  unsigned char buffer[PACKET_MAX];
  int status;
  
  if ((status = read(this->fd, buffer, sizeof(buffer))) <= 0) {
    log_err(errno, "read() failed");
    return -1;
  }

  if (this->debug) 
    log_dbg("tun_decaps(%d)",status);
  
   if (this->cb_ind)
#if defined (__OpenBSD__)
     /* tun interface adds 4 bytes to front of packet under OpenBSD */
     return this->cb_ind(this, buffer+4, status);
#else
     return this->cb_ind(this, buffer, status);
#endif

  return 0;

#elif defined (__sun__)

  unsigned char buffer[PACKET_MAX];
  struct strbuf sbuf;
  int f = 0;
  
  sbuf.maxlen = PACKET_MAX;      
  sbuf.buf = buffer;
  if (getmsg(this->fd, NULL, &sbuf, &f) < 0) {
    log_err(errno, "getmsg() failed");
    return -1;
  }

  if (this->cb_ind)
    return this->cb_ind(this, buffer, sbuf.len);

  return 0;
  
#endif

}


int tun_encaps(struct tun_t *tun, void *pack, unsigned len)
{
  if (options.tap) {
    struct dhcp_ethhdr_t *ethh = (struct dhcp_ethhdr_t *)pack;
    memcpy(ethh->src, options.tapmac, DHCP_ETH_ALEN);
    log_dbg("writing to tun/tap src=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x dst=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
	    ethh->src[0],ethh->src[1],ethh->src[2],ethh->src[3],ethh->src[4],ethh->src[5],
	    ethh->dst[0],ethh->dst[1],ethh->dst[2],ethh->dst[3],ethh->dst[4],ethh->dst[5]);
  } else {
    pack += DHCP_ETH_HLEN;
    len -= DHCP_ETH_HLEN;
  }

  if (tun->debug) 
    log_dbg("tun_encaps(%d)",len);

#if defined (__OpenBSD__)

  unsigned char buffer[PACKET_MAX+4];

  /* Can we user writev here to be more efficient??? */
  *((long*)(&buffer))=htonl(AF_INET);
  memcpy(&buffer[4], pack, PACKET_MAX);

  return write(tun->fd, buffer, len+4);

#elif defined(__linux__) || defined (__FreeBSD__) || defined (__APPLE__) || defined (__NetBSD__)

  return write(tun->fd, pack, len);

#elif defined (__sun__)

  struct strbuf sbuf;
  sbuf.len = len;      
  sbuf.buf = pack;
  return putmsg(tun->fd, NULL, &sbuf, 0);

#endif
}

int tun_runscript(struct tun_t *tun, char* script) {
  char saddr[TUN_ADDRSIZE];
  char smask[TUN_ADDRSIZE];
  char b[TUN_ADDRSIZE];
  struct in_addr net;
  int status;

  net.s_addr = tun->addr.s_addr & tun->netmask.s_addr;

  if ((status = fork()) < 0) {
    log_err(errno, "fork() returned -1!");
    return 0;
  }
  
  if (status > 0) { /* Parent */
    return 0;
  }
  
/*
#ifdef HAVE_CLEARENV
  if (clearenv() != 0) {
    log_err(errno,
	    "clearenv() did not return 0!");
    exit(0);
  }
#endif
*/
  
  if (setenv("DEV", tun->devname, 1) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  strncpy(saddr, inet_ntoa(tun->addr), sizeof(saddr));
  saddr[sizeof(saddr)-1] = 0;
  if (setenv("ADDR", saddr, 1 ) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  strncpy(smask, inet_ntoa(tun->netmask), sizeof(smask));
  smask[sizeof(smask)-1] = 0;
  if (setenv("MASK", smask, 1) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  strncpy(b, inet_ntoa(net), sizeof(b));
  b[sizeof(b)-1] = 0;
  if (setenv("NET", b, 1 ) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  snprintf(b, sizeof(b), "%d", options.uamport);
  if (setenv("UAMPORT", b, 1 ) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  snprintf(b, sizeof(b), "%d", options.uamuiport);
  if (setenv("UAMUIPORT", b, 1 ) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  if (setenv("DHCPIF", options.dhcpif ? options.dhcpif : "", 1 ) != 0) {
    log_err(errno, "setenv() did not return 0!");
    exit(0);
  }

  if (execl(script, script, tun->devname, saddr, smask, (char *) 0) != 0) {
    log_err(errno, "execl() did not return 0!");
    exit(0);
  }
  
  exit(0);
}
