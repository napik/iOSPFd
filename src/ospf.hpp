#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <variant>
#include <vector>

using namespace std;

void ospf_send();

void ospf_recv_dospf();

void ospf_recv_allospf();

// Non DR/BDR routers send on 224.0.0.6 (this is to DR's) and listen to 224.0.0.5 (other ospf routers)
#define MULTICAST_AllSPFRouters "224.0.0.5"
#define MULTICAST_AllDRouters "224.0.0.6"
#define OSPF_PROTOCOL_NUMBER 89
#define len_hello (sizeof(struct ospf_hello))
#define len_header (sizeof(struct ospf_header))
#define len_data (len_header + len_hello)

/*
Version (1-byte)	- 2
Type    (1-byte)    - It specifies the type of OSPF packet. There are 5 different types of OSPF packets.
    1- Hello packet
    2- Database Descriptor packet
    3- Link State Request packet
    4- Link State Update packet
    5- Link State Acknowledgment packet

Packet Length   (2-bytes)                   - Total length of the OSPF packet
Router ID                                   - The Router ID of the advertising router
Area ID             	                    - 32 bit Area ID assigned to the interface sending the OSPF
Checksum        (4-bytes)                   - Standard IP Checksum of OSPF packet excluding
Authentication field AuType (2-bytes)       - Authentication Type
        0 - No Password
        1 - Plain - text password
        2 - MD5 authentication
Authentication (8-bytes)                   	- Authentication data to verify the packet's integrity
*/

/*****************************************************************
 *                         OSPF Header                           *
 *****************************************************************
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |    Version    |      Type     |        Packet length          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          Router ID                            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Area ID                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |        Checksum               |            AuType             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       Authentication                          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       Authentication                          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

/*
 * 1- Hello
 * 2- Database Descriptor packet
 * 3- Link State Request packet
 * 4- Link State Update packet
 * 5- Link State Acknowledgment packet
 */
enum class ospf_header_type : uint8_t {
    HELLO = 1, DD = 2, LSR = 3, LSU = 4, LSA = 5
};
struct ospf_header {
    uint8_t version;
    ospf_header_type type;
    uint16_t plength;
    struct in_addr router_id;
    struct in_addr area_id;
    uint16_t checksum;
    uint16_t autype;
    uint64_t Authentication;
};

/*****************************************************************
 *                        Hello Packet                           *
 *****************************************************************
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                        Network Mask                           |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |         HelloInterval         |    Options    |    Rtl Pri    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                     RouterDeadInterval                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      Designated Router                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                   Backup Designated Router                    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          Neighbor                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

struct ospf_hello {
    uint32_t NetworkMask;
    uint16_t HelloInterval;
    uint8_t option;
    uint8_t rtlpri;
    uint32_t RouterDeadInterval;
    struct in_addr DesignatedRouter;
    struct in_addr BackupDesignatedRouter;
    struct in_addr Neighbor;
};

/*****************************************************************
 *                   	     LSA Header                          *
 *****************************************************************
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |            LSA Age            |    Options    |    LS Type    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          Link state ID                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       Advertising Router                      |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                       LS Sequence Number                      |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |            LS Checksum        |             Length            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

/* OSPF LSA Type definition. */
enum class ospf_lsa_header_ls_type : uint8_t {
    OSPF_UNKNOWN_LSA = 0,
    OSPF_ROUTER_LSA = 1,
    OSPF_NETWORK_LSA = 2,
    OSPF_SUMMARY_LSA = 3,
    OSPF_ASBR_SUMMARY_LSA = 4,
    OSPF_AS_EXTERNAL_LSA = 5,
    OSPF_GROUP_MEMBER_LSA = 6, /* Not supported. */
    OSPF_AS_NSSA_LSA = 7,
    OSPF_EXTERNAL_ATTRIBUTES_LSA = 8, /* Not supported. */
    OSPF_OPAQUE_LINK_LSA = 9,
    OSPF_OPAQUE_AREA_LSA = 10,
    OSPF_OPAQUE_AS_LSA = 11,
};

#define OSPF_LSA_HEADER_SIZE 20U
#define OSPF_ROUTER_LSA_LINK_SIZE 12U
#define OSPF_MAX_LSA_SIZE 1500U

struct ospf_lsa_header {
    uint16_t lsa_age;
    uint8_t option;
    ospf_lsa_header_ls_type ls_type;
    // uint32_t link_state_id;
    struct in_addr link_state_id;
    // uint32_t adv_router;
    struct in_addr adv_router;
    uint32_t ls_sequence_numer;
    uint16_t ls_checksum;
    uint16_t lengnt;
};

