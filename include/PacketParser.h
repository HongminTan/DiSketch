#ifndef PACKET_PARSER_H
#define PACKET_PARSER_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IPv4Layer.h"
#include "Packet.h"
#include "TwoTuple.h"

struct PacketRecord {
    TwoTuple flow;
    std::chrono::nanoseconds timestamp;
};

uint32_t ip_string_to_uint32(const std::string& ip_str);

std::string uint32_to_ip_string(uint32_t ip);

// 根据文件大小估计 reserve 空间
size_t estimate_packet_count(const std::string& file_path);

// 字节序转换
inline uint16_t swap_bytes16(uint16_t val) {
    return (val >> 8) | (val << 8);
}

// 字节序转换
inline uint32_t swap_bytes32(uint32_t val) {
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) |
           ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

class PcapReader {
   public:
    explicit PcapReader(const std::string& filename);

    bool open();
    bool get_next_packet(pcpp::RawPacket& raw_packet);
    void close();

   private:
    std::string filename_;
    std::ifstream file_;
    bool is_big_endian_;
    bool has_nano_precision_;
    pcpp::LinkLayerType link_type_;
};

class PacketParser {
   public:
    using PacketVector = std::vector<PacketRecord>;

    PacketVector parse_pcap(const std::string& file_path) const;
};

#endif
