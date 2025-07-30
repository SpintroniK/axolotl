#pragma once

#include "Chunk.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>

enum class InterpretResult : std::uint8_t
{
    Ok,
    CompileError,
    RuntimeError,
};


template <typename T, std::size_t Size>
class Stack
{
public:
    void push(const T& value)
    {
        data[stack_top++] = value;
    }

    T pop()
    {
        return data[--stack_top];
    }

    void reset()
    {
        stack_top = 0;
    }

private:
    std::array<T, Size> data{};
    std::size_t stack_top = 0;
};

class Vm
{
public:
    [[nodiscard]] InterpretResult interpret(Chunk code)
    {
        chunk = std::move(code);
        ip = 0;
        return run();
    }

private:
    template <typename F>
    void binary_op(F&& func)
    {
        const auto rhs = stack.pop();
        const auto lhs = stack.pop();
        stack.push(std::invoke(std::forward<F>(func), lhs, rhs));
    }

    [[nodiscard]] InterpretResult run()
    {
        for (;;)
        {
            const auto instruction = read_byte_as<OpCode>();
            switch (instruction)
            {
            case OpCode::Return:
            {
                std::cout << stack.pop() << '\n';
                return InterpretResult::Ok;
            }
            case OpCode::Negate:
            {
                stack.push(-stack.pop());
                break;
            }
            case OpCode::Add:
            {
                binary_op(std::plus<Value>{});
                break;
            }
            case OpCode::Subtract:
            {
                binary_op(std::minus<Value>{});
                break;
            }
            case OpCode::Mutliply:
            {
                binary_op(std::multiplies<Value>{});
                break;
            }
            case OpCode::Divide:
            {
                binary_op(std::divides<Value>{});
                break;
            }
            case OpCode::Constant:
            {
                const auto constant = chunk.constants[read_byte_as<std::uint8_t>()];
                stack.push(constant);
                break;
            }
            }
        }
    }

    template <typename T = std::byte>
    [[nodiscard]] constexpr auto read_byte_as() noexcept -> T
    {
        return static_cast<T>(chunk.data[ip++]);
    }

    void reset_stack()
    {
        stack.reset();
    }

    static constexpr auto stack_size = 256U;

    Chunk chunk;
    std::size_t ip = 0;
    Stack<Value, stack_size> stack;
};