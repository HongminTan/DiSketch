#include "PacketParser.h"

#include <chrono>
#include <iomanip>
#include <iostream>

constexpr int SHOW_NUM = 5;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: parse_pcap <pcap-file>" << std::endl;
        return 1;
    }

    try {
        PacketParser parser;

        auto start = std::chrono::high_resolution_clock::now();
        auto packets = parser.parse_pcap(argv[1]);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

        std::cout << "Parsed " << packets.size() << " IPv4 packets in "
                  << duration_ms << " ms\n"
                  << std::endl;

        for (size_t i = 0; i < packets.size() && i < SHOW_NUM; ++i) {
            const auto& packet = packets[i];
            const double seconds =
                std::chrono::duration<double>(packet.timestamp).count();
            std::cout << std::fixed << std::setprecision(6) << seconds
                      << "s: " << uint32_to_ip_string(packet.flow.src_ip)
                      << " -> " << uint32_to_ip_string(packet.flow.dst_ip)
                      << std::endl;
        }

        if (packets.size() > SHOW_NUM) {
            std::cout << "... (" << (packets.size() - SHOW_NUM)
                      << " more packets)" << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
