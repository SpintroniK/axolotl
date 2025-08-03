#pragma once

#include "Chunk.hpp"
#include "Value.hpp"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <variant>

namespace debug
{
    static constexpr bool enabled = true;

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
            case OpCode::Print: simple_instruction("PRINT", offset);
            case OpCode::Jump: return jump_instruction("JUMP", 1, chunk, offset);
            case OpCode::JumpIfFalse: return jump_instruction("JUMP_IF_FALSE", 1, chunk, offset);
            case OpCode::Add: return simple_instruction("ADD", offset);
            case OpCode::Subtract: return simple_instruction("SUBTRACT", offset);
            case OpCode::Mutliply: return simple_instruction("MULTIPLY", offset);
            case OpCode::Divide: return simple_instruction("DIVIDE", offset);
            case OpCode::Return: return simple_instruction("RETURN", offset);
            case OpCode::Nil: return simple_instruction("NIL", offset);
            case OpCode::True: return simple_instruction("TRUE", offset);
            case OpCode::False: return simple_instruction("FALSE", offset);
            case OpCode::Pop: return simple_instruction("POP", offset);
            case OpCode::GetLocal: return byte_instruction("GET_LOCAL", chunk, offset);
            case OpCode::Setlocal: return byte_instruction("SET_LOCAL", chunk, offset);
            case OpCode::GetGlobal: return constant_instruction("GET_GLOBAL", chunk, offset);
            case OpCode::DefineGlobal: return constant_instruction("DEFINE_GLOBAL", chunk, offset);
            case OpCode::SetGlobal: return constant_instruction("SET_GLOBAL", chunk, offset);
            case OpCode::Equal: return simple_instruction("EQUAL", offset);
            case OpCode::Greater: return simple_instruction("GREATER", offset);
            case OpCode::Less: return simple_instruction("LESS", offset);
            case OpCode::Not: return simple_instruction("NOT", offset);
            default:
                std::cout << "[DEBUG] Unknown opcode: " << static_cast<int>(instruction) << '\n';
                return offset + 1;
            }

            return offset;
        }

        static std::size_t byte_instruction(std::string_view name, const Chunk& chunk, std::size_t offset)
        {
            const auto slot = chunk.data[offset + 1];

            std::cout << std::left << std::setw(16) << name << std::setw(4) << static_cast<signed char>(slot) << '\n';
            return offset + 2;
        }

        static std::size_t jump_instruction(std::string_view name, int sign, const Chunk& chunk, std::size_t offset)
        {
            auto jump = static_cast<uint16_t>(chunk.data[offset + 1] << 8);
            jump |= static_cast<std::uint16_t>(chunk.data[offset + 2]);
            std::cout << name << "    " << offset << " " << (offset + 3 + sign * jump) << '\n';
            return offset + 3;
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
            std::visit([](const auto& value) { std::cout << value; }, value);
        }
    };
} // namespace debug