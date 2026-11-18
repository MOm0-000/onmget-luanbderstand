#include "udp.h"

#include "icmp.h"
#include "ip.h"

/**
 * @brief udp处理程序表
 *
 */
map_t udp_table;

/**
 * @brief 处理一个收到的udp数据包
 *
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip) {
    // UDP 固定首部为 8 字节；长度字段包含 UDP 首部和 UDP 数据。
    if (buf->len < sizeof(udp_hdr_t)) {
        return;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)buf->data;
    size_t udp_len = swap16(hdr->total_len16);
    if (udp_len < sizeof(udp_hdr_t) || udp_len > buf->len) {
        return;
    }

    // 避免将 IP 载荷中的多余尾部字节作为 UDP 数据或参与校验。
    if (buf->len > udp_len && buf_remove_padding(buf, buf->len - udp_len) != 0) {
        return;
    }

    // 校验和字段本身不参与计算；transport_checksum 会临时添加
    // 12 字节伪首部，并在返回前恢复原有数据和缓冲区状态。
    uint16_t received_checksum = hdr->checksum16;
    hdr->checksum16 = 0;
    uint16_t expected_checksum = transport_checksum(
        NET_PROTOCOL_UDP, buf, src_ip, net_if_ip);
    hdr->checksum16 = received_checksum;

    // UDP 计算结果为 0 时，在线路上应编码成全 1 (0xffff)。
    if (expected_checksum == 0) {
        expected_checksum = 0xffff;
    }
    if (received_checksum != expected_checksum) {
        return;
    }

    uint16_t dst_port = swap16(hdr->dst_port16);
    uint16_t src_port = swap16(hdr->src_port16);
    // map_t 内部采用紧密存储；函数指针可能位于非自然对齐地址。
    // 先拷贝到局部变量，避免直接解引用潜在未对齐的指针。
    void *handler_entry = map_get(&udp_table, &dst_port);
    if (handler_entry == NULL) {
        // ip_in() 去除了 IP 首部，但其字节仍位于缓冲区前方。
        // 恢复原始 IP 首部，供 ICMP 差错报文引用。
        if (buf_add_header(buf, sizeof(ip_hdr_t)) == 0) {
            icmp_unreachable(buf, src_ip, ICMP_CODE_PORT_UNREACH);
        }
        return;
    }

    udp_handler_t handler;
    memcpy(&handler, handler_entry, sizeof(handler));
    if (handler == NULL || buf_remove_header(buf, sizeof(udp_hdr_t)) != 0) {
        return;
    }
    handler(buf->data, buf->len, src_ip, src_port);
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port) {
    // UDP 长度字段为 16 位，且包含 8 字节首部。
    if (buf->len > UINT16_MAX - sizeof(udp_hdr_t)) {
        return;
    }
    if (buf_add_header(buf, sizeof(udp_hdr_t)) != 0) {
        return;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)buf->data;
    hdr->src_port16 = swap16(src_port);
    hdr->dst_port16 = swap16(dst_port);
    hdr->total_len16 = swap16((uint16_t)buf->len);
    hdr->checksum16 = 0;
    hdr->checksum16 = transport_checksum(
        NET_PROTOCOL_UDP, buf, net_if_ip, dst_ip);
    if (hdr->checksum16 == 0) {
        hdr->checksum16 = 0xffff;
    }

    ip_out(buf, dst_ip, NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 *
 */
void udp_init() {
    map_init(&udp_table, sizeof(uint16_t), sizeof(udp_handler_t), 0, 0, NULL, NULL);
    net_add_protocol(NET_PROTOCOL_UDP, udp_in);
}

/**
 * @brief 打开一个udp端口并注册处理程序
 *
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler) {
    return map_set(&udp_table, &port, &handler);
}

/**
 * @brief 关闭一个udp端口
 *
 * @param port 端口号
 */
void udp_close(uint16_t port) {
    map_delete(&udp_table, &port);
}

/**
 * @brief 发送一个udp包
 *
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port) {
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dst_ip, dst_port);
}