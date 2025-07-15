#pragma once
#include <string>

namespace prajna::ir {

enum struct Target { none, host, nvptx, amdgpu };

inline std::string TargetToString(Target ir_target) {
    switch (ir_target) {
        case Target::none:
            PRAJNA_UNREACHABLE;
            return "";
        case Target::host:
            return "host";
        case Target::nvptx:
            return "nvptx";
        case Target::amdgpu:
            return "amdgpu";
    }

    return "";
}

inline Target StringToTarget(std::string str_target) {
    if (str_target == "host") return Target::host;
    if (str_target == "nvptx") return Target::nvptx;
    if (str_target == "amdgpu") return Target::amdgpu;

    PRAJNA_UNREACHABLE;
    return Target::none;
}

}  // namespace prajna::ir