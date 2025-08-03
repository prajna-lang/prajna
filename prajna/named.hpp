#pragma once

#include <string>

namespace prajna {

class Named {
   public:
    // 默认值为空, 否则llvm的IR不太好看
    std::string name = "NameIsUndefined";
    std::string fullname = "FullnameIsUndefined";
};

inline std::string ConcatFullname(std::string base_name, std::string name) {
    return base_name + "::" + name;
}

}  // namespace prajna
