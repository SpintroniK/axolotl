#pragma once

#include "Chunk.hpp"
#include "Compiler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
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

    auto set(std::size_t index, const T& value) -> void
    {
        data[index] = value;
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
    template <typename Func>
    InterpretResult binary_op()
    {
        const auto lhs = stack.pop();
        const auto rhs = stack.pop();

        std::visit(
        [&](const auto& lhs_val, const auto& rhs_val)
        {
            using LhsType = std::decay_t<decltype(lhs_val)>;
            using RhsType = std::decay_t<decltype(rhs_val)>;

            // Ensure the types are the same for the operation
            if constexpr (std::is_same_v<LhsType, RhsType>)
            {
                if constexpr (std::is_same_v<LhsType, Number>)
                {
                    // Push the result of applying the function back onto the stack
                    stack.push(Func{}(lhs_val, rhs_val));
                }
                else if constexpr (std::is_same_v<LhsType, String>)
                {
                    // Handle string concatenation
                    stack.push(lhs_val + rhs_val);
                }
                else
                {
                    // Handle unsupported types
                    throw std::runtime_error("Unsupported type for binary operation");
                }
            }
            else
            {
                // Handle type mismatch here
            }
        },
        rhs, lhs);

        return InterpretResult::Ok;
    }


    [[nodiscard]] InterpretResult run()
    {
        for (;;)
        {
            const auto instruction = read_byte_as<OpCode>();
            switch (instruction)
            {
            case OpCode::Print:
            {
                std::visit(
                []<typename value_t>(const value_t& value)
                {
                    if constexpr (std::is_same_v<value_t, Function>)
                    {
                        std::cout << "<Fn " << value.get_name() << '>';
                    }
                    else
                    {
                        std::cout << value << '\n';
                    }
                },
                stack.pop());
                break;
            }
            case OpCode::Loop:
            {
                const auto offset = read_short();
                ip -= offset;
                break;
            }
            case OpCode::Return:
            {
                return InterpretResult::Ok;
            }
            case OpCode::Jump:
            {
                const auto offset = read_short();
                ip += offset;
                break;
            }
            case OpCode::JumpIfFalse:
            {
                const auto offset = read_short();
                if (is_falsey(peek(0)))
                {
                    ip += offset;
                }
                break;
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
                binary_op<std::plus<>>();
                break;
            }
            case OpCode::Subtract:
            {
                binary_op<std::minus<>>();
                break;
            }
            case OpCode::Mutliply:
            {
                binary_op<std::multiplies<>>();
                break;
            }
            case OpCode::Divide:
            {
                binary_op<std::divides<>>();
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
            case OpCode::Pop:
            {
                stack.pop();
                break;
            }
            case OpCode::GetLocal:
            {
                const auto slot = read_byte_as<std::uint8_t>();
                stack.push(stack.at(slot));
                break;
            }
            case OpCode::Setlocal:
            {
                const auto slot = read_byte_as<std::uint8_t>();
                stack.set(slot, peek(0));
                break;
            }
            case OpCode::GetGlobal:
            {
                const auto constant = chunk.constants[read_byte_as<std::uint8_t>()];
                const auto name = std::get<String>(constant);
                if (!globals.contains(name))
                {
                    // TODO: implement
                    //  runtime error => Undefined variable ''.
                    return InterpretResult::RuntimeError;
                }

                stack.push(globals[name]);
                break;
            }
            case OpCode::DefineGlobal:
            {
                const auto constant = chunk.constants[read_byte_as<std::uint8_t>()];
                const auto name = std::get<String>(constant);
                globals[name] = stack.pop();
                break;
            }
            case OpCode::SetGlobal:
            {
                const auto constant = chunk.constants[read_byte_as<std::uint8_t>()];
                const auto name = std::get<String>(constant);
                if (!globals.contains(name))
                {
                    // TODO: handle error
                    return InterpretResult::RuntimeError;
                }
                globals[name] = peek(0);
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
                binary_op<std::greater<>>();
                break;
            }
            case OpCode::Less:
            {
                binary_op<std::less<>>();
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

    [[nodiscard]] constexpr auto read_short() noexcept -> std::uint16_t
    {
        ip += 2;
        return static_cast<std::uint16_t>((chunk.data[ip - 2] << 8) | chunk.data[ip - 1]);
    }


    auto peek(int offset) -> Value
    {
        return stack.at(stack.top() + offset - 1);
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
    std::map<std::string, Value> globals;
};