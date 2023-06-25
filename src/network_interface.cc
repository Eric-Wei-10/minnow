#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto cache_it = arp_cache_.find(next_hop.ipv4_numeric());
  auto waitlist_it = arp_waitlist_.find(next_hop.ipv4_numeric());

  if (cache_it != arp_cache_.end()) {
    // If the host doesn't know target MAC address, send ARP message.
    if (waitlist_it == arp_waitlist_.end()) {
      // No waitlist item indicates that no ARP request recently.
      // Create item, push dgram and send ARP message.

    } else {
      // If there is ARP request recently, push dgram into 'waitings'.

      if (waitlist_it->second.timer > NetworkInterface::ARP_INTERVAL) {
        // If ARP request is more than 5s ago, retx it.
      }
    }
  } else {
    // If the host knows target MAC address, send dgram.
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if (!(frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST)) {
    // Destination of frame is not this host, ignore it.
    return nullopt;
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    // Payload is IPv4.
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) {
      // If no error, return dgram.
      return dgram;
    }
  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp;
    if (parse(arp, frame.payload)) {
      // Insert or update cache item and clean waitlist.

      if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric()) {
        // Send ARP reply.
      }
    }
  }

  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for (auto cache_it = arp_cache_.begin(); cache_it != arp_cache_.end(); ) {
    // Iterately decrease ttl.
    if (cache_it->second.ttl < ms_since_last_tick) {
      // If ttl timeout, remove item.
      auto tmp = cache_it;
      cache_it++;
      arp_cache_.erase(tmp);
    } else {
      // If not timeout, decrease ttl.
      cache_it->second.ttl -= ms_since_last_tick;
      cache_it++;
    }
  }

  for (auto waitlist_it = arp_waitlist_.begin(); waitlist_it != arp_waitlist_.end(); waitlist_it++) {
    // Iterately increase timer.
    waitlist_it->second.timer += ms_since_last_tick;
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (frames_out_.empty()) {
    return nullopt;
  } else {
    EthernetFrame eframe = frames_out_.front();
    frames_out_.pop();
    return eframe;
  }
}
