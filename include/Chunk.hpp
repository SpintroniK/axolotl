#pragma once

#include "Value.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class OpCode : std::uint8_t
{
    Constant,
    Negate,
    Return,
};

namespace debug
{
    class Debug;
}

class Chunk
{
    friend class debug::Debug;
    friend class Vm;

public:
    template <typename T>
    void write(T byte, std::size_t line)
    {
        data.emplace_back(static_cast<std::byte>(byte));
        lines.push_back(line);
    }

    std::size_t add_constant(Value value)
    {
        constants.emplace_back(value);
        return constants.size() - 1;
    }

private:
    std::vector<std::byte> data;
    ValueArray constants;
    std::vector<std::size_t> lines;
};