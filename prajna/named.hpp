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

inline std::string MangleNvvmName(std::string name) {
    if (name.find("__nv_") != std::string::npos) {
        return name;
    }

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
            case '*':
                str_re.append("_P_");
                continue;
        }
        str_re.push_back(*iter);
    }

    return str_re;
}

inline std::string MangleHipName(std::string name) {
    if (name.find("__ocml_") != std::string::npos) {
        return name;
    }

    // 提取函数名（忽略命名空间）
    size_t last_colon = name.rfind("::");
    std::string simple_name =
        (last_colon == std::string::npos) ? name : name.substr(last_colon + 2);

    // 替换所有非字母数字字符为下划线
    std::string str_re;
    str_re.reserve(simple_name.size());
    for (char c : simple_name) {
        if (std::isalnum(c) || c == '.') {
            str_re.push_back(c);
        } else {
            str_re.push_back('.');
        }
    }

    // 确保名称非空且有效
    if (str_re.empty()) {
        str_re = "kernel";
    }

    return str_re;
}

}  // namespace prajna