/*
Router-LSAs

 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |    0    |V|E|B|        0      |            # links            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          Link ID                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                         Link Data                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     Type      |     # TOS     |            metric             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                              ...                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      TOS      |        0      |          TOS  metric          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                          Link ID                              |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                         Link Data                             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                              ...                              |


bit V
  When set, the router is an endpoint of one or more fully
  adjacent virtual links having the described area as Transit area
  (V is for virtual link endpoint).

bit E
  When set, the router is an AS boundary router (E is for external).

bit B
  When set, the router is an area border router (B is for border).

# links
  The number of router links described in this LSA.  This must be
  the total collection of router links (i.e., interfaces) to the
  area.

Type
    Type   Description
    __________________________________________________
    1      Point-to-point connection to another router
    2      Connection to a transit network
    3      Connection to a stub network
    4      Virtual link

Link ID
   Type   Link ID
   ______________________________________
   1      Neighboring router's Router ID
   2      IP address of Designated Router
   3      IP network/subnet number
   4      Neighboring router's Router ID

Link Data
    # TOS
        The number of different TOS metrics given for this link, not
        counting the required link metric (referred to as the TOS 0
        metric in [Ref9]).  For example, if no additional TOS metrics
        are given, this field is set to 0.

    metric
        The cost of using this router link.
*/

/* OSPF LSA Link Type. */
#define LSA_LINK_TYPE_POINTOPOINT 1
#define LSA_LINK_TYPE_TRANSIT 2
#define LSA_LINK_TYPE_STUB 3
#define LSA_LINK_TYPE_VIRTUALLINK 4

/* OSPF Router LSA Flag. */
#define ROUTER_LSA_BORDER 0x01   /* The router is an ABR */
#define ROUTER_LSA_EXTERNAL 0x02 /* The router is an ASBR */
#define ROUTER_LSA_VIRTUAL 0x04  /* The router has a VL in this area */
#define ROUTER_LSA_NT 0x10       /* The routers always translates Type-7 */
#define ROUTER_LSA_SHORTCUT 0x20 /* Shortcut-ABR specific flag */

#define IS_ROUTER_LSA_VIRTUAL(x) ((x)->flags & ROUTER_LSA_VIRTUAL)
#define IS_ROUTER_LSA_EXTERNAL(x) ((x)->flags & ROUTER_LSA_EXTERNAL)
#define IS_ROUTER_LSA_BORDER(x) ((x)->flags & ROUTER_LSA_BORDER)
#define IS_ROUTER_LSA_SHORTCUT(x) ((x)->flags & ROUTER_LSA_SHORTCUT)
#define IS_ROUTER_LSA_NT(x) ((x)->flags & ROUTER_LSA_NT)

/* OSPF Router-LSA Link information. */
struct router_lsa_link {
    struct in_addr link_id;
    struct in_addr link_data;
    struct {
        u_char type;
        u_char tos_count;
        u_int16_t metric;
    } m[1];
};

/* OSPF Router-LSAs structure. */
struct router_lsa {
    struct ospf_lsa_header header;
    u_char flags;
    u_char zero;
    u_int16_t links;
    struct {
        struct in_addr link_id;
        struct in_addr link_data;
        u_char type;
        u_char tos;
        u_int16_t metric;
    } link[1];
};

/* OSPF Network-LSAs structure. */
struct network_lsa {
    struct ospf_lsa_header header;
    struct in_addr mask;
    struct in_addr routers[1];
};

/* OSPF Summary-LSAs structure. */
struct summary_lsa {
    struct ospf_lsa_header header;
    struct in_addr mask;
    u_char tos;
    u_char metric[3];
};

/* OSPF AS-external-LSAs structure. */
struct as_external_lsa {
    struct ospf_lsa_header header;
    struct in_addr mask;
    struct {
        u_char tos;
        u_char metric[3];
        struct in_addr fwd_addr;
        u_int32_t route_tag;
    } e[1];
};

/*****************************************************************
 *                     Database Descriptor                       *
 *****************************************************************
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |       Interface MTU           |    Options    |0|0|0|0|0|I|M|MS
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                     DD sequence number                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

struct ospf_lsa_identification {
    uint32_t ls_type;
    uint32_t link_state_id;
    uint32_t adv_router;
};

struct ospf_lsa_update {
    uint32_t number;
};

struct ospf_dd_header {
    uint16_t version;
    uint8_t options;
    u_char u1;
    u_char u2;
    u_char u3;
    u_char u4;
    u_char u5;
    u_char I;
    u_char M;
    u_char MS;
    uint32_t dd_sequence_number;
};

struct ospf_dd {
    struct ospf_dd_header header;
    /*
     * An LSA Header
     */
    u_char *LSA_header;
};
