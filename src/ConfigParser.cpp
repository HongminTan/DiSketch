#include "ConfigParser.h"

bool ConfigParser::parse(const std::string& ini_path, DiSketchConfig& config) {
    CSimpleIniA ini;
    ini.SetUnicode();

    SI_Error rc = ini.LoadFile(ini_path.c_str());
    if (rc < 0) {
        std::cerr << "无法打开配置文件: " << ini_path << std::endl;
        return false;
    }

    // 解析全局配置
    config.pcap_path = ini.GetValue("global", "pcap", "");
    if (config.pcap_path.empty()) {
        std::cerr << "配置缺少 pcap 路径" << std::endl;
        return false;
    }

    std::string sketch_kind_str =
        ini.GetValue("global", "sketch_kind", "CountSketch");
    config.sketch_kind = parse_sketch_kind(sketch_kind_str);

    config.epoch_duration_ns =
        ini.GetLongValue("global", "epoch_ns", 1000000000);
    config.max_epochs = ini.GetLongValue("global", "max_epochs", 0);
    config.full_sketch_depth =
        ini.GetLongValue("global", "full_sketch_depth", 8);
    config.heavy_hitter_ratio =
        ini.GetDoubleValue("global", "heavy_ratio", 0.0001);
    config.enable_progress_bar =
        parse_bool(ini.GetValue("global", "progress_bar", "true"));

    // 解析所有 fragment 配置
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);

    std::unordered_map<std::string, int> fragment_index;

    for (const auto& section : sections) {
        std::string section_name = section.pItem;

        // 跳过非 fragment 和 path section
        if (section_name == "global") {
            continue;
        }
        if (section_name.substr(0, 5) == "path:") {
            continue;
        }
        if (section_name.substr(0, 9) != "fragment:") {
            continue;
        }

        // 解析 fragment
        FragmentSetting frag;
        frag.name = ini.GetValue(section_name.c_str(), "name", "");
        if (frag.name.empty()) {
            // 使用 section 名去掉 "fragment:" 前缀
            frag.name = section_name.substr(9);
        }

        std::string kind_str = ini.GetValue(section_name.c_str(), "kind", "");
        if (!kind_str.empty()) {
            frag.kind = parse_sketch_kind(kind_str);
        } else {
            frag.kind = config.sketch_kind;  // 默认使用全局设置
        }

        frag.memory_bytes =
            ini.GetLongValue(section_name.c_str(), "memory", 8 * 1024 * 1024);
        frag.depth = ini.GetLongValue(section_name.c_str(), "depth", 1);
        frag.initial_subepoch =
            ini.GetLongValue(section_name.c_str(), "initial_subepoch", 1);
        frag.max_subepoch = ini.GetLongValue(
            section_name.c_str(), "max_subepoch", frag.initial_subepoch);
        frag.rho_target =
            ini.GetDoubleValue(section_name.c_str(), "rho_target", 1);

        std::string boost_str =
            ini.GetValue(section_name.c_str(), "boost_single_hop", "false");
        frag.boost_single_hop = parse_bool(boost_str);

        // 验证并修正配置
        if (frag.depth == 0) {
            frag.depth = 1;
        }
        if (frag.max_subepoch < frag.initial_subepoch) {
            frag.max_subepoch = frag.initial_subepoch;
        }

        fragment_index[frag.name] =
            static_cast<int>(config.topology.fragments.size());
        config.topology.fragments.push_back(frag);
    }

    if (config.topology.fragments.empty()) {
        std::cerr << "配置缺少 fragment 定义" << std::endl;
        return false;
    }

    // 解析所有 path
    for (const auto& section : sections) {
        std::string section_name = section.pItem;

        if (section_name.substr(0, 5) != "path:") {
            continue;
        }

        PathSetting path;
        path.name = ini.GetValue(section_name.c_str(), "name", "");
        if (path.name.empty()) {
            // 使用 section 名去掉 "path:" 前缀
            path.name = section_name.substr(5);
        }

        std::string nodes_str = ini.GetValue(section_name.c_str(), "nodes", "");
        if (nodes_str.empty()) {
            std::cerr << "路径 " << section_name << " 缺少 nodes 定义"
                      << std::endl;
            return false;
        }

        // 解析逗号分隔的节点名称
        std::stringstream ss(nodes_str);
        std::string node_name;
        while (std::getline(ss, node_name, ',')) {
            // 去除首尾空格
            size_t start = node_name.find_first_not_of(" \t\r\n");
            size_t end = node_name.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                node_name = node_name.substr(start, end - start + 1);
            }

            auto it = fragment_index.find(node_name);
            if (it == fragment_index.end()) {
                std::cerr << "路径 " << section_name
                          << " 中的节点未定义: " << node_name << std::endl;
                return false;
            }
            path.node_indices.push_back(it->second);
        }

        if (path.node_indices.empty()) {
            std::cerr << "路径 " << section_name << " 没有有效节点"
                      << std::endl;
            return false;
        }

        config.topology.paths.push_back(path);
    }

    if (config.topology.paths.empty()) {
        std::cerr << "配置缺少 path 定义" << std::endl;
        return false;
    }

    return true;
}

SketchKind ConfigParser::parse_sketch_kind(const std::string& value) const {
    if (value == "CountMin" || value == "countmin") {
        return SketchKind::CountMin;
    } else if (value == "UnivMon" || value == "univmon") {
        return SketchKind::UnivMon;
    } else if (value == "CountSketch" || value == "countsketch") {
        return SketchKind::CountSketch;
    }
    return SketchKind::CountSketch;  // 默认值
}

bool ConfigParser::parse_bool(const std::string& value) const {
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "True";
}
