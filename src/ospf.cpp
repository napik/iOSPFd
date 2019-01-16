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

using namespace std;

#define LISTEN_PORT 1
// Non DR/BDR routers send on 224.0.0.6 (this is to DR's)
// and listen to 224.0.0.5 (other ospf routers)
#define MULTICAST_AllSPFRouters "224.0.0.5"
#define MULTICAST_AllDRouters "224.0.0.6"
#define OSPF_PROTOCOL_NUMBER 89

void send_ospf() {
  ssize_t result = 0;
  int sockfd = 0;

  struct sockaddr_in dest;

  sockfd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);
  // sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd < 0) {
    perror("socket() send");
    exit(1);
  }

  int on = 1;
  setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

  bzero(&dest, sizeof(struct sockaddr_in));
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = inet_addr(MULTICAST_AllSPFRouters);

  u_char *ospf_data;

  ospf_header *ospf_header = (struct ospf_header *)&ospf_data;
  ospf_header->version = 0;
  ospf_header->type = 0;
  ospf_header->plength = 0;
  ospf_header->router_id = 0;
  ospf_header->area_id = 0;
  ospf_header->checksum = 0;
  ospf_header->autype = 0;
  ospf_header->Authentication1 = 0;
  ospf_header->Authentication2 = 0;

  result = send(sockfd, &ospf_data, sizeof(ospf_data), 0);
  if (result < 0) {
    perror("send packet");
    exit(1);
  }
}

void recv_dns() {
  struct sockaddr_in addr;
  socklen_t addrlen;
  int recv_buffer_size = 1024;
  ssize_t result = 0;
  int sockfd = 0;
  const int optval = 1;
  u_char *recv_buffer;

  sockfd = socket(AF_INET, SOCK_RAW, OSPF_PROTOCOL_NUMBER);
  if (sockfd < 0) {
    perror("recv socket");
    exit(1);
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) <
      0) {
    perror("setsockopt SOL_SOCKET");
    exit(1);
  }

  bzero(&addr, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(LISTEN_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(sockfd, (sockaddr *)&addr, sizeof(struct sockaddr_in));
  listen(sockfd, 10);

  recv_buffer = (unsigned char *)malloc(recv_buffer_size);

  while (true) {
    recv_buffer_size = recvfrom(sockfd, recv_buffer, recv_buffer_size, 0,
                                (struct sockaddr *)&addr, &addrlen);
    if (result == SO_ERROR) {
      close(sockfd);
      perror("socket err");
      exit(1);
    }

    printf("\n\033[0;31mResponse from %s:\033[0m\n", inet_ntoa(addr.sin_addr));
  }
}
