#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ),
  window_size_(TCPConfig::DEFAULT_CAPACITY)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return abs_seqno_ - abs_ackno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutive_retransmissions_;
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

    // Send FIN if there is nothing to send.
    if (outbound_stream.is_finished() && window_size_ != 0) {
      fin_sent_ = true;
      msg.FIN = true;
    }

    messages_out_.push(msg);
    outstanding_.push(msg);
    
    // Start timer.
    if (!timer_started_) {
      timer_started_ = true;
      consecutive_retransmissions_ = 0;
      timer_countdown_ = initial_RTO_ms_;
    }

    abs_seqno_ += msg.sequence_length();

    return;
  }

  // Suppose SYN is sent but not acked, connection is not established.
  if (!syn_acked_) {
    return;
  }

  // If FIN is sent, then the stream is closed.
  if (fin_sent_) {
    return;
  }

  // If window size is 0, send segment with payload 1.
  if (window_size_ == 0) {
    if (outstanding_.empty()) {
      TCPSenderMessage msg;

      msg.seqno = Wrap32::wrap(abs_seqno_, isn_);

      if (outbound_stream.bytes_buffered() != 0) {
        // Buffer is not empty.
        string_view sv = outbound_stream.peek();
        string payload = {sv.begin(), sv.end()};

        msg.payload = payload.substr(0, 1);
        outbound_stream.pop(1);
      } else {
        // Buffer is empty.
        if (outbound_stream.is_finished()) {
          // Stream is closed and FIN has not been sent.
          fin_sent_ = true;
          msg.FIN = true;
        } else {
          // Stream is not closed.
          return;
        }
      }

      messages_out_.push(msg);
      outstanding_.push(msg);

      // Start timer.
      if (!timer_started_) {
        timer_started_ = true;
        consecutive_retransmissions_ = 0;
        timer_countdown_ = initial_RTO_ms_;
      }

      abs_seqno_ += 1;

      return;
    } else {
      return;
    }
  }

  // Send messages according to buffer and window size.
  uint64_t bytes_can_send = window_size_ - sequence_numbers_in_flight();
  uint64_t bytes_can_read = outbound_stream.bytes_buffered();

  // Buffer is empty.
  if (bytes_can_read == 0) {
    if (outbound_stream.is_finished() && bytes_can_send != 0) {
      // Send FIN message.
      fin_sent_ = true;

      TCPSenderMessage msg;
      msg.FIN = true;
      msg.seqno = Wrap32::wrap(abs_seqno_, isn_);

      messages_out_.push(msg);
      outstanding_.push(msg);

      // Start timer.
      if (!timer_started_) {
        timer_started_ = true;
        consecutive_retransmissions_ = 0;
        timer_countdown_ = initial_RTO_ms_;
      }

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
    string_view sv = outbound_stream.peek();
    string payload = {sv.begin(), sv.end()};

    msg.seqno = Wrap32::wrap(abs_seqno_, isn_);
    msg.payload = Buffer(payload.substr(0, payload_size));
    outbound_stream.pop(payload_size);

    // If stream is finished and there is enough space, send FIN.
    if (payload_size < bytes_can_send && outbound_stream.is_finished()) {
      fin_sent_ = true;
      msg.FIN = true;
    }

    messages_out_.push(msg);
    outstanding_.push(msg);

    // Start timer.
    if (!timer_started_) {
      timer_started_ = true;
      consecutive_retransmissions_ = 0;
      timer_countdown_ = initial_RTO_ms_;
    }

    // Update abs_seqno.
    abs_seqno_ += msg.sequence_length();

    // Update information about future data.
    bytes_can_send -= payload_size;
    bytes_to_send -= payload_size;
  }

  return;
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
  uint64_t new_ackno;

  if (msg.ackno.has_value()) {
    // If msg contains ackno, get it and go through codes below.
    new_ackno = msg.ackno.value().unwrap(isn_, abs_ackno_);
    // Ack SYN.
    if (!syn_acked_ && new_ackno > 0) syn_acked_ = true;
  } else {
    // If msg doesn't contains ackno, update window size and return.
    window_size_ = msg.window_size;
    return;
  }

  // Ignore invalid messages.
  if (window_size_ != 0 && (new_ackno < abs_ackno_ || new_ackno > abs_seqno_))
    return;
  
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

  if (new_ackno > abs_ackno_) {
    if (outstanding_.empty()) {
      timer_started_ = false;
    } else {
      consecutive_retransmissions_ = 0;
      timer_countdown_ = initial_RTO_ms_;
    }
  }

  abs_ackno_ = new_ackno;
  window_size_ = msg.window_size;

  return;
}

void TCPSender::tick( uint64_t ms_since_last_tick )
{
  // Your code here.
  // If timer is not started, simply return.
  if (!timer_started_) return;

  uint64_t tick_countdown = ms_since_last_tick;
  bool resend = false;

  while (tick_countdown >= timer_countdown_) {
    // Update tick countdown and retx count.
    tick_countdown -= timer_countdown_;
    consecutive_retransmissions_ += 1;

    // Consecutive retx exceeds limit, stop timer and return.
    if (consecutive_retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS) {
      timer_started_ = false;
      return;
    }

    // Consecutive retx doesn't exceeds limit, double backoff.
    if (window_size_ != 0) {
      timer_countdown_ = initial_RTO_ms_ << consecutive_retransmissions_;
    } else {
      timer_countdown_ = initial_RTO_ms_;
      consecutive_retransmissions_ -= 1;  // When window size is 0, this message should be resent forever.
    }

    // Only resend one message each time 'tick' is called.
    if (!resend) {
      resend = true;
      messages_out_.push(outstanding_.front());
    }
  }

  timer_countdown_ -= tick_countdown;

  return;
}
