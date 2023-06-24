#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <iostream>

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t abs_ackno_{0};  // Next byte to be acked.
  uint64_t abs_seqno_{0};  // Next byte to be sent.
  uint16_t window_size_{0}; // Receiver's window size.
  // Messages that has been sent. (used for 'maybe_send').
  queue<TCPSenderMessage> messages_out_{};
  // Messages that has been sent. (used for 'receive' and 'tick').
  queue<TCPSenderMessage> outstanding_{};
  bool syn_sent_{false};    // Whether SYN has been sent.
  bool syn_acked_{false};   // Whether SYN has been acked.
  bool fin_sent_{false};    // Whether FIN has been sent.
  // Timer variables below
  bool timer_started_{false};
  uint64_t consecutive_retransmissions_{0};
  uint64_t timer_countdown_{0};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
