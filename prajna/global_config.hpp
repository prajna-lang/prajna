#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "boost/property_tree/json_parser.hpp"
#include "boost/property_tree/ptree.hpp"
#include "nlohmann/json.hpp"

namespace prajna {

class GlobalConfig {
   public:
    static boost::property_tree::ptree& Instance();

   private:
    GlobalConfig() = default;
    ~GlobalConfig() = default;
    GlobalConfig(GlobalConfig&) = delete;
    GlobalConfig& operator=(GlobalConfig&) = delete;

    nlohmann::json config_;
};

inline boost::property_tree::ptree& GlobalConfig::Instance() {
    static boost::property_tree::ptree instance;
    if (instance.empty()) {
        try {
            boost::property_tree::read_json(
                std::filesystem::current_path() / "matazure_config.json", instance);
        } catch (std::exception& e) {
            instance.put("error", "notexits");
            // Do none, get will give default value
        }
    }
    return instance;
}

}  // namespace prajna
