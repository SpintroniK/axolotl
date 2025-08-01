#pragma once

#include "Chunk.hpp"
#include "Compiler.hpp"
#include "Value.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string_view>
#include <variant>

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

    [[nodiscard]] auto at(std::size_t index) const noexcept -> T
    {
        return data[index];
    }

    [[nodiscard]] auto top() const noexcept -> std::size_t
    {
        return stack_top;
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

    [[nodiscard]] InterpretResult interpret(Compiler& compiler)
    {
        auto compiled_chunk = compiler.compile();
        if (!compiled_chunk)
        {
            return InterpretResult::CompileError;
        }

        chunk = compiled_chunk.value();
        ip = 0;
        return run();
    }

private:
    template <typename F>
    InterpretResult binary_op(F&& func)
    {
        if (!values::is<Number>(peek(0)) || !values::is<Number>(peek(1)))
        {
            // TODO: implement
            //  runtime_error();
            return InterpretResult::RuntimeError;
        }

        const auto rhs = values::as<Number>(stack.pop());
        const auto lhs = values::as<Number>(stack.pop());
        stack.push(std::invoke(std::forward<F>(func), lhs, rhs));
        return InterpretResult::Ok;
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
                std::visit([](const auto& value) { std::cout << "Return: " << value << '\n'; }, stack.pop());
                return InterpretResult::Ok;
            }
            case OpCode::Negate:
            {
                if (values::is<Number>(peek(0)))
                {
                    // TODO: implement
                    // runtime_error("Operand must be a number.");
                    return InterpretResult::RuntimeError;
                }
                stack.push(values::make(-values::as<Number>(stack.pop())));
                break;
            }
            case OpCode::Add:
            {
                binary_op(std::plus<Number>{});
                break;
            }
            case OpCode::Subtract:
            {
                binary_op(std::minus<Number>{});
                break;
            }
            case OpCode::Mutliply:
            {
                binary_op(std::multiplies<Number>{});
                break;
            }
            case OpCode::Divide:
            {
                binary_op(std::divides<Number>{});
                break;
            }
            case OpCode::Not:
            {
                stack.push(is_falsey(stack.pop()));
                break;
            }
            case OpCode::Constant:
            {
                const auto constant = chunk.constants[read_byte_as<std::uint8_t>()];
                stack.push(constant);
                break;
            }
            case OpCode::Nil:
            {
                stack.push(values::make(double{ 0 }));
                break;
            }
            case OpCode::True:
            {
                stack.push(values::make(true));
                break;
            }
            case OpCode::False:
            {
                stack.push(values::make(false));
                break;
            }
            case OpCode::Equal:
            {
                const auto b = stack.pop();
                const auto a = stack.pop();
                stack.push(a == b);
                break;
            }
            case OpCode::Greater:
            {
                binary_op(std::greater<Value>{});
                break;
            }
            case OpCode::Less:
            {
                binary_op(std::less<Value>{});
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


    auto peek(int offset) -> Value
    {
        return stack.at(offset);
    }

    void reset_stack()
    {
        stack.reset();
    }


    static auto is_falsey(const Value& value) -> bool
    {
        return (values::is<Number>(value) && values::as<Number>(value) == 0.) ||
        (values::is<Boolean>(value) && !values::as<Boolean>(value));
    }

    static constexpr auto stack_size = 256U;

    Chunk chunk;
    std::size_t ip = 0;
    Stack<Value, stack_size> stack;
};