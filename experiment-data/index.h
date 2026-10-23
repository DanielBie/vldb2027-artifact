#pragma once

#include <optional>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <string>

namespace Config{
    static const uint16_t Nodesize = 256;
};

typedef uint64_t Key;
typedef uint64_t Value;

std::vector<std::string> counter_list = {"instructions"};

