#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( uint32_t route_prefix,
                        uint8_t prefix_length,
                        optional<Address> next_hop,
                        size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  TableEntry entry;
  entry.route_prefix_ = route_prefix;
  entry.prefix_length_ = prefix_length;
  entry.next_hop_ = next_hop;
  entry.interface_num_ = interface_num;
  routing_table_.push_back( entry );
}

void Router::route_one_datagram( InternetDatagram& dgram )
{
  // Check TTL.
  if ( dgram.header.ttl <= 1 )
    return;

  // Longest prefix match.
  uint32_t destination = dgram.header.dst;
  int match_idx = -1;
  int max_matched_len = -1;
  for ( size_t i = 0; i < routing_table_.size(); i++ ) {
    auto mask = routing_table_[i].prefix_length_ == 0
                  ? 0
                  : numeric_limits<int>::min() >> ( routing_table_[i].prefix_length_ - 1 );
    if ( ( destination & mask ) == routing_table_[i].route_prefix_
         && max_matched_len < routing_table_[i].prefix_length_ ) {
      match_idx = i;
      max_matched_len = routing_table_[i].prefix_length_;
    }
  }

  // If no match, drop the datagram.
  if ( match_idx == -1 )
    return;

  // Decrement TTL and update checksum.
  dgram.header.ttl -= 1;
  dgram.header.compute_checksum();

  // send the datagram
  auto next_hop = routing_table_[match_idx].next_hop_;
  auto interface_num = routing_table_[match_idx].interface_num_;
  if ( next_hop.has_value() ) {
    interfaces_[interface_num].send_datagram( dgram, next_hop.value() );
  } else {
    interfaces_[interface_num].send_datagram( dgram, Address::from_ipv4_numeric( destination ) );
  }
}

void Router::route()
{
  // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
  for ( auto& interface : interfaces_ ) {
    optional<InternetDatagram> maybe_dgram;
    while ( ( maybe_dgram = interface.maybe_receive() ) != nullopt ) {
      route_one_datagram( maybe_dgram.value() );
    }
  }
}
