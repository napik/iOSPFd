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

/*

Version		- 2 (1-byte)
Type		- It specifies the type of OSPF packet. There are 5 different
types of OSPF packets. (1-byte) 1- Hello packet 2- Database Descriptor packet 3-
Link State Request packet 4- Link State Update packet 5- Link State
Acknowledgment packet

Packet Length	 - Total length of the OSPF packet(2 - bytes)
Router ID	 - The Router ID of the advertising router
Area ID		 - 32 bit Area ID assigned to the interface sending the OSPF
packet(4 - bytes) Checksum	 - Standard IP Checksum of OSPF packet excluding
Authentication field(2 - bytes) AuType		 - Authentication Type(2 -
bytes) 0 - No Password 1 - Plain - text password 2 - MD5 authentication
Authentication	 -  Authentication data to verify the packet's integrity
(8-bytes)
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

struct class ospf_header {
  uint8_t version;
  uint8_t type;
  uint16_t plength;
  uint32_t router_id;
  uint32_t router_id;
  uint16_t checksum;
  uint16_t autype;
  uint32_t Authentication1;
  uint32_t Authentication2;
}

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

struct class ospf_hello {
  uint32_t NetworkMask;
  uint16_t HelloInterval;
  uint8_t option;
  uint8_t rtlpri;
  uint32_t RouterDeadInterval;
  uint32_t DesignatedRouter;
  uint32_t BackupDesignatedRouter;
  uint32_t Neighbor;
}

/*****************************************************************
 *                     Database Descriptor                       *
 *****************************************************************
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |       Interface MTU           |    Options    |0|0|0|0|0|I|M|MS
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                     DD sequence number                        |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                                                               |
 +-                                                             -+
 |                         An LSA Header                         |
 +-                                                             -+
 |                                                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

struct class ospf_header {
  uint16_t version;
  uint8_t options;
  u_char 0;
  u_char 0;
  u_char 0;
  u_char 0;
  u_char 0;
  u_char I;
  u_char M;
  u_char MS;
  uint32_t dd_sequence_number;
  /*
   * An LSA Header
   */
}
