#include "ip.h"

#include "arp.h"
#include "ethernet.h"
#include "icmp.h"
#include "net.h"

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac) {
    (void)src_mac;  // IPv4 向上层交付源 IP，而不是源 MAC。

    // IPv4 固定首部至少 20 字节。
    if (buf->len < sizeof(ip_hdr_t)) {
        return;
    }

    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;
    size_t hdr_len = (size_t)hdr->hdr_len * IP_HDR_LEN_PER_BYTE;
    size_t total_len = swap16(hdr->total_len16);

    // 校验版本、首部长度和数据报总长度（含可选字段）。
    if (hdr->version != IP_VERSION_4 ||
        hdr_len < sizeof(ip_hdr_t) ||
        hdr_len > buf->len ||
        total_len < hdr_len ||
        total_len > buf->len) {
        return;
    }

    // 连同接收到的校验和字段一起计算，正确时结果应为 0。
    if (checksum16((uint16_t *)hdr, hdr_len) != 0) {
        return;
    }

    // 本实验协议栈只处理发往本机的 IPv4 数据报。
    if (memcmp(hdr->dst_ip, net_if_ip, NET_IP_LEN) != 0) {
        return;
    }

    // 这里没有接收分片重组模块，不能将不完整的数据交给上层。
    if (swap16(hdr->flags_fragment16) & (IP_MORE_FRAGMENT | 0x1FFF)) {
        return;
    }

    // 删除以太网的尾部填充，并保留原始 IP 报文供 ICMP 差错使用。
    if (buf->len > total_len) {
        if (buf_remove_padding(buf, buf->len - total_len) != 0) {
            return;
        }
    }

    uint8_t protocol = hdr->protocol;
    uint8_t src_ip[NET_IP_LEN];
    memcpy(src_ip, hdr->src_ip, NET_IP_LEN);

    if (buf_remove_header(buf, hdr_len) != 0) {
        return;
    }
    // net_in 按运行时注册的协议表分发；未注册则返回 -1。
    // 此时没有处理函数修改 buf，可以恢复原始首部生成 ICMP 差错。
    if (net_in(buf, protocol, src_ip) != 0) {
        if (buf_add_header(buf, hdr_len) == 0) {
            icmp_unreachable(buf, src_ip, ICMP_CODE_PROTOCOL_UNREACH);
        }
    }
}

/**
 * @brief 处理一个要发送的ip分片
 *
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf) {
    // IPv4 偏移以 8 字节为单位；单个分片不能超过链路 MTU。
    if ((offset % IP_HDR_OFFSET_PER_BYTE) != 0 ||
        buf->len + sizeof(ip_hdr_t) > ETHERNET_MAX_TRANSPORT_UNIT ||
        (mf && (buf->len % IP_HDR_OFFSET_PER_BYTE) != 0)) {
        return;
    }
    if (buf_add_header(buf, sizeof(ip_hdr_t)) != 0) {
        return;
    }

    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;
    hdr->version = IP_VERSION_4;
    hdr->hdr_len = sizeof(ip_hdr_t) / IP_HDR_LEN_PER_BYTE;
    hdr->tos = 0;
    hdr->total_len16 = swap16((uint16_t)buf->len);
    hdr->id16 = swap16((uint16_t)id);
    hdr->flags_fragment16 = swap16((uint16_t)(
        (offset / IP_HDR_OFFSET_PER_BYTE) | (mf ? IP_MORE_FRAGMENT : 0)));
    hdr->ttl = IP_DEFALUT_TTL;
    hdr->protocol = (uint8_t)protocol;
    memcpy(hdr->src_ip, net_if_ip, NET_IP_LEN);
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    hdr->hdr_checksum16 = 0;
    hdr->hdr_checksum16 = checksum16((uint16_t *)hdr, sizeof(ip_hdr_t));

    // ARP 解析目标 IP 并交给以太网层发送。
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 *
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol) {
    const size_t header_len = sizeof(ip_hdr_t);
    const size_t max_payload = ETHERNET_MAX_TRANSPORT_UNIT - header_len;
    const size_t fragment_payload =
        (max_payload / IP_HDR_OFFSET_PER_BYTE) * IP_HDR_OFFSET_PER_BYTE;
    static uint16_t next_id = 0;

    // 一个 IPv4 原始数据报（重组后）的总长度最多 65535 字节。
    if (buf->len + header_len > UINT16_MAX) {
        return;
    }

    uint16_t id = next_id++;
    uint8_t dst_ip[NET_IP_LEN];
    memcpy(dst_ip, ip, NET_IP_LEN);

    if (buf->len <= max_payload) {
        ip_fragment_out(buf, dst_ip, protocol, id, 0, 0);
        return;
    }

    // arp_out() 可能复用全局 txbuf 来发送 ARP 请求，因此在发出
    // 第一片之前备份完整载荷，避免后续分片的数据被覆盖。
    buf_t original;
    if (buf_init(&original, buf->len) != 0) {
        return;
    }
    memcpy(original.data, buf->data, buf->len);

    for (size_t offset = 0; offset < original.len;) {
        size_t remaining = original.len - offset;
        size_t part_len = remaining > fragment_payload ? fragment_payload : remaining;

        buf_t fragment;
        if (buf_init(&fragment, part_len) != 0) {
            return;
        }
        memcpy(fragment.data, original.data + offset, part_len);
        int more_fragments = (offset + part_len < original.len);
        ip_fragment_out(&fragment, dst_ip, protocol, id, (uint16_t)offset, more_fragments);
        offset += part_len;
    }
}

/**
 * @brief 初始化ip协议
 *
 */
void ip_init() {
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}
