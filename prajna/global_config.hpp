#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"

namespace prajna {

class GlobalConfig {
   public:
    static GlobalConfig& Instance();

    bool Load(const std::string& config_path = "");

    nlohmann::json& GetConfig() { return config_; }

    std::string GetOutputDir() { return "Temp"; }

   private:
    GlobalConfig() = default;
    ~GlobalConfig() = default;
    GlobalConfig(GlobalConfig&) = delete;
    GlobalConfig& operator=(GlobalConfig&) = delete;

    nlohmann::json config_;
};

GlobalConfig& GlobalConfig::Instance() {
    static GlobalConfig instance;
    return instance;
}

bool GlobalConfig::Load(const std::string& config_path) {
    std::string actual_path = config_path;
    if (config_path.empty()) {
        // 默认路径是与当前源文件（global_config.cpp）同目录
        std::filesystem::path base_path = std::filesystem::path(__FILE__).parent_path();
        actual_path = (base_path / ".matazure_config.json").string();
    }

    std::cout << "Trying to load: " << actual_path << std::endl;

    std::ifstream file(actual_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << actual_path << std::endl;
        return false;
    }
    try {
        file >> config_;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }

    return true;
}

}  // namespace prajna