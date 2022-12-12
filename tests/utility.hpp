#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace prajna::tests {

inline std::vector<std::string> getFiles(std::string dir) {
    std::vector<std::string> files;
    for (const auto &file : std::filesystem::directory_iterator(dir)) {
        files.push_back(file.path().string());
    }

    // 排一下顺序, 这样就能直接根据文件顺序来选择测试
    std::sort(files.begin(), files.end());

    return files;
}

}  // namespace prajna::tests
