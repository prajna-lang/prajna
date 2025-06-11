#pragma once
#include <fstream>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"

namespace prajna {

class GlobalConfig {
   public:
    static GlobalConfig& Instance();

    bool Load(std::string& config_path = ".matazure_config.hson");

    nlohmann::json& GetConfig() { return config_; }

    std::string GetOutDir() { return "Temp"; }

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

bool GlobalConfig::Load(std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_path << std::endl;
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