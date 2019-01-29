#include "inc/ospf.hpp"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <variant>
#include <vector>

#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <cstring>
#include <thread>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

long checksum(unsigned short *data, unsigned int count)
{
  unsigned short *addr = data;
  /*
   * Compute Internet Checksum for "count" bytes*  beginning at location "addr"
   */
  long sum = 0;
  while (count > 1) {
    /*  This is the inner loop */
    sum += *addr++;
    count -= 2;
  }
  /*  Add left-over byte, if any */
  if (count > 0)
    sum += *(unsigned char *)addr;

  /*  Fold 32-bit sum to 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);

  return ~sum;
}

void ospf_send()
{
  struct ifreq ifr;
  char iface[] = "eth0";
  struct in_addr router_id;
  ssize_t result = 0;
  int sockfd = 0;
  struct sockaddr_in source_addr, dest_addr;

  sockfd = socket(AF_INET, SOCK_RAW, 89);
  if (sockfd < 0) {
    perror("socket() send");
    exit(1);
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  ioctl(sockfd, SIOCGIFADDR, &ifr);

  bzero(&source_addr, sizeof(struct sockaddr_in));
  source_addr.sin_family = AF_INET;
  source_addr.sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
  router_id = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

  bzero(&dest_addr, sizeof(struct sockaddr_in));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr.s_addr = inet_addr(MULTICAST_AllSPFRouters);

  if (bind(sockfd, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
    perror("bind failed");
    exit(1);
  }

  while (true) {
    u_char ospf_data[8 * 1024];

    struct ospf_header *ospf_hdr = (struct ospf_header *)&ospf_data;
    ospf_hdr->version = 2;
    ospf_hdr->type = 1;
    ospf_hdr->router_id = router_id.s_addr;
    ospf_hdr->area_id = inet_addr("0.0.0.0");
    ospf_hdr->checksum = 0;
    ospf_hdr->autype = 0;
    ospf_hdr->Authentication = 0;

    struct ospf_hello *ospf_hel = (struct ospf_hello *)&ospf_data[len_header];
    ospf_hel->NetworkMask = inet_addr("255.255.255.0");
    ospf_hel->HelloInterval = htons(10);
    ospf_hel->option = 1 << 1;
    ospf_hel->rtlpri = 1;
    ospf_hel->RouterDeadInterval = htonl(40);
    ospf_hel->DesignatedRouter = inet_addr("192.168.36.1");
    ospf_hel->BackupDesignatedRouter = inet_addr("192.168.36.2");
    ospf_hel->Neighbor = inet_addr("192.168.36.2");

    ospf_hdr->checksum = checksum((unsigned short *)ospf_data, len_data);
    ospf_hdr->plength = htons(len_data);

    result =
      sendto(sockfd, &ospf_data, len_data, 0,
	     (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (result < 0) {
      perror("send packet");
      exit(1);
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

void ospf_recv_allospf()
{
  char iface[] = "eth0";
  struct sockaddr_in source_socket_address, dest_socket_address;
  struct sockaddr_ll sll;
  struct ifreq ifr;
  struct ip_mreq mreq;
  struct in_addr tmp_ip_addr;
  int packet_size;
  int yes = 1;
  int result;
  int sockfd;
  char interface[15];
  unsigned char *buffer;

  buffer = (unsigned char *)malloc(65536);

  sockfd = socket(AF_INET, SOCK_RAW, 89);
  if (sockfd == -1) {
    perror("Failed to create socket\n");
    exit(1);
  }

  result = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes));
  if (result < 0) {
    perror("setsockopt() IPPROTO_IP, IP_HDRINCL");
    exit(1);
  }

  result = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (result < 0) {
    perror("setsockopt() SOL_SOCKET, SO_REUSEADDR");
    exit(1);
  }

  int size = sizeof(struct sockaddr_ll);

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  ioctl(sockfd, SIOCGIFADDR, &ifr);
  mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_AllSPFRouters);
  memcpy(&mreq.imr_interface,
	 &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,
	 sizeof(struct in_addr));
  result =
    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
	       sizeof(mreq));
  if (result < 0) {
    perror("setsockopt");
    exit(1);
  }

  while (true) {
    packet_size =
      recvfrom(sockfd, buffer, 65536, 0, (struct sockaddr *)&sll, &size);
    if (packet_size < 0) {
      perror("Failed to get packets");
      exit(1);
    }

    struct iphdr *ip_packet = (struct iphdr *)buffer;

    memset(&source_socket_address, 0, sizeof(source_socket_address));
    source_socket_address.sin_addr.s_addr = ip_packet->saddr;
    memset(&dest_socket_address, 0, sizeof(dest_socket_address));
    dest_socket_address.sin_addr.s_addr = ip_packet->daddr;

    if (source_socket_address.sin_addr.s_addr == (((struct sockaddr_in *)
						   &ifr.ifr_addr)
						  ->sin_addr).s_addr) {
      if_indextoname(sll.sll_ifindex, interface);

      printf("Incoming Packet (%s): \n", interface);
      printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
      printf("Source Address: %s\n", (char *)
	     inet_ntoa(source_socket_address.sin_addr));
      printf("Destination Address: %s\n",
	     (char *)inet_ntoa(dest_socket_address.sin_addr));
      printf("Identification: %d\n\n", ntohs(ip_packet->id));

      buffer += sizeof(struct iphdr);

      struct ospf_header *ospf_hdr = (struct ospf_header *)buffer;
      printf("OSPF ");
      printf("V%d ", ospf_hdr->version);
      switch (ospf_hdr->type) {
      case 1:
	printf("HELLO\n");
	break;
      }

      tmp_ip_addr.s_addr = ospf_hdr->router_id;
      printf("router id: %s\n", inet_ntoa(tmp_ip_addr));
      tmp_ip_addr.s_addr = ospf_hdr->area_id;
      printf("area id: %s\n\n", inet_ntoa(tmp_ip_addr));
    } else {
      //      printf("ignoring self packet\n");
    }
  }
}

void ospf_recv_dospf()
{
  struct sockaddr_in source_socket_address, dest_socket_address;
  struct sockaddr_ll sll;
  struct ifreq ifr;
  char iface[] = "eth0";
  struct ip_mreq mreq;

  int packet_size;

  int yes = 1;
  int result;
  int sockfd;
  char *interface;
  unsigned char *buffer;
  struct in_addr tmp_ip_addr;

  buffer = (unsigned char *)malloc(65536);

  sockfd = socket(AF_INET, SOCK_RAW, 89);
  if (sockfd == -1) {
    perror("Failed to create socket\n");
    exit(1);
  }

  result = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes));
  if (result < 0) {
    perror("setsockopt() IPPROTO_IP, IP_HDRINCL");
    exit(1);
  }

  result = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (result < 0) {
    perror("setsockopt() SOL_SOCKET, SO_REUSEADDR");
    exit(1);
  }

  socklen_t size = sizeof(struct sockaddr_ll);

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  ioctl(sockfd, SIOCGIFADDR, &ifr);
  mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_AllDRouters);
  memcpy(&mreq.imr_interface,
	 &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,
	 sizeof(struct in_addr));
  result =
    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
	       sizeof(mreq));
  if (result < 0) {
    perror("setsockopt");
    exit(1);
  }

  while (true) {
    packet_size =
      recvfrom(sockfd, buffer, 65536, 0, (struct sockaddr *)&sll, &size);
    if (packet_size < 0) {
      perror("Failed to get packets");
      exit(1);
    }

    struct iphdr *ip_packet = (struct iphdr *)buffer;

    memset(&source_socket_address, 0, sizeof(source_socket_address));
    source_socket_address.sin_addr.s_addr = ip_packet->saddr;
    memset(&dest_socket_address, 0, sizeof(dest_socket_address));
    dest_socket_address.sin_addr.s_addr = ip_packet->daddr;

    if (source_socket_address.sin_addr.s_addr == (((struct sockaddr_in *)
						   &ifr.ifr_addr)
						  ->sin_addr).s_addr) {

      if_indextoname(sll.sll_ifindex, interface);

      printf("Incoming Packet (%s): \n", interface);
      printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
      printf("Source Address: %s\n", (char *)
	     inet_ntoa(source_socket_address.sin_addr));
      printf("Destination Address: %s\n",
	     (char *)inet_ntoa(dest_socket_address.sin_addr));
      printf("Identification: %d\n\n", ntohs(ip_packet->id));

      buffer += sizeof(struct iphdr);

      struct ospf_header *ospf_hdr = (struct ospf_header *)buffer;
      printf("OSPF ");
      printf("V%d ", ospf_hdr->version);
      switch (ospf_hdr->type) {
      case 1:
	printf("HELLO\n");
	break;
      }

      tmp_ip_addr.s_addr = ospf_hdr->router_id;
      printf("router id: %s\n", inet_ntoa(tmp_ip_addr));
      tmp_ip_addr.s_addr = ospf_hdr->area_id;
      printf("area id: %s\n\n", inet_ntoa(tmp_ip_addr));
    } else {
      //      printf("ignoring self packet\n");
    }
  }
}
