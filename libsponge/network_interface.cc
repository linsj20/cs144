#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame ef;
    ef.header().src = _ethernet_address;
    if(_mapping.find(next_hop_ip) != _mapping.end()){
        ef.header().dst = _mapping[next_hop_ip].second;
        ef.header().type = EthernetHeader::TYPE_IPv4;
        ef.payload() = dgram.serialize();
        _frames_out.push(ef);
        return;
    }
    bool first = false;
    if(_waiting.find(next_hop_ip) == _waiting.end()){
        first = true;
        _waiting[next_hop_ip].first = _time;
    }
    _waiting[next_hop_ip].second.push_back(dgram);
    if(first || _time - _waiting[next_hop_ip].first > 5000) {
        send_arp(1, ETHERNET_BROADCAST, next_hop_ip);
        _waiting[next_hop_ip].first = _time;
    }
}

void NetworkInterface::send_arp(uint16_t opcode, EthernetAddress dst, uint32_t dst_ip) {
    ARPMessage arp;
    EthernetFrame frame;
    arp.opcode = opcode;
    arp.sender_ethernet_address = _ethernet_address;
    arp.sender_ip_address = _ip_address.ipv4_numeric();
    if (opcode != ARPMessage::OPCODE_REQUEST) {
        arp.target_ethernet_address = dst;
    }
    arp.target_ip_address = dst_ip;
    frame.header().src = _ethernet_address;
    frame.header().dst = dst;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.payload() = arp.serialize();
    _frames_out.push(std::move(frame));
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().type == EthernetHeader::TYPE_IPv4 && frame.header().dst == _ethernet_address){
        InternetDatagram dgram;
        if(dgram.parse(frame.payload()) == ParseResult::NoError){return dgram;}
    }
    else if (frame.header().type == EthernetHeader::TYPE_ARP){
        ARPMessage arp;
        if(arp.parse(frame.payload()) == ParseResult::NoError){
            _mapping[arp.sender_ip_address] = make_pair(_time, arp.sender_ethernet_address);
            if(_waiting.find(arp.sender_ip_address) != _waiting.end()){
                auto itr = _waiting[arp.sender_ip_address].second.begin();
                while(itr != _waiting[arp.sender_ip_address].second.end()) {
                    EthernetFrame ef;
                    ef.header().type = EthernetHeader::TYPE_IPv4;
                    ef.header().src = _ethernet_address;
                    ef.header().dst = arp.sender_ethernet_address;
                    ef.payload().append(itr->serialize());
                    _frames_out.push(ef);
                    itr++;
                }
                _waiting.erase(arp.sender_ip_address);
            }
            if(arp.opcode == 1 && arp.target_ip_address == _ip_address.ipv4_numeric()){
                send_arp(2, arp.sender_ethernet_address, arp.sender_ip_address);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    queue<uint32_t> q;
    _time += ms_since_last_tick;
    auto itr = _mapping.begin();
    while(itr != _mapping.end()){
        if(_time - itr->second.first > 30000){
            q.push(itr->first);
        }
        itr++;
    }
    while(!q.empty()){
        _mapping.erase(q.front());
        q.pop();
    }
}
