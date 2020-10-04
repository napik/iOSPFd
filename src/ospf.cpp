#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <thread>
#include <strings.h>
#include <cstring>
#include "ospf.hpp"
#include <iostream>

using namespace std;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

long OSPF_Server::checksum(unsigned short *data, unsigned int count) {
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
        sum += *(unsigned char *) addr;

    /*  Fold 32-bit sum to 16 bits */
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);

    return ~sum;
}

void OSPF_Server::ospf_send() {
    struct ifreq ifr, ifr_mask;
    char iface[] = "Wi-Fi";
    struct in_addr router_id;
    ssize_t result = 0;
    int sockfd = 0;
    struct sockaddr_in source_addr, dest_addr;

    sockfd = socket(AF_INET, SOCK_RAW, OSPF_PROTOCOL_NUMBER);
    if (sockfd < 0) {
        perror("socket() send");
        exit(1);
    }

    int val = 0;
    //setsockopt(sockfd, IPPROTO_IP, IP_MTU_DISCOVERY, &val, sizeof(val));

    val = 1;

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFADDR, &ifr);

    ifr_mask.ifr_addr.sa_family = AF_INET;
    strncpy(ifr_mask.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFNETMASK, &ifr_mask);

    bzero(&source_addr, sizeof(struct sockaddr_in));
    source_addr.sin_family = AF_INET;
    source_addr.sin_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr;
    router_id = source_addr.sin_addr;

    bzero(&dest_addr, sizeof(struct sockaddr_in));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(MULTICAST_AllSPFRouters);

    if (bind(sockfd, (struct sockaddr *) &source_addr, sizeof(source_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    while (true) {
        u_char ospf_data[8 * 1024];

        auto *ospf_hdr = (struct ospf_header *) &ospf_data;
        ospf_hdr->version = 2;
        ospf_hdr->type = ospf_header_type::HELLO;
        ospf_hdr->router_id = router_id;
        ospf_hdr->area_id.s_addr = inet_addr("0.0.0.0");
        ospf_hdr->checksum = 0;
        ospf_hdr->autype = 0;
        ospf_hdr->Authentication = 0;

        auto *ospf_hel = (struct ospf_hello *) &ospf_data[len_header];
        ospf_hel->NetworkMask.s_addr = ((struct sockaddr_in *) &ifr_mask.ifr_netmask)->sin_addr.s_addr;
        ospf_hel->HelloInterval = htons(10);
        ospf_hel->option = 1 << 1;
        ospf_hel->rtlpri = 1;
        ospf_hel->RouterDeadInterval = htonl(40);
        //ospf_hel->DesignatedRouter.s_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;;
        ospf_hel->DesignatedRouter.s_addr = inet_addr("0.0.0.0");
        ospf_hel->BackupDesignatedRouter.s_addr = inet_addr("0.0.0.0");
        unsigned short neighbor_count = 0;
        ospf_hel->Neighbors = nullptr;

        unsigned int len_data = sizeof(struct ospf_hello) + sizeof(struct in_addr) * neighbor_count;

        ospf_hdr->checksum = checksum((unsigned short *) ospf_data, len_data);
        ospf_hdr->plength = htons(len_data);

        printf("\n\nlen_header %lu", len_header);
        printf("\nplength %u\n\n", len_data);

        result = sendto(sockfd, &ospf_data, len_header + len_data, 0, (struct sockaddr *) &dest_addr,
                        sizeof(dest_addr));
        if (result < 0) {
            perror("send packet");
            exit(1);
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void OSPF_Server::ospf_recv_all_ospf() {

    char iface[] = "eth0";
    struct ifreq ifr;
    int yes = 1;
    int result;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_RAW, OSPF_PROTOCOL_NUMBER);
    if (sockfd < -1) {
        perror("Failed to create socket\n");
        exit(1);
    }

    result = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (result < 0) {
        perror("setsockopt() SOL_SOCKET, SO_REUSEADDR");
        exit(1);
    }

    result = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes));
    if (result < 0) {
        perror("setsockopt() IPPROTO_IP, IP_HDRINCL");
        exit(1);
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFADDR, &ifr);

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_AllSPFRouters);
    mreq.imr_interface.s_addr = ((struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr)->sin_addr.s_addr;

    result = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq));
    if (result < 0) {
        perror("setsockopt() IPPROTO_IP, IP_ADD_MEMBERSHIP");
        exit(1);
    }

    while (true) {
        perror("Connection accepted");
        connection_handler(sockfd);

//        if (pthread_create(&tid[i++], NULL, OSPF_Server::connection_handler, &newSocket) != 0) {
//            perror("could not create thread");
//            exit(1);
//        }

        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( thread_id , NULL);
        perror("Handler assigned");
    }
}

void *OSPF_Server::connection_handler(int socket_desc) {
    struct sockaddr_in sll;

    ssize_t packet_size;
    unsigned char *buffer;
    int size_buffer = 65536;
    buffer = (unsigned char *) malloc(size_buffer);
    socklen_t size_sll = sizeof(sll);

    packet_size = recvfrom(socket_desc, buffer, size_buffer, 0, (struct sockaddr *) &sll, &size_sll);
    if (packet_size < 0) {
        perror("Failed to get packets");
        exit(1);
    }

    pthread_mutex_lock(&lock);
    /*    if ((((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr).s_addr ==
        src_addr.sin_addr.s_addr) {
*/
    buffer = printOspfHeader(buffer, sll);
    printOspfData(buffer);

    pthread_mutex_unlock(&lock);
}

unsigned char *OSPF_Server::printOspfHeader(unsigned char *buffer, const sockaddr_in &sll) {
    auto *ip_packet = (struct ip *) buffer;
    buffer += sizeof(struct ip);

    printf("\n\n");

    printf("%sIncoming Packet (%s):%s\n", "\x1B[31m", (char *) inet_ntoa(sll.sin_addr), "\033[0m");
    printf("Packet Size (bytes): %d\n", ntohs(ip_packet->ip_len));
    printf("Source Address: %s\n", (char *) inet_ntoa(ip_packet->ip_src));
    printf("Destination Address: %s\n", (char *) inet_ntoa(ip_packet->ip_dst));
    printf("Identification: %d\n\n", ntohs(ip_packet->ip_id));

    return buffer;
}

void OSPF_Server::printOspfData(unsigned char *buffer) {
    auto *ospf_hdr = (struct ospf_header *) buffer;
    buffer += sizeof(struct ospf_header);

    printf("router id: %s\n", inet_ntoa(ospf_hdr->router_id));
    printf("area id: %s\n\n", inet_ntoa(ospf_hdr->area_id));

    printf("OSPF V%d ", ospf_hdr->version);
    switch (ospf_hdr->type) {
        case ospf_header_type::HELLO: {
            auto *ospf_hello = (struct ospf_hello *) buffer;
            buffer += sizeof(struct ospf_hello);

            printf("HELLO\n");
            printf("\tDR: %s, BDR: %s", inet_ntoa(ospf_hello->DesignatedRouter),
                   inet_ntoa(ospf_hello->BackupDesignatedRouter));
//            for (auto &neighbor: ospf_hello->Neighbors) {
//                printf("\tNeighbors: %s\n", inet_ntoa(neighbor));
//            }
            printf("\tHelloInterval: %hu, RouterDeadInterval: %u\n", ntohs(ospf_hello->HelloInterval),
                   ntohl(ospf_hello->RouterDeadInterval));
            printf("\toption: %hhu\n", ospf_hello->option);
            printf("\trtlpri: %hhu\n", ospf_hello->rtlpri);
            break;
        }
        case ospf_header_type::DD : {
            auto *dd_hdr = (struct ospf_dd_header *) buffer;

            printf("\x1B[33mDD\033[0m\n");
            printf("Version:\t%u\nOption:\t%u\nI:\t%s\nM:\t%s\nMS:\t%s\nSequence number:\t%u\n",
                   dd_hdr->version,
                   dd_hdr->options,
                   dd_hdr->I ? "true" : "false",
                   dd_hdr->M ? "true" : "false",
                   dd_hdr->MS ? "true" : "false",
                   dd_hdr->dd_sequence_number
            );
            break;
        }
        case ospf_header_type::LSR: {
            auto *lsa_ident = (struct ospf_lsa_identification *) buffer;

            printf("LSR\nls_type: %u\nadv_router: %s\nlink_state_id: %s\n",
                   lsa_ident->ls_type,
                   inet_ntoa(lsa_ident->adv_router),
                   inet_ntoa(lsa_ident->link_state_id)
            );
            break;
        }
        case ospf_header_type::LSU: {
            auto *lsa_update = (struct ospf_lsa_update *) buffer;
            buffer += sizeof(struct ospf_lsa_update);

            printf("LSU\n");

            unsigned int number = ntohl(lsa_update->number);
            printf("number: %u\n", number);

            for (u_int i = 0; i < number; i++) {
                auto *lsa_hdr = (struct ospf_lsa_header *) buffer;
                buffer += sizeof(struct ospf_lsa_header);

                printf("lsa_age: %d\n", ntohs(lsa_hdr->lsa_age));
                printf("ls_type: %hhu\n", lsa_hdr->ls_type);
                printf("option: %d\n", lsa_hdr->option);
                printf("adv_router: %s\n", inet_ntoa(lsa_hdr->adv_router));
                printf("checksum: %d\n", lsa_hdr->ls_checksum);
                printf("link_state_id: %s\n", inet_ntoa(lsa_hdr->link_state_id));
                printf("ls_sequence_number: %d\n", ntohs(lsa_hdr->ls_sequence_number));
                printf("length: %d\n", ntohs(lsa_hdr->length));
                buffer += ntohs(lsa_hdr->length);
            }
            break;
        }
        case ospf_header_type::LSA: {
            auto *lsa_hdr = (struct ospf_lsa_header *) buffer;

            printf("LSA\nlsa_age: %d\noption: %d\nadv_router: %s\nchecksum: %d\nlink_state_id: %s\nls_sequence_number: %d\nlenght: %d\n",
                   ntohs(lsa_hdr->lsa_age),
                   lsa_hdr->option,
                   inet_ntoa(lsa_hdr->adv_router),
                   lsa_hdr->ls_checksum,
                   inet_ntoa(lsa_hdr->link_state_id),
                   ntohs(lsa_hdr->ls_sequence_number),
                   ntohs(lsa_hdr->length)
            );

            switch (lsa_hdr->ls_type) {
                case ospf_lsa_header_ls_type::OSPF_UNKNOWN_LSA: {
                    printf("ls_type: %s\n", "UNKNOWN_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_ROUTER_LSA: {
                    printf("ls_type: %s\n", "ROUTER_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_NETWORK_LSA: {
                    printf("ls_type: %s\n", "NETWORK_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_SUMMARY_LSA: {
                    printf("ls_type: %s\n", "SUMMARY_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_ASBR_SUMMARY_LSA: {
                    printf("ls_type: %s\n", "ASBR_SUMMARY_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_AS_EXTERNAL_LSA: {
                    printf("ls_type: %s\n", "AS_EXTERNAL_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_GROUP_MEMBER_LSA: {
                    printf("ls_type: %s\n", "GROUP_MEMBER_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_AS_NSSA_LSA: {
                    printf("ls_type: %s\n", "AS_NSSA_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_EXTERNAL_ATTRIBUTES_LSA: {
                    printf("ls_type: %s\n", "EXTERNAL_ATTRIBUTES_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_OPAQUE_LINK_LSA: {
                    printf("ls_type: %s\n", "OPAQUE_LINK_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_OPAQUE_AREA_LSA: {
                    printf("ls_type: %s\n", "OPAQUE_AREA_LSA");
                    break;
                }
                case ospf_lsa_header_ls_type::OSPF_OPAQUE_AS_LSA: {
                    printf("ls_type: %s\n", "OPAQUE_AS_LSA");
                    break;
                }
            }
            break;
        }
    }
}

void OSPF_Server::ospf_recv_dd_ospf() {
    struct sockaddr_in src_addr, dst_addr;
    struct sockaddr_in sll;
    struct ifreq ifr;
    char iface[] = "eth0";
    ssize_t packet_size;
    int sockfd;
    unsigned char *buffer;

    buffer = (unsigned char *) malloc(65536);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    int yes = 1;
    int result;
    sockfd = socket(AF_INET, SOCK_RAW, OSPF_PROTOCOL_NUMBER);
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

    ioctl(sockfd, SIOCGIFADDR, &ifr);

    if (bind(sockfd, (struct sockaddr *) &ifr.ifr_addr, sizeof(struct sockaddr_in)) < 0) {
        perror("bind failed");
        exit(1);
    }

    socklen_t size = sizeof(sll);
    while (true) {
        memset(buffer, 0, 65536);
        packet_size = recvfrom(sockfd, buffer, 65536, 0, (struct sockaddr *) &sll, &size);
        if (packet_size < 0) {
            perror("Failed to get packets");
            exit(1);
        }

        auto *ip_packet = (struct ip *) buffer;

        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin_addr = ip_packet->ip_src;
        memset(&dst_addr, 0, sizeof(dst_addr));
        dst_addr.sin_addr = ip_packet->ip_dst;

        pthread_mutex_lock(&lock);
        if (src_addr.sin_addr.s_addr != (((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr).s_addr) {
            printf("ignoring self packet\n");
        }
        printf("Incoming Packet (%s): \n", (char *) inet_ntoa(sll.sin_addr));
        printf("Packet Size (bytes): %d\n", ntohs(ip_packet->ip_len));
        printf("Source Address: %s\n", (char *) inet_ntoa(src_addr.sin_addr));
        printf("Destination Address: %s\n", (char *) inet_ntoa(dst_addr.sin_addr));
        printf("Identification: %d\n\n", ntohs(ip_packet->ip_id));

        buffer += sizeof(struct ip);

        auto *ospf_hdr = (struct ospf_header *) buffer;
        printf("OSPF V%d ", ospf_hdr->version);
        printf("\trouter id: %s\n", inet_ntoa(ospf_hdr->router_id));
        printf("\tarea id: %s\n", inet_ntoa(ospf_hdr->area_id));
        switch (ospf_hdr->type) {
            case ospf_header_type::HELLO:
                printf("HELLO\n");
                break;
            case ospf_header_type::DD:
                printf("DD\n");
                break;
            case ospf_header_type::LSR:
                printf("LSR\n");
                break;
            case ospf_header_type::LSU:
                printf("LSU\n");
                break;
            case ospf_header_type::LSA:
                printf("LSA\n");
                break;
        }

        auto *ospf_hello = (struct ospf_hello *) buffer;
        printf("\tHello designated router: %s\n", inet_ntoa(ospf_hello->DesignatedRouter));
        pthread_mutex_unlock(&lock);
    }
}

std::thread OSPF_Server::ospf_send_thread() {
    return std::thread(&OSPF_Server::ospf_send, this);
}

std::thread OSPF_Server::ospf_recv_all_ospf_thread() {
    return std::thread(&OSPF_Server::ospf_recv_all_ospf, this);
}
