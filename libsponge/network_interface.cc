#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
// send_datagram方法用于发送以太网包，其中涉及ARP广播寻址
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    /*
        1、获取下一跳IP地址
        2、遍历Arp表，如果没有进行arp广播
        3、如果在已发送未回应的arp列表中没有，在构造arp包，避免5秒内多次发送相同arp包
        4、构造arp请求、arp操作码、发送端mac地址、发送端ip地址、接收端mac地址-置空，接收端ip地址
        5、构建以太网帧、填充以太网头
        6、ARP请求序列化后作为以太网帧的payload
        7、将填充完毕的以太网帧推入_frames_out通道,等待被传输
        8、记录当前发送的ARP请求包, key=下一跳IP地址,val=该ARP请求的过期时间
    */
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    const auto &arp_iter = _arp_table.find(next_hop_ip);
    // 遍历arp表
    if (arp_iter == _arp_table.end()) {
        // 遍历等待列表
        if (_waiting_arp_response_ip_addr.find(next_hop_ip) == _waiting_arp_response_ip_addr.end()) {
            // 构造arp包
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = next_hop_ip;

            // 构造以太帧
            EthernetFrame eth_frame;
            eth_frame.header() = {
                                    /*dst*/  ETHERNET_BROADCAST,
                                    /*src*/  _ethernet_address,
                                    /*type*/ EthernetHeader::TYPE_ARP,};
            // arp请求序列化
            eth_frame.payload() = arp_request.serialize();
            _frames_out.push(eth_frame);
            // 记录ARP请求包
            _waiting_arp_response_ip_addr[next_hop_ip] = _arp_response_default_ttl;
        }
        // 将该 ip 包加入等待队列中
        _waiting_arp_internet_datagrams.push_back({next_hop, dgram});
    } else {
        // arp 表中有目标ip-mac对应关系，直接生成以太网帧
        EthernetFrame eth_frame;
        eth_frame.header() = {
                                /*dst*/  arp_iter->second.eth_addr,
                                /*src*/  _ethernet_address,
                                /*type*/  EthernetHeader::TYPE_IPv4};
        eth_frame.payload() = dgram.serialize();
        _frames_out.push(eth_frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
// recv_frame 用于接收以太网帧，会有三种情况
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    /*
        1、不是发给本机mac地址的frame帧，并且不是arp广播包
        2、发的是ip包
        3、发的arp包
    */
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    // 如果是ip包，如果解析有问题，返回空
    if (frame.header().type == EthernetHeader::TYPE_IPv4){
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) != ParseResult::NoError)
            return nullopt;
        // 这里不能有ip判断，因为是链路层协议
        return datagram;
    } 
    // 如果是arp包
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        // 如果是arp包
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError)
            return nullopt;

        // 从arp包中拿到对应的字段
        const uint32_t &src_ip_addr = arp_msg.sender_ip_address;
        const uint32_t &dst_ip_addr = arp_msg.target_ip_address;
        const EthernetAddress &src_eth_addr = arp_msg.sender_ethernet_address;
        const EthernetAddress &dst_eth_addr = arp_msg.target_ethernet_address;

        // 判断是否为响应或应答
        bool is_valid_arp_request = 
            arp_msg.opcode == ARPMessage::OPCODE_REQUEST && dst_ip_addr == _ip_address.ipv4_numeric();
        bool is_valid_arp_response = 
            arp_msg.opcode == ARPMessage::OPCODE_REPLY && dst_eth_addr == _ethernet_address;

        // 如果接收是arp请求包，构造回应包
        if (is_valid_arp_request) {
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = src_eth_addr;
            arp_reply.target_ip_address = src_ip_addr;

            // 构造以太帧
            EthernetFrame eth_frame;
            eth_frame.header() = {
                                    /*dst*/  src_eth_addr,
                                    /*src*/  _ethernet_address,
                                    /*type*/ EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_reply.serialize();
            _frames_out.push(eth_frame);
        }
        // 无论是请求还是回应，都会更新arp表
        if (is_valid_arp_request || is_valid_arp_response) {
            _arp_table[src_ip_addr] = {src_eth_addr, _arp_entry_default_ttl};

            for (auto iter = _waiting_arp_internet_datagrams.begin(); iter != _waiting_arp_internet_datagrams.end(); /**/) {
                if (iter->first.ipv4_numeric() == src_ip_addr) {
                    // 重新发送
                    send_datagram(iter->second, iter->first);
                    iter = _waiting_arp_internet_datagrams.erase(iter);                
                } else {
                    ++iter;                    
                }
            }
            _waiting_arp_response_ip_addr.erase(src_ip_addr);
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    /*
        1、循环清理arp_table中过期的条目
        2、循环清理arp等待队列中过期条目，并且重新发送arp请求
    */
   for (auto iter = _arp_table.begin(); iter != _arp_table.end(); /**/) {
        if (iter->second.ttl <= ms_since_last_tick) {
            iter = _arp_table.erase(iter);
        } else {
            iter->second.ttl -= ms_since_last_tick;
            ++iter;
        }
   }

   for (auto iter = _waiting_arp_response_ip_addr.begin(); iter != _waiting_arp_response_ip_addr.end(); /**/) {
        if (iter->second <= ms_since_last_tick) {
            // 重新构造arp请求包,广播包
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {};
            arp_request.target_ip_address = iter->first;

            // 构造以太帧
            EthernetFrame eth_frame;
            eth_frame.header() = {
                                    /*dst*/  ETHERNET_BROADCAST,
                                    /*src*/  _ethernet_address,
                                    /*type*/ EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_request.serialize();
            _frames_out.push(eth_frame);

            iter->second = _arp_response_default_ttl;
        } else {
            iter->second -= ms_since_last_tick;
            ++iter;
        }
   }
}
