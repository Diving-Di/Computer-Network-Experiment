#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    // 1. RST check
    if (seg.header().rst) {
        _set_rst_state(false);
        return;
    }

    // 2. Give to receiver
    _receiver.segment_received(seg);

    // 3. ACK check
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // Special case: LISTEN state (Receiver LISTEN, Sender CLOSED)
    // If we receive an ACK in LISTEN, we ignore it (relaxed behavior).
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::LISTEN &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        if (seg.header().ack) {
            return;
        }
        // If we receive a non-SYN segment in LISTEN (and not ACK), we ignore it.
        if (!seg.header().syn) {
            return;
        }
    }

    // 2. Give to receiver
    _receiver.segment_received(seg);

    // 3. ACK check
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // Check if we need to update linger state
    // If inbound stream just ended (or ended previously) and outbound stream is NOT at EOF,
    // then we don't need to linger.
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // 4. Reply logic
    // Only fill window if we are not in LISTEN (or if we are active opener)
    if (_sender.next_seqno_absolute() > 0 || _receiver.ackno().has_value()) {
        _sender.fill_window();
    }

    bool reply_needed = false;
    if (seg.length_in_sequence_space() > 0) {
        reply_needed = true;
    }
    
    // Check for keep-alive (seqno = ackno - 1)
    if (_receiver.ackno().has_value() && 
        seg.length_in_sequence_space() == 0 && 
        seg.header().seqno == _receiver.ackno().value() - 1) {
        reply_needed = true;
    }

    if (_sender.segments_out().empty() && reply_needed) {
        _sender.send_empty_segment();
    }

    _trans_segments_to_out_queue();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();
    _trans_segments_to_out_queue();
    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        // Abort connection
        _set_rst_state(true);
        return;
    }

    _trans_segments_to_out_queue();
    
    // Check for clean shutdown
    if (_receiver.stream_out().input_ended() &&
        _sender.stream_in().eof() &&
        _sender.bytes_in_flight() == 0 &&
        _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2) {
        
        if (!_linger_after_streams_finish) {
            _is_active = false;
        } else if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _is_active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _trans_segments_to_out_queue();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _is_active = true;
    _trans_segments_to_out_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _set_rst_state(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_trans_segments_to_out_queue() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        
        _segments_out.push(seg);
    }
}

void TCPConnection::_set_rst_state(bool send_rst) {
    if (send_rst) {
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
    }
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}
