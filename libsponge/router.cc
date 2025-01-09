#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
        cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

        _router_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    /*
        1、从数据报头部获取目的ip信息
        2、初始化最大路由条目
        3、循环找匹配路由规则：如果前缀全为0，说明匹配所有地址，或者将路由前缀和目的地址异或，再右移32-子网掩码长度，如果都为0说明成功匹配
        4、如果当前最大路由条目是初始的，或者当前最大路由条目的长度小于新匹配的路由规则长度，更新最长匹配规则
        5、如果存在最匹配的，并且数据包仍然存活，则将其转发
        6、获取下一个的地址和网络接口
        7、数据报转发
    */
    const uint32_t dst_ip_addr = dgram.header().dst;
    auto max_matched_entry = _router_table.end();

    for (auto iter = _router_table.begin(); iter != _router_table.end(); iter++) {
        if (iter->prefix_length == 0 || (iter->route_prefix ^ dst_ip_addr) >> (32 - iter->prefix_length) == 0) {
            if (max_matched_entry == _router_table.end() || max_matched_entry->prefix_length < iter->prefix_length) {
                max_matched_entry = iter;
            }
        }
    }

    // step 5, ttl在数据包头部
    if (max_matched_entry != _router_table.end() && dgram.header().ttl-- > 1) {
        const optional <Address> next_hop = max_matched_entry->next_hop;
        AsyncNetworkInterface &interface = _interfaces[max_matched_entry->interface_idx];

        // 如果有下一跳进行转发
        if (next_hop.has_value()) {
            interface.send_datagram(dgram, next_hop.value());
        } else {
            // 目的主机和当前主机在同一局域中
            interface.send_datagram(dgram, Address::from_ipv4_numeric(dst_ip_addr));
        }
    }
    // 其他情况丢弃

}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    // &是引用，可以直接修改_interfaces容器中的元素
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty())
        {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

