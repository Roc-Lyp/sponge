#include "tcp_segment.hh"

#include "parser.hh"
#include "util.hh"

#include <variant>

using namespace std;

//! \param[in] buffer string/Buffer to be parsed
//! \param[in] datagram_layer_checksum pseudo-checksum from the lower-layer protocol
ParseResult TCPSegment::parse(const Buffer buffer, const uint32_t datagram_layer_checksum) {
    // 对buffer内容计算校验和,与下面一层传入的校验和进行比对,如果不一致直接返回
    InternetChecksum check(datagram_layer_checksum);
    check.add(buffer);
    if (check.value()) {
        return ParseResult::BadChecksum;
    }
    // 解析拿到tcp协议请求头和请求体
    NetParser p{buffer};
    _header.parse(p);
    _payload = p.buffer();
    return p.get_error();
}

// 请求体大小+syn标志位占据的一个序列号+fin标志位占据的一个序列号
size_t TCPSegment::length_in_sequence_space() const {
    return payload().str().size() + (header().syn ? 1 : 0) + (header().fin ? 1 : 0);
}

//! \param[in] datagram_layer_checksum pseudo-checksum from the lower-layer protocol
BufferList TCPSegment::serialize(const uint32_t datagram_layer_checksum) const {
    // 拿到TCP请求头  
    TCPHeader header_out = _header;
    header_out.cksum = 0;

    // calculate checksum -- taken over entire segment
    // 计算TCP报文的校验和 --- 如果TCP报文校验和与传入的校验和计算一致,那么check.value()返回值应该为0
    InternetChecksum check(datagram_layer_checksum);
    check.add(header_out.serialize());
    check.add(_payload);
    header_out.cksum = check.value();

    // 将TCP报文添加到BufferList中
    BufferList ret;
    ret.append(header_out.serialize());
    ret.append(_payload);

    return ret;
}
