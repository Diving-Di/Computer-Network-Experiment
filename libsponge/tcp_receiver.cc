#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    if (header.syn) {
        _isn = header.seqno;
    }
    if (!_isn.has_value()) {
        return;
    }

    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(header.seqno, *_isn, checkpoint);

    if (header.syn) {
        _reassembler.push_substring(seg.payload().copy(), 0, header.fin);
    } else {
        if (abs_seqno == 0) {
            return;
        }
        _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, header.fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()) {
        return std::nullopt;
    }
    uint64_t written = _reassembler.stream_out().bytes_written();
    uint64_t abs_ackno = written + 1;
    if (_reassembler.stream_out().input_ended()) {
        abs_ackno++;
    }
    return wrap(abs_ackno, *_isn);
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
