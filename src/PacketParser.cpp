#include "PacketParser.h"

uint32_t ip_string_to_uint32(const std::string& ip_str) {
    uint32_t result = 0;
    std::istringstream iss(ip_str);
    std::string octet_str;
    int shift = 24;

    while (std::getline(iss, octet_str, '.')) {
        int octet = std::stoi(octet_str);
        if (octet < 0 || octet > 255) {
            throw std::runtime_error("Invalid IP address: " + ip_str);
        }
        result |= static_cast<uint32_t>(octet) << shift;
        shift -= 8;
    }

    return result;
}

std::string uint32_to_ip_string(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << '.' << ((ip >> 16) & 0xFF) << '.'
        << ((ip >> 8) & 0xFF) << '.' << (ip & 0xFF);
    return oss.str();
}

size_t estimate_packet_count(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return 100000;
    }

    std::streamsize file_size = file.tellg();
    file.close();

    constexpr size_t PACKETS_PER_MB = 13000;
    size_t file_size_mb = static_cast<size_t>(file_size / (1024 * 1024));
    size_t estimated_packets = file_size_mb * PACKETS_PER_MB;

    return estimated_packets > 0 ? estimated_packets : 10000;
}

// Pcap File Struct
namespace {

constexpr uint32_t MAGIC_MICROSECONDS_LE = 0xa1b2c3d4;
constexpr uint32_t MAGIC_MICROSECONDS_BE = 0xd4c3b2a1;
constexpr uint32_t MAGIC_NANOSECONDS_LE = 0xa1b23c4d;
constexpr uint32_t MAGIC_NANOSECONDS_BE = 0x4d3cb2a1;

#pragma pack(push, 1)
struct PcapFileHeader {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

struct PcapPacketHeader {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

}  // namespace

PcapReader::PcapReader(const std::string& filename)
    : filename_(filename),
      is_big_endian_(false),
      has_nano_precision_(false),
      link_type_(pcpp::LINKTYPE_ETHERNET) {}

bool PcapReader::open() {
    file_.open(filename_, std::ios::binary);
    if (!file_) {
        return false;
    }

    PcapFileHeader header;
    file_.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file_) {
        return false;
    }

    switch (header.magic_number) {
        case MAGIC_MICROSECONDS_LE:
            is_big_endian_ = false;
            has_nano_precision_ = false;
            break;
        case MAGIC_MICROSECONDS_BE:
            is_big_endian_ = true;
            has_nano_precision_ = false;
            break;
        case MAGIC_NANOSECONDS_LE:
            is_big_endian_ = false;
            has_nano_precision_ = true;
            break;
        case MAGIC_NANOSECONDS_BE:
            is_big_endian_ = true;
            has_nano_precision_ = true;
            break;
        default:
            return false;
    }

    if (is_big_endian_) {
        header.network = swap_bytes32(header.network);
    }

    if (pcpp::RawPacket::isLinkTypeValid(static_cast<int>(header.network))) {
        link_type_ = static_cast<pcpp::LinkLayerType>(header.network);
    }

    return true;
}

bool PcapReader::get_next_packet(pcpp::RawPacket& raw_packet) {
    raw_packet.clear();

    PcapPacketHeader pkt_header;
    file_.read(reinterpret_cast<char*>(&pkt_header), sizeof(pkt_header));
    if (!file_ || file_.gcount() == 0) {
        return false;
    }
    if (file_.gcount() != sizeof(pkt_header)) {
        throw std::runtime_error("Incomplete packet header");
    }

    if (is_big_endian_) {
        pkt_header.ts_sec = swap_bytes32(pkt_header.ts_sec);
        pkt_header.ts_usec = swap_bytes32(pkt_header.ts_usec);
        pkt_header.incl_len = swap_bytes32(pkt_header.incl_len);
        pkt_header.orig_len = swap_bytes32(pkt_header.orig_len);
    }

    if (pkt_header.incl_len == 0 ||
        pkt_header.incl_len > PCPP_MAX_PACKET_SIZE) {
        file_.seekg(pkt_header.incl_len, std::ios::cur);
        return get_next_packet(raw_packet);
    }

    uint8_t* packet_data = new uint8_t[pkt_header.incl_len];
    file_.read(reinterpret_cast<char*>(packet_data), pkt_header.incl_len);
    if (!file_ ||
        static_cast<uint32_t>(file_.gcount()) != pkt_header.incl_len) {
        delete[] packet_data;
        throw std::runtime_error("Incomplete packet data");
    }

    bool success;
    if (has_nano_precision_) {
        timespec ts;
        ts.tv_sec = static_cast<time_t>(pkt_header.ts_sec);
        ts.tv_nsec = static_cast<long>(pkt_header.ts_usec);
        success = raw_packet.setRawData(
            packet_data, static_cast<int>(pkt_header.incl_len), ts, link_type_,
            static_cast<int>(pkt_header.orig_len));
    } else {
        timeval tv;
        tv.tv_sec = static_cast<long>(pkt_header.ts_sec);
        tv.tv_usec = static_cast<long>(pkt_header.ts_usec);
        success = raw_packet.setRawData(
            packet_data, static_cast<int>(pkt_header.incl_len), tv, link_type_,
            static_cast<int>(pkt_header.orig_len));
    }

    if (!success) {
        delete[] packet_data;
        throw std::runtime_error("Failed to set raw packet data");
    }

    return true;
}

void PcapReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

PacketParser::PacketVector PacketParser::parse_pcap(
    const std::string& file_path) const {
    PacketVector packets;

    PcapReader reader(file_path);
    if (!reader.open()) {
        throw std::runtime_error("Failed to open pcap file: " + file_path);
    }

    packets.reserve(estimate_packet_count(file_path));

    pcpp::RawPacket raw_packet;
    while (reader.get_next_packet(raw_packet)) {
        pcpp::Packet parsed_packet(&raw_packet, pcpp::OsiModelNetworkLayer);

        auto* ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
        if (!ipv4_layer) {
            continue;
        }

        PacketRecord record;
        record.flow.src_ip = ipv4_layer->getSrcIPv4Address().toInt();
        record.flow.dst_ip = ipv4_layer->getDstIPv4Address().toInt();

        const timespec& ts = raw_packet.getPacketTimeStamp();
        record.timestamp = std::chrono::seconds{ts.tv_sec} +
                           std::chrono::nanoseconds{ts.tv_nsec};

        packets.push_back(record);
    }

    reader.close();

    // 按时间戳排序
    std::sort(packets.begin(), packets.end(),
              [](const PacketRecord& a, const PacketRecord& b) {
                  return a.timestamp < b.timestamp;
              });

    return packets;
}
