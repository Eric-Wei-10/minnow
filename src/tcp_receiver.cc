#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  // If SYN is not received and the incoming message doesn't contain SYN, return.
  if ( !syn_rcvd_ && !message.SYN )
    return;

  // Get checkpoint.
  uint64_t checkpoint = syn_rcvd_ + inbound_stream.bytes_pushed();

  // If SYN is contained in the message, initialize TCPReceiver's seqno.
  if ( !syn_rcvd_ && message.SYN ) {
    syn_rcvd_ = true;
    zero_point_ = message.seqno;
  }

  string data = message.payload;
  uint64_t first_index = message.seqno.unwrap( zero_point_.value(), checkpoint ) - ( !message.SYN );
  reassembler.insert( first_index, data, message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  TCPReceiverMessage message;
  message.ackno = ackno( inbound_stream );
  message.window_size = window_size( inbound_stream );
  return message;
}

optional<Wrap32> TCPReceiver::ackno( const Writer& inbound_stream ) const
{
  if ( !zero_point_.has_value() )
    return nullopt;
  else {
    uint64_t bytes_pushed = inbound_stream.bytes_pushed();
    bool is_closed = inbound_stream.is_closed();
    return Wrap32::wrap( syn_rcvd_ + bytes_pushed + is_closed, zero_point_.value() );
  }
}

uint16_t TCPReceiver::window_size( const Writer& inbound_stream ) const
{
  uint64_t available_capacity = inbound_stream.available_capacity();
  return available_capacity > 0xffff ? 0xffff : available_capacity;
}
