#include "inc/ospf.hpp"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <thread>
#include <variant>
#include <vector>

using namespace std;

int main() {
  std::thread thr_send = std::thread(ospf_send);
  std::thread thr_recv = std::thread(ospf_recv);
  thr_send.join();
  thr_recv.join();
}
