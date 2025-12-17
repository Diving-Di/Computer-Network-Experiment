#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    size_t window_size = _window_size == 0 ? 1 : _window_size;
    
    while (window_size > _bytes_in_flight) {
        TCPSegment seg;
        if (_next_seqno == 0) {
            seg.header().syn = true;
        }
        
        size_t remaining_window = window_size - _bytes_in_flight;
        size_t payload_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, remaining_window - (seg.header().syn ? 1 : 0));
        
        std::string payload = _stream.read(payload_size);
        seg.payload() = Buffer(std::move(payload));
        
        if (_stream.eof() && seg.length_in_sequence_space() + 1 <= remaining_window) {
             if (_next_seqno + seg.length_in_sequence_space() < _stream.bytes_written() + 2) {
                 seg.header().fin = true;
             }
        }
        
        size_t len = seg.length_in_sequence_space();
        if (len == 0) {
            break;
        }
        
        seg.header().seqno = wrap(_next_seqno, _isn);
        
        _segments_out.push(seg);
        _outstanding_segments.push({_next_seqno, seg});
        
        _next_seqno += len;
        _bytes_in_flight += len;
        
        if (!_timer_running) {
            _timer_running = true;
            _timer_ms = 0;
        }
        
        if (seg.header().fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    
    if (abs_ackno > _next_seqno) {
        return;
    }
    
    _window_size = window_size;
    
    bool new_data_acked = false;
    while (!_outstanding_segments.empty()) {
        auto &front = _outstanding_segments.front();
        uint64_t seg_abs_seqno = front.first;
        size_t seg_len = front.second.length_in_sequence_space();
        
        if (seg_abs_seqno + seg_len <= abs_ackno) {
            _bytes_in_flight -= seg_len;
            _outstanding_segments.pop();
            new_data_acked = true;
        } else {
            break;
        }
    }
    
    if (new_data_acked) {
        _rto = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        _timer_ms = 0;
        if (!_outstanding_segments.empty()) {
            _timer_running = true;
        } else {
            _timer_running = false;
        }
    }
    
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_running) {
        return;
    }
    
    _timer_ms += ms_since_last_tick;
    
    if (_timer_ms >= _rto && !_outstanding_segments.empty()) {
        _segments_out.push(_outstanding_segments.front().second);
        
        if (_window_size > 0) {
            _rto *= 2;
        }
        
        _timer_running = true;
        _timer_ms = 0;
        _consecutive_retransmissions++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
