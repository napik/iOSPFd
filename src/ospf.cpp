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

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

long checksum(unsigned short *data, unsigned int count) {
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

void ospf_send() {
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
  router_id = source_addr.sin_addr;

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

    result = sendto(sockfd, &ospf_data, len_data, 0,
                    (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (result < 0) {
      perror("send packet");
      exit(1);
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

void ospf_recv_allospf() {
  struct sockaddr_in src_addr, dst_addr;
  struct sockaddr_in sll;
  /*  char iface[] = "eth0";
    struct ifreq ifr;
    struct ip_mreq mreq;
  */
  struct in_addr tmp_ip_addr;
  int packet_size;
  int yes = 1;
  int result;
  int sockfd;
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

  /*  ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFADDR, &ifr);
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_AllSPFRouters);
    mreq.imr_interface.s_addr = ((struct sockaddr_in
    *)&ifr.ifr_ifru.ifru_addr)->sin_addr.s_addr;

    result =
      setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                 sizeof(mreq));
    if (result < 0) {
      perror("setsockopt() IPPROTO_IP, IP_ADD_MEMBERSHIP");
      exit(1);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_AllDRouters);
    result =
      setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                 sizeof(mreq));
    if (result < 0) {
      perror("setsockopt() IPPROTO_IP, IP_ADD_MEMBERSHIP");
      exit(1);
    }
  */
  socklen_t size = sizeof(sll);
  while (true) {
    memset(buffer, 0, 65536);
    packet_size =
        recvfrom(sockfd, buffer, 65536, 0, (struct sockaddr *)&sll, &size);

    if (packet_size < 0) {
      perror("Failed to get packets");
      exit(1);
    }

    struct iphdr *ip_packet = (struct iphdr *)buffer;
    buffer += sizeof(struct iphdr);

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_addr.s_addr = ip_packet->saddr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_addr.s_addr = ip_packet->daddr;

    pthread_mutex_lock(&lock);
    /*    if ((((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr).s_addr ==
            src_addr.sin_addr.s_addr) {
    */
    printf("\n\n\n\x1B[31mIncoming Packet (%s):\033[0m \n",
           (char *)inet_ntoa(sll.sin_addr));

    printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
    printf("Source Address: %s\n", (char *)inet_ntoa(src_addr.sin_addr));
    printf("Destination Address: %s\n", (char *)inet_ntoa(dst_addr.sin_addr));
    printf("Identification: %d\n\n", ntohs(ip_packet->id));

    struct ospf_header *ospf_hdr = (struct ospf_header *)buffer;
    buffer += sizeof(struct ospf_header);

    tmp_ip_addr.s_addr = ospf_hdr->router_id;
    printf("router id: %s\n", inet_ntoa(tmp_ip_addr));
    tmp_ip_addr.s_addr = ospf_hdr->area_id;
    printf("area id: %s\n\n", inet_ntoa(tmp_ip_addr));

    printf("OSPF V%d ", ospf_hdr->version);
    switch (ospf_hdr->type) {
      /*         2- Database Descriptor packet 3- Link State Request packet 4-
       * Link State Update packet 5- Link State Acknowledgment packet
       */
    case 1: {
      printf("HELLO\n");
      break;
    }
    case 2: {
      printf("\x1B[33mDD\033[0m\n");
      struct ospf_dd_header *dd_hdr = (struct ospf_dd_header *)buffer;
      printf("Version: %u\n", dd_hdr->version);
      printf("Option: %u\n", dd_hdr->options);
      printf("I:  %s\n", dd_hdr->I ? "true" : "false");
      printf("M:  %s\n", dd_hdr->M ? "true" : "false");
      printf("MS: %s\n", dd_hdr->MS ? "true" : "false");
      printf("dd_sequence_number: %u\n", dd_hdr->dd_sequence_number);
      break;
    }
    case 3: {
      printf("LSR\n");
      struct ospf_lsa_identification *lsa_ident =
          (struct ospf_lsa_identification *)buffer;
      printf("ls_type: %u\n", lsa_ident->ls_type);
      tmp_ip_addr.s_addr = lsa_ident->adv_router;
      printf("adv_router: %s\n", inet_ntoa(tmp_ip_addr));
      tmp_ip_addr.s_addr = lsa_ident->link_state_id;
      printf("link_state_id: %s\n", inet_ntoa(tmp_ip_addr));
      break;
    }
    case 4: {
      printf("LSU\n");
      struct ospf_lsa_update *lsa_update = (struct ospf_lsa_update *)buffer;
      buffer += sizeof(struct ospf_lsa_update);

      unsigned int number = ntohl(lsa_update->number);
      printf("number: %u\n", number);
      for (u_int i = 0; i < number; i++) {
        struct ospf_lsa_header *lsa_hdr = (struct ospf_lsa_header *)buffer;
        buffer += sizeof(struct ospf_lsa_header);

        printf("\n\nlsa_age: %d\n", ntohs(lsa_hdr->lsa_age));
        printf("ls_type: %d\n", lsa_hdr->ls_type);
        printf("option: %d\n", lsa_hdr->option);
        printf("adv_router: %s\n", inet_ntoa(lsa_hdr->adv_router));
        printf("checksum: %d\n", lsa_hdr->ls_checksum);
        printf("link_state_id: %s\n", inet_ntoa(lsa_hdr->link_state_id));
        printf("ls_sequence_numer: %d\n", ntohs(lsa_hdr->ls_sequence_numer));
        printf("lenght: %d\n", ntohs(lsa_hdr->lengnt));
        buffer += ntohs(lsa_hdr->lengnt);
      }
      break;
    }
    case 5: {
      printf("LSA\n");

      struct ospf_lsa_header *lsa_hdr = (struct ospf_lsa_header *)buffer;
      printf("lsa_age: %d\n", ntohs(lsa_hdr->lsa_age));
      printf("option: %d\n", lsa_hdr->option);
      printf("adv_router: %s\n", inet_ntoa(lsa_hdr->adv_router));
      printf("checksum: %d\n", lsa_hdr->ls_checksum);
      printf("link_state_id: %s\n", inet_ntoa(lsa_hdr->link_state_id));
      printf("ls_sequence_numer: %d\n", ntohs(lsa_hdr->ls_sequence_numer));
      printf("lenght: %d\n", ntohs(lsa_hdr->lengnt));

      switch (lsa_hdr->ls_type) {
      case OSPF_UNKNOWN_LSA: {
        printf("ls_type: %s\n", "UNKNOWN_LSA");
        break;
      }
      case OSPF_ROUTER_LSA: {
        printf("ls_type: %s\n", "ROUTER_LSA");
        break;
      }
      case OSPF_NETWORK_LSA: {
        printf("ls_type: %s\n", "NETWORK_LSA");
        break;
      }
      case OSPF_SUMMARY_LSA: {
        printf("ls_type: %s\n", "SUMMARY_LSA");
        break;
      }
      case OSPF_ASBR_SUMMARY_LSA: {
        printf("ls_type: %s\n", "ASBR_SUMMARY_LSA");
        break;
      }
      case OSPF_AS_EXTERNAL_LSA: {
        printf("ls_type: %s\n", "AS_EXTERNAL_LSA");
        break;
      }
      case OSPF_GROUP_MEMBER_LSA: {
        printf("ls_type: %s\n", "GROUP_MEMBER_LSA");
        break;
      }
      case OSPF_AS_NSSA_LSA: {
        printf("ls_type: %s\n", "AS_NSSA_LSA");
        break;
      }
      case OSPF_EXTERNAL_ATTRIBUTES_LSA: {
        printf("ls_type: %s\n", "EXTERNAL_ATTRIBUTES_LSA");
        break;
      }
      case OSPF_OPAQUE_LINK_LSA: {
        printf("ls_type: %s\n", "OPAQUE_LINK_LSA");
        break;
      }
      case OSPF_OPAQUE_AREA_LSA: {
        printf("ls_type: %s\n", "OPAQUE_AREA_LSA");
        break;
      }
      case OSPF_OPAQUE_AS_LSA: {
        printf("ls_type: %s\n", "OPAQUE_AS_LSA");
        break;
      }
      }
      break;
    }
    }

    /*    } else {
          printf("\nignoring self packet\n");
        }
    */
    pthread_mutex_unlock(&lock);
  }
}

void ospf_recv_dospf() {
  struct sockaddr_in src_addr, dst_addr;
  struct sockaddr_in sll;
  struct ifreq ifr;
  char iface[] = "eth0";
  int packet_size;
  int yes = 1;
  int result;
  int sockfd;
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

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
  ioctl(sockfd, SIOCGIFADDR, &ifr);

  if (bind(sockfd, (struct sockaddr *)&ifr.ifr_addr,
           sizeof(struct sockaddr_in)) < 0) {
    perror("bind failed");
    exit(1);
  }

  socklen_t size = sizeof(sll);
  while (true) {
    memset(buffer, 0, 65536);
    packet_size =
        recvfrom(sockfd, buffer, 65536, 0, (struct sockaddr *)&sll, &size);
    if (packet_size < 0) {
      perror("Failed to get packets");
      exit(1);
    }

    struct iphdr *ip_packet = (struct iphdr *)buffer;

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_addr.s_addr = ip_packet->saddr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_addr.s_addr = ip_packet->daddr;

    pthread_mutex_lock(&lock);
    /*    if (src_addr.sin_addr.s_addr == (((struct sockaddr_in *)
                                          &ifr.ifr_addr)
                                         ->sin_addr).s_addr) {
    */
    printf("Incoming Packet (%s): \n", (char *)inet_ntoa(sll.sin_addr));
    printf("Packet Size (bytes): %d\n", ntohs(ip_packet->tot_len));
    printf("Source Address: %s\n", (char *)inet_ntoa(src_addr.sin_addr));
    printf("Destination Address: %s\n", (char *)inet_ntoa(dst_addr.sin_addr));
    printf("Identification: %d\n\n", ntohs(ip_packet->id));

    buffer += sizeof(struct iphdr);

    struct ospf_header *ospf_hdr = (struct ospf_header *)buffer;
    printf("OSPF V%d ", ospf_hdr->version);
    switch (ospf_hdr->type) {
    case 1:
      printf("HELLO\n");
      break;
    }

    tmp_ip_addr.s_addr = ospf_hdr->router_id;
    printf("router id: %s\n", inet_ntoa(tmp_ip_addr));
    tmp_ip_addr.s_addr = ospf_hdr->area_id;
    printf("area id: %s\n\n", inet_ntoa(tmp_ip_addr));
    /*    } else {
          printf("ignoring self packet\n");
        }
    */
    pthread_mutex_unlock(&lock);
  }
}
