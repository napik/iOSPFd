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

#include<netinet/ip.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <net/if.h>

#include <cstring>
#include <thread>

using namespace std;

#define LISTEN_PORT 1
// Non DR/BDR routers send on 224.0.0.6 (this is to DR's)
// and listen to 224.0.0.5 (other ospf routers)
#define MULTICAST_AllSPFRouters "224.0.0.5"
#define MULTICAST_AllDRouters "224.0.0.6"
#define OSPF_PROTOCOL_NUMBER 89

long checksum(unsigned char *addr, unsigned int count) {
/* Compute Internet Checksum for "count" bytes*  beginning at location "addr" */
   long sum = 0;
   while( count > 1 )  {
           /*  This is the inner loop */
               sum += * addr++;
               count -= 2;
   }
           /*  Add left-over byte, if any */
       if( count > 0 )
               sum += * (unsigned char *) addr;

           /*  Fold 32-bit sum to 16 bits */
       while (sum>>16)
           sum = (sum & 0xffff) + (sum >> 16);

       return ~sum;
}

void ospf_send() {
  ssize_t result = 0;
  int sockfd = 0;

  int len_hello = sizeof(struct ospf_hello);
  int len_header = sizeof(struct ospf_header);
  struct sockaddr_in source_addr, dest_addr;

while (true) {
  u_char ospf_data[8*1024];

  //sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  sockfd = socket(AF_INET, SOCK_RAW, 89);
  // sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd < 0) {
    perror("socket() send");
    exit(1);
  }

  bzero(&source_addr, sizeof(struct sockaddr_in));
  source_addr.sin_family = AF_INET;
  source_addr.sin_addr.s_addr = inet_addr("192.168.36.129");

  if (bind(sockfd, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
     perror("bind failed");
     exit(1);
  }

  bzero(&dest_addr, sizeof(struct sockaddr_in));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr.s_addr = inet_addr(MULTICAST_AllSPFRouters);
  //dest_addr.sin_addr.s_addr = inet_addr("192.168.36.2");

//  ospf_data = new u_char[64*1024];


  struct ospf_header *ospf_hdr = (struct ospf_header *)&ospf_data;
  ospf_hdr->version = 2;
  ospf_hdr->type = 1;
  ospf_hdr->router_id = inet_addr("192.168.36.129");
  ospf_hdr->area_id = inet_addr("0.0.0.0");
  ospf_hdr->checksum = 0;
  ospf_hdr->autype = 0;
  ospf_hdr->Authentication = 0;

  // ospf_data += len_header;

  struct ospf_hello *ospf_hel = (struct ospf_hello *)&ospf_data[len_header];
  ospf_hel->NetworkMask = inet_addr("255.255.255.0");
  ospf_hel->HelloInterval = htons(10);
  ospf_hel->option = 1 << 1;
  ospf_hel->rtlpri = 1;
  ospf_hel->RouterDeadInterval = htonl(40);
  ospf_hel->DesignatedRouter = inet_addr("192.168.36.1");
  ospf_hel->BackupDesignatedRouter = inet_addr("192.168.36.2");
  ospf_hel->Neighbor = inet_addr("192.168.36.2");

//  ospf_data += len_hello;

  ospf_hdr->checksum = checksum(ospf_data, len_hello);
  ospf_hdr->plength = len_header + len_hello;


  result = sendto(sockfd, &ospf_data, ospf_hdr->plength, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
  if (result < 0) {
    perror("send packet");
    exit(1);
  }

std::this_thread::sleep_for(std::chrono::seconds(10));
}
}


void ospf_recv() {
    // Structs that contain source IP addresses
    struct sockaddr_in source_socket_address, dest_socket_address;
    struct sockaddr_in source_addr;

    int packet_size;
    printf("Recv start\n");

    // Allocate string buffer to hold incoming packet data
    unsigned char *buffer = (unsigned char *)malloc(65536);
    // Open the raw socket
    int sock = socket (AF_INET, SOCK_RAW, 89);
    if(sock == -1)
    {
        //socket creation failed, may be because of non-root privileges
        perror("Failed to create socket\n");
        exit(1);
    }

  int on = 1;
  int result = setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
  if (result < 0) {
      perror("socket() recv");
      exit(1);
  }

  bzero(&source_addr, sizeof(struct sockaddr_in));
  source_addr.sin_family = AF_INET;
  source_addr.sin_addr.s_addr = inet_addr("192.168.36.129");

  if (bind(sock, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
     perror("bind failed");
     exit(1);
  }
  struct sockaddr_ll sll; 
  socklen_t size = sizeof(struct sockaddr_ll);
  char * interface;
    while(true) {
      // recvfrom is used to read data from a socket
      packet_size = recvfrom(sock , buffer , 65536 , 0 , (struct sockaddr *)&sll, &size);
      if (packet_size == -1) {
        printf("Failed to get packets\n");
        exit(1);
      }
      if_indextoname(sll.sll_ifindex, interface);

      struct iphdr *ip_packet = (struct iphdr *)buffer;

      memset(&source_socket_address, 0, sizeof(source_socket_address));
      source_socket_address.sin_addr.s_addr = ip_packet->saddr;
      memset(&dest_socket_address, 0, sizeof(dest_socket_address));
      dest_socket_address.sin_addr.s_addr = ip_packet->daddr;

      printf("Incoming Packet: \n");
      printf("Packet Size (bytes): %d\n",ntohs(ip_packet->tot_len));
      printf("Source Address: %s\n", (char *)inet_ntoa(source_socket_address.sin_addr));
      printf("Destination Address: %s\n", (char *)inet_ntoa(dest_socket_address.sin_addr));
      printf("Identification: %d\n\n", ntohs(ip_packet->id));
    }
}

