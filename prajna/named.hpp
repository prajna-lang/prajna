#pragma once

#include <string>

namespace prajna {

class Named {
   public:
    // 默认值为空, 否则llvm的IR不太好看
    std::string name = "";
    std::string fullname = "";
};

inline std::string ConcatFullname(std::string base_name, std::string name) {
    return base_name + "::" + name;
}

inline std::string MangleNvvmName(std::string name) {
    std::string str_re;
    for (auto iter = name.begin(); iter != name.end(); ++iter) {
        switch (*iter) {
            case ':':
                str_re.append("_N_");
                continue;
            case '<':
                str_re.append("T_");
                continue;
            case '>':
                str_re.append("T_");
            case ',':
                str_re.append("_C_");
                continue;
            case ' ':
                str_re.append("_S_");
                continue;
        }
        str_re.push_back(*iter);
    }

    return str_re;
}

}  // namespace prajna
