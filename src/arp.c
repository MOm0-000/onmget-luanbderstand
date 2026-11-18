#include "arp.h"

#include "ethernet.h"
#include "net.h"

#include <stdio.h>
#include <string.h>
/**
 * @brief 初始的arp包
 *
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type16 = swap16(ARP_HW_ETHER),
    .pro_type16 = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = NET_IF_IP,
    .sender_mac = NET_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表，<ip,mac>的容器
 *
 */
map_t arp_table;

/**
 * @brief arp buffer，<ip,buf_t>的容器
 *
 */
map_t arp_buf;

/**
 * @brief 打印一条arp表项
 *
 * @param ip 表项的ip地址
 * @param mac 表项的mac地址
 * @param timestamp 表项的更新时间
 */
void arp_entry_print(void *ip, void *mac, time_t *timestamp) {
    printf("%s | %s | %s\n", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个arp表
 *
 */
void arp_print() {
    printf("===ARP TABLE BEGIN===\n");
    map_foreach(&arp_table, arp_entry_print);
    printf("===ARP TABLE  END ===\n");
}

/**
 * @brief 发送一个arp请求
 *
 * @param target_ip 想要知道的目标的ip地址
 */
void arp_req(uint8_t *target_ip) 
{
    /* Step1: 初始化缓冲区，大小为 ARP 报文结构体长度 */
    buf_init(&txbuf, sizeof(arp_pkt_t));

    /* Step2: 填写 ARP 报头 */
    arp_pkt_t *arp_pkt = (arp_pkt_t *)txbuf.data;
    // 先用预先定义好的模板进行拷贝（包含了本机 MAC/IP 和各类固定字段）
    *arp_pkt = arp_init_pkt;
    // 填入目标 IP
    memcpy(arp_pkt->target_ip, target_ip, NET_IP_LEN);

    /* Step3: 设置操作类型为 ARP_REQUEST，并转换大小端 */
    arp_pkt->opcode16 = swap16(ARP_REQUEST);

    /* Step4: 广播发送 ARP 报文 */
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 发送一个arp响应
 *
 * @param target_ip 目标ip地址
 * @param target_mac 目标mac地址
 */
void arp_resp(uint8_t *target_ip, uint8_t *target_mac) 
{
    /* Step1: 初始化缓冲区 */
    buf_init(&txbuf, sizeof(arp_pkt_t));

    /* Step2: 填写 ARP 报头 */
    arp_pkt_t *arp_pkt = (arp_pkt_t *)txbuf.data;
    // 使用模板初始化，自带本机 MAC/IP 以及固定规范字段
    *arp_pkt = arp_init_pkt;
    
    // 设置操作类型为 ARP 响应，并转换大小端
    arp_pkt->opcode16 = swap16(ARP_REPLY);

    // 填入接收方（请求方）的 IP 和 MAC 地址
    memcpy(arp_pkt->target_ip, target_ip, NET_IP_LEN);
    memcpy(arp_pkt->target_mac, target_mac, NET_MAC_LEN);

    /* Step3: 发送 ARP 报文（单播） */
    ethernet_out(&txbuf, target_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void arp_in(buf_t *buf, uint8_t *src_mac) {
    /* Step1: 检查数据长度 */
    if (buf->len < sizeof(arp_pkt_t)) 
    {return;} // 长度不足，丢弃
    

    /* Step2: 报头检查 */
    arp_pkt_t *arp_pkt = (arp_pkt_t *)buf->data;
    // 检查硬件类型、协议类型、MAC长度、IP长度
    if (swap16(arp_pkt->hw_type16) != ARP_HW_ETHER ||
        swap16(arp_pkt->pro_type16) != NET_PROTOCOL_IP ||
        arp_pkt->hw_len != NET_MAC_LEN ||
        arp_pkt->pro_len != NET_IP_LEN) 
    {return;}// 不符合规范，丢弃
   

    // 检查操作类型（只处理请求和应答）
    uint16_t opcode = swap16(arp_pkt->opcode16);
    if (opcode != ARP_REQUEST && opcode != ARP_REPLY) {return; }

    /* Step3: 更新 ARP 表项 */
    // 无论收到请求还是应答，都记录对方的 IP 和 MAC 映射
    // Key: 发送方 IP, Value: 发送方 MAC
    map_set(&arp_table, arp_pkt->sender_ip, src_mac);

    /* Step4: 查看缓存情况 */
    buf_t *cached_buf = (buf_t *)map_get(&arp_buf, arp_pkt->sender_ip);
    
    if (cached_buf != NULL)
    {
        // 有缓存：说明之前在等这个 IP 的 MAC，现在等到了，把缓存的数据发出去
        ethernet_out(cached_buf, src_mac, NET_PROTOCOL_IP);
        // 发送完毕后删除缓存
        map_delete(&arp_buf, arp_pkt->sender_ip);
    } 
    else 
    {
        // 无缓存：判断是否为本机的 ARP 请求
        if (opcode == ARP_REQUEST && memcmp(arp_pkt->target_ip, net_if_ip, NET_IP_LEN) == 0) 
        {
            // 对方在请求本机的 MAC 地址，调用 arp_resp 发送响应
            // (注意：arp_resp 目前是 TO-DO 状态，需要你后续补全)
            arp_resp(arp_pkt->sender_ip, src_mac);
        }
    }
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 */
void arp_out(buf_t *buf, uint8_t *ip) 
{
    /* Step1: 查找 ARP 表 */
    uint8_t *mac = (uint8_t *)map_get(&arp_table, ip);

    /* Step2: 找到对应 MAC 地址，直接发送 */
    if (mac != NULL) 
    {
        ethernet_out(buf, mac, NET_PROTOCOL_IP);
        return;
    }

    /* Step3: 未找到对应 MAC 地址 */
    buf_t *cached_buf = (buf_t *)map_get(&arp_buf, ip);
    
    if (cached_buf == NULL) 
    {
        // 没有缓存，说明是第一个发往该 IP 的包
        // 将数据包缓存到 arp_buf 中
        map_set(&arp_buf, ip, buf);
        // 发送 ARP 请求
        arp_req(ip);
    } 
    else 
    {
        // 已经有缓存了，说明正在等待 ARP 回应，不能再发请求，直接丢弃当前包
        // (这里什么都不做，函数返回即可，当前 buf 由上层框架回收)
    }
}

/**
 * @brief 初始化arp协议
 *
 */
void arp_init() {
    map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL, NULL);
    map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, NULL, buf_copy);
    net_add_protocol(NET_PROTOCOL_ARP, arp_in);
    arp_req(net_if_ip);
}