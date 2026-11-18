#include "icmp.h"

#include "ip.h"
#include "net.h"

/**
 * @brief 发送icmp响应
 *
 * @param req_buf 收到的icmp请求包
 * @param src_ip 源ip地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip) {
    // 回显应答必须原样保留请求中的标识符、序号和数据。
    // 因此复制整份 ICMP 报文，只修改类型/代码并重新计算校验和。
    if (buf_init(&txbuf, req_buf->len) != 0) {
        return;
    }
    memcpy(txbuf.data, req_buf->data, req_buf->len);

    icmp_hdr_t *hdr = (icmp_hdr_t *)txbuf.data;
    hdr->type = ICMP_TYPE_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum16 = 0;
    hdr->checksum16 = checksum16((uint16_t *)txbuf.data, txbuf.len);

    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip) {
    // 至少包含 8 字节 ICMP 首部，才能读取类型等字段。
    if (buf->len < sizeof(icmp_hdr_t)) {
        return;
    }

    icmp_hdr_t *hdr = (icmp_hdr_t *)buf->data;
    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        icmp_resp(buf, src_ip);
    }
}

/**
 * @brief 发送icmp不可达
 *
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code) {
    // recv_buf 应包含原始 IPv4 首部和数据（而非已去掉 IP 首部的载荷）。
    if (recv_buf->len < sizeof(ip_hdr_t)) {
        return;
    }
    ip_hdr_t *ip_hdr = (ip_hdr_t *)recv_buf->data;
    size_t ip_hdr_len = (size_t)ip_hdr->hdr_len * IP_HDR_LEN_PER_BYTE;
    if (ip_hdr->version != IP_VERSION_4 ||
        ip_hdr_len < sizeof(ip_hdr_t) || ip_hdr_len > recv_buf->len) {
        return;
    }

    // ICMP 目的不可达报文携带：原 IP 首部（含可选字段）+ 原数据前 8 字节。
    size_t available = recv_buf->len - ip_hdr_len;
    size_t total_len = swap16(ip_hdr->total_len16);
    if (total_len >= ip_hdr_len && total_len < recv_buf->len) {
        available = total_len - ip_hdr_len;
    }
    size_t quoted_data_len = available < 8 ? available : 8;
    size_t quote_len = ip_hdr_len + quoted_data_len;

    if (buf_init(&txbuf, sizeof(icmp_hdr_t) + quote_len) != 0) {
        return;
    }
    icmp_hdr_t *hdr = (icmp_hdr_t *)txbuf.data;
    memset(hdr, 0, sizeof(*hdr));  // 差错报文首部的后 4 字节为未使用字段，需清零。
    hdr->type = ICMP_TYPE_UNREACH;
    hdr->code = (uint8_t)code;
    memcpy(txbuf.data + sizeof(icmp_hdr_t), recv_buf->data, quote_len);

    hdr->checksum16 = 0;
    hdr->checksum16 = checksum16((uint16_t *)txbuf.data, txbuf.len);
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 初始化icmp协议
 *
 */
void icmp_init() {
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}