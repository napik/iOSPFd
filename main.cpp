#include <thread>
#include <mutex>

#include "src/ospf.hpp"


using namespace std;

int main() {
    unsigned int Num_Threads = thread::hardware_concurrency();
    vector<thread> Pool;

    auto *ospf_server = new OSPF_Server();

    bool start_sender = true;
    bool start_reader = true;
    std::thread ospf_send_thread;

    if (start_sender) {
        ospf_send_thread = ospf_server->ospf_send_thread();
    }

    if (start_reader) {
        ospf_server->ospf_recv_all_ospf();
        //std::thread ospf_recv_all_ospf_thread = ospf_server->ospf_recv_all_ospf_thread();
        //std::thread ospf_recv_dd_ospf_thread = ospf_server->ospf_recv_dd_ospf_thread();

        //ospf_recv_all_ospf_thread.join();
        //ospf_recv_dd_ospf_thread.join();
    }

    if (start_sender) {
        ospf_send_thread.join();
    }
}