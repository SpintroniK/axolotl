#pragma once

#include "Chunk.hpp"
#include "Debug.hpp"
#include "Scanner.hpp"
#include "Value.hpp"

#include <charconv>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>


enum class Precedence : std::uint8_t
{
    NONE,
    ASSIGNMENT, // =
    OR,         // or
    AND,        // and
    EQUALITY,   // == !=
    COMPARISON, // < > <= >=
    TERM,       // + -
    FACTOR,     // * /
    UNARY,      // ! -
    CALL,       // . ()
    PRIMARY
};

class Compiler;
using ParseFn = void (Compiler::*)();

struct ParseRule
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};


struct Parser
{
    Token previous;
    Token current;
    bool had_error = false;
    bool panic_mode = false;
};

class Compiler
{
public:
    std::optional<Chunk> compile(std::string_view source)
    {
        compiling_chunk = Chunk{};

        parser.panic_mode = false;
        parser.had_error = false;

        advance();
        expression();
        consume(TokenType::Eof, "Expected end of expression.");

        end_compiler();

        if (parser.had_error)
        {
            return std::nullopt;
        }

        return compiling_chunk;
    }

private:
    auto advance() -> void
    {
        parser.previous = parser.current;

        for (;;)
        {
            parser.current = scanner.scan_token();
            if (parser.current.get_type() != TokenType::ERROR)
            {
                break;
            }
            error_at_current(parser.current.get_lexme());
        }
    }

    auto consume(TokenType token_type, std::string_view message) -> void
    {
        if (parser.current.get_type() == token_type)
        {
            advance();
            return;
        }

        error_at_current(message);
    }

    auto expression() -> void
    {
        parse_precedence(Precedence::ASSIGNMENT);
    }

    auto emit_constant(const Value& value) -> void
    {
        emit_bytes(OpCode::Constant, make_constant(value));
    }

    auto make_constant(const Value& value) -> std::uint8_t
    {
        const auto constant = current_chunk().add_constant(value);
        if (constant > std::numeric_limits<std::uint8_t>::max())
        {
            error("Too many constants in one chunk.");
            return 0;
        }
        return constant;
    }

    auto number() -> void
    {
        Number val{};
        const auto result = std::from_chars(parser.previous.get_lexme().begin(), parser.previous.get_lexme().end(), val);

        emit_constant(values::make(val));
    }

    auto grouping() -> void
    {
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    }

    auto unary() -> void
    {
        const auto operator_type = parser.previous.get_type();

        parse_precedence(Precedence::UNARY);

        switch (operator_type)
        {
        case TokenType::MINUS: emit_byte(OpCode::Negate); break;
        default: return;
        }
    }


    auto binary() -> void
    {
        const auto operator_type = parser.previous.get_type();
        ParseRule rule = get_rule(operator_type);
        parse_precedence(static_cast<Precedence>(static_cast<std::underlying_type_t<Precedence>>(rule.precedence) + 1));

        switch (operator_type)
        {
        case TokenType::PLUS: emit_byte(OpCode::Add); break;
        case TokenType::MINUS: emit_byte(OpCode::Subtract); break;
        case TokenType::STAR: emit_byte(OpCode::Mutliply); break;
        case TokenType::SLASH: emit_byte(OpCode::Divide); break;
        default: return; // Unreachable.
        }
    }


    auto parse_precedence(Precedence precedence) -> void
    {
        advance();
        auto prefix_rule = get_rule(parser.previous.get_type()).prefix;
        if (prefix_rule == nullptr)
        {
            error("Expected expression.");
            return;
        }

        std::invoke(prefix_rule, this);

        while (precedence <= get_rule(parser.current.get_type()).precedence)
        {
            advance();
            auto infix_rule = get_rule(parser.previous.get_type()).infix;
            std::invoke(infix_rule, this);
        }
    }

    auto error_at_current(std::string_view message) -> void
    {
        error_at(parser.previous, message);
    }

    auto error(std::string_view message) -> void
    {
        error_at(parser.previous, message);
    }

    auto error_at(const Token& token, std::string_view message) -> void
    {
        if (parser.panic_mode)
        {
            return;
        }

        parser.panic_mode = true;
        std::cout << " Error";

        if (token.get_type() == TokenType::Eof)
        {
            std::cout << " at end";
        }
        else if (token.get_type() == TokenType::ERROR)
        {
        }
        else
        {
            std::cout << " at '" << token.get_lexme() << "'";
        }

        std::cout << ": " << message << '\n';
        parser.had_error = true;
    }

    auto end_compiler() -> void
    {
        emit_return();
        if (!parser.had_error && debug::enabled)
        {
            debug::Debug::dissassemble_chunk(current_chunk(), "code");
        }
    }

    auto emit_return() -> void
    {
        emit_byte(OpCode::Return);
    }

    template <typename T>
    auto emit_byte(T byte) -> void
    {
        current_chunk().write(byte, parser.previous.get_line());
    }

    template <typename... T>
    auto emit_bytes(T... bytes) -> void
    {
        (emit_byte(bytes), ...);
    }

    [[nodiscard]] auto current_chunk() const noexcept -> Chunk
    {
        return compiling_chunk;
    }

    [[nodiscard]] auto get_rule(TokenType type) const -> ParseRule
    {
        return rules[static_cast<std::size_t>(type)];
    }


    std::vector<ParseRule> rules{
        { .prefix = &Compiler::grouping, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::unary, .infix = &Compiler::binary, .precedence = Precedence::TERM },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::TERM },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::FACTOR },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::FACTOR },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::number, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
    };

    Parser parser;
    Scanner scanner;
    Chunk compiling_chunk;
};