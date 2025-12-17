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

    if (_arp_cache.count(next_hop_ip)) {
        EthernetFrame frame;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.header().src = _ethernet_address;
        frame.header().dst = _arp_cache[next_hop_ip].mac;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    } else {
        if (_waiting_arp_timer.find(next_hop_ip) == _waiting_arp_timer.end()) {
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;

            EthernetFrame frame;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.payload() = arp_request.serialize();
            _frames_out.push(frame);

            _waiting_arp_timer[next_hop_ip] = 0;
        }
        _waiting_arp_response[next_hop_ip].push_back(dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return {};
    }

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) == ParseResult::NoError) {
            const uint32_t sender_ip = arp_msg.sender_ip_address;
            const EthernetAddress sender_mac = arp_msg.sender_ethernet_address;

            _arp_cache[sender_ip] = {sender_mac, 0};

            if (_waiting_arp_response.count(sender_ip)) {
                for (const auto &dgram : _waiting_arp_response[sender_ip]) {
                    send_datagram(dgram, Address::from_ipv4_numeric(sender_ip));
                }
                _waiting_arp_response.erase(sender_ip);
                _waiting_arp_timer.erase(sender_ip);
            }

            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST &&
                arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ethernet_address = sender_mac;
                arp_reply.target_ip_address = sender_ip;

                EthernetFrame reply_frame;
                reply_frame.header().type = EthernetHeader::TYPE_ARP;
                reply_frame.header().src = _ethernet_address;
                reply_frame.header().dst = sender_mac;
                reply_frame.payload() = arp_reply.serialize();
                _frames_out.push(reply_frame);
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto it = _arp_cache.begin(); it != _arp_cache.end();) {
        it->second.ttl += ms_since_last_tick;
        if (it->second.ttl >= ARP_ENTRY_TTL) {
            it = _arp_cache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = _waiting_arp_timer.begin(); it != _waiting_arp_timer.end();) {
        it->second += ms_since_last_tick;
        if (it->second >= ARP_RESPONSE_TTL) {
            it = _waiting_arp_timer.erase(it);
        } else {
            ++it;
        }
    }
}
