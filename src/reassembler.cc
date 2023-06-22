#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  uint64_t available_capacity = output.available_capacity();
  uint64_t first_unacceptable = first_unassembled_ + available_capacity;
  uint64_t data_length = data.length();

  // If the data has been pushed into stream.
  if ( ( first_index + data_length <= first_unassembled_ && data_length != 0 ) || first_index > first_unacceptable )
    return;

  if ( first_index + data_length > first_unacceptable ) {
    // Crop data.
    data = data.substr( 0, first_unacceptable - first_index );
    is_last_substring = false;
  }

  // Insert pair into 'unassembled_'
  auto it = unassembled_.find( first_index );
  if ( it == unassembled_.end() ) {
    unassembled_[first_index] = data;
  } else {
    if ( data_length > it->second.length() ) {
      unassembled_[first_index] = data;
    }
  }

  for ( it = unassembled_.begin(); it != unassembled_.end(); ) {
    // If hole exists.
    if ( it->first > first_unassembled_ )
      break;

    if ( it->first + it->second.length() <= first_unassembled_ ) {
      // If the string has been assembled, remove it from 'unassembled'.
      auto tmp = it;
      it++;
      unassembled_.erase( tmp );
    } else {
      // Push string into stream.
      output.push( it->second.substr( first_unassembled_ - it->first ) );
      first_unassembled_ = it->first + it->second.length();
      // Remove corresponding item from 'unassembled'.
      auto tmp = it;
      it++;
      unassembled_.erase( tmp );
    }
  }

  // Receive end signal.
  if ( is_last_substring ) {
    finish_received_ = true;
  }

  // End stream.
  if ( finish_received_ && bytes_pending() == 0 ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  uint64_t bytes_count = 0;
  uint64_t largest_index = first_unassembled_;

  for ( auto it = unassembled_.begin(); it != unassembled_.end(); it++ ) {
    if ( it->first <= largest_index ) {
      if ( it->first + it->second.length() <= largest_index ) {
        // Current item is covered, skip it.
        continue;
      } else {
        // Current item is partly covered.
        bytes_count += it->first + it->second.length() - largest_index;
        largest_index = it->first + it->second.length();
      }
    } else {
      // CUrrent item is entirely not covered.
      bytes_count += it->second.length();
      largest_index = it->first + it->second.length();
    }
  }

  return bytes_count;
}
