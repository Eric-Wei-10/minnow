#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return abs_seqno_ - abs_ackno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if (messages_out_.empty())
    return nullopt;
  else {
    TCPSenderMessage msg = messages_out_.front();
    messages_out_.pop();
    return msg;
  }
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  // Send SYN message.
  if (!syn_sent_) {
    syn_sent_ = true;

    TCPSenderMessage msg;
    msg.SYN = true;
    msg.seqno = Wrap32::wrap(abs_seqno_, isn_);

    messages_out_.push(msg);
    outstanding_.push(msg);

    abs_seqno_ += msg.sequence_length();

    return;
  }

  if (fin_sent_) {
    return;
  }

  // Send messages according to buffer and window size.
  uint64_t bytes_can_send = window_size_ - sequence_numbers_in_flight();
  uint64_t bytes_can_read = outbound_stream.bytes_buffered();

  // Buffer is empty.
  if (bytes_can_read == 0) {
    if (outbound_stream.is_finished()) {
      // Send FIN message.
      fin_sent_ = true;

      TCPSenderMessage msg;
      msg.FIN = true;
      msg.seqno = Wrap32::wrap(abs_seqno_, isn_);

      messages_out_.push(msg);
      outstanding_.push(msg);

      abs_seqno_ += msg.sequence_length();

      return;
    } else {
      return;
    }
  }

  // Buffer is not empty.
  uint64_t bytes_to_send = bytes_can_send < bytes_can_read ? bytes_can_send : bytes_can_read;
  while (bytes_to_send) {
    TCPSenderMessage msg;
    uint64_t payload_size = 
      bytes_to_send < TCPConfig::MAX_PAYLOAD_SIZE ? bytes_to_send : TCPConfig::MAX_PAYLOAD_SIZE;

    msg.seqno = Wrap32::wrap(abs_seqno_, isn_);
    // msg.payload = Buffer(outbound_stream.peek().data());
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(abs_seqno_, isn_);
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  uint64_t new_ackno = msg.ackno.value().unwrap(isn_, abs_ackno_);
  cout << "here " << new_ackno << endl;

  if (new_ackno < abs_ackno_)
    return;

  cout << "here1" << endl;
  
  while (!outstanding_.empty()) {
    TCPSenderMessage message = outstanding_.front();
    uint64_t seqno = message.seqno.unwrap(isn_, abs_seqno_);
    uint64_t message_length = message.sequence_length();

    if (seqno + message_length <= new_ackno) {
      outstanding_.pop();
    } else {
      break;
    }
  }

  abs_ackno_ = new_ackno;
  window_size_ = msg.window_size;

  return;
}

void TCPSender::tick( uint64_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}
