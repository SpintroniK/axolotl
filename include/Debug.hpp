#pragma once

#include "Chunk.hpp"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace debug
{
    class Debug
    {
    public:
        static void dissassemble_chunk(const Chunk& chunk, std::string_view name)
        {
            std::cout << "== " << name << " ==\n";
            for (std::size_t offset = 0; offset < chunk.data.size();)
            {
                offset = dissassemble_instruction(chunk, offset);
            }
        }

        static std::size_t dissassemble_instruction(const Chunk& chunk, std::size_t offset)
        {
            std::cout << std::setw(4) << std::setfill('0') << offset << ' ';

            if (offset > 0 && chunk.lines[offset] == chunk.lines[offset - 1])
            {
                std::cout << "   | ";
            }
            else
            {
                std::cout << std::setw(4) << std::setfill('0') << chunk.lines[offset] << ' ';
            }

            const auto instruction = static_cast<OpCode>(chunk.data[offset]);
            switch (instruction)
            {
            case OpCode::Constant: return constant_instruction("CONSTANT", chunk, offset);
            case OpCode::Negate: return simple_instruction("NEGATE", offset);
            case OpCode::Add: return simple_instruction("ADD", offset);
            case OpCode::Subtract: return simple_instruction("SUBTRACT", offset);
            case OpCode::Mutliply: return simple_instruction("MULTIPLY", offset);
            case OpCode::Divide: return simple_instruction("DIVIDE", offset);
            case OpCode::Return: return simple_instruction("RETURN", offset);
            default:
                std::cout << "[DEBUG] Unknown opcode: " << static_cast<int>(instruction) << '\n';
                return offset + 1;
            }

            return offset;
        }

        static std::size_t simple_instruction(std::string_view name, std::size_t offset)
        {
            std::cout << name << '\n';
            return offset + 1;
        }

        static std::size_t constant_instruction(std::string_view name, const Chunk& chunk, std::size_t offset)
        {
            const auto constant = static_cast<std::size_t>(chunk.data[offset + 1]);
            std::cout << name << ' ' << constant << '\t';
            print_value(chunk.constants[constant]);
            std::cout << '\n';
            return offset + 2;
        }

        template <typename T>
        static void print_value(const T& value)
        {
            std::cout << value;
        }
    };
} // namespace debug