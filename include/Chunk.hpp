#pragma once

#include "Value.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class OpCode : std::uint8_t
{
    Constant,
    Nil,
    True,
    False,
    Pop,
    GetLocal,
    Setlocal,
    GetGlobal,
    DefineGlobal,
    SetGlobal,
    Equal,
    Greater,
    Less,
    Add,
    Subtract,
    Mutliply,
    Divide,
    Not,
    Negate,
    Print,
    Jump,
    JumpIfFalse,
    Loop,
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
    auto write(T byte, std::size_t line) -> void
    {
        data.emplace_back(static_cast<std::byte>(byte));
        lines.push_back(line);
    }

    auto add_constant(const Value& value) -> std::size_t
    {
        constants.emplace_back(value);
        return constants.size() - 1;
    }

    template <typename T>
    auto set(std::size_t index, const T& value) -> void
    {
        data[index] = value;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return data.size();
    }

private:
    std::vector<std::byte> data;
    ValueArray constants;
    std::vector<std::size_t> lines;
};