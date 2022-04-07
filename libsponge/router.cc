#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // Your code here.
    uint32_t prefix = route_prefix & (((1 << prefix_length) - 1) << (32 - prefix_length));
    _mapping[prefix] = make_pair(make_pair(interface_num, prefix_length), next_hop);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Your code here.
    if(dgram.header().ttl <= 1){return;}
    auto itr = _mapping.lower_bound(dgram.header().dst);
    uint32_t mask = ~0;
    if(itr == _mapping.end() || ((mask << (32 - itr->second.first.second)) & dgram.header().dst) != itr->first){
        auto itr1 = _mapping.begin(), itr2 = _mapping.begin();
        itr1++;
        while(itr1 != _mapping.end()){
            if(((mask << (32 - itr1->second.first.second)) & dgram.header().dst) == itr1->first) {
                itr2 = itr1;
            }
            itr1++;
        }
        itr = itr2;
    }
    dgram.header().ttl--;
    Address dst = itr->second.second.has_value() ? itr->second.second->ip() :
                                                 Address::from_ipv4_numeric(dgram.header().dst);
    _interfaces[itr->second.first.first].send_datagram(dgram, dst);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
