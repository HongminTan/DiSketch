#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

// 开启 SimpleIni 的通用字符集支持
#define SI_CONVERT_GENERIC

#include "DiSketch.h"
#include "SimpleIni.h"

// 配置文件解析器，使用 SimpleIni 库解析 INI 配置文件
class ConfigParser {
   public:
    ConfigParser() = default;

    /* 从 INI 文件路径加载并解析配置
     * @param ini_path INI 配置文件路径
     * @param config 输出参数，解析后的 DiSketch 配置
     * @return 解析成功返回 true，失败返回 false
     */
    bool parse(const std::string& ini_path, DiSketchConfig& config);

   private:
    /// 解析 Sketch 类型字符串
    SketchKind parse_sketch_kind(const std::string& value) const;

    /// 解析布尔值字符串
    bool parse_bool(const std::string& value) const;
};

#endif  // CONFIG_PARSER_H
