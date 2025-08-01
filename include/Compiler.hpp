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
#include <string>
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
using ParseFn = void (Compiler::*)(bool);

struct ParseRule
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};


struct Parser
{
    Token previous{ Token{ TokenType::Eof } };
    Token current{ Token{ TokenType::Eof } };
    bool had_error = false;
    bool panic_mode = false;
};

class Compiler
{
public:
    explicit Compiler(std::string_view source) : scanner{ source }
    {
    }

    std::optional<Chunk> compile()
    {
        compiling_chunk = Chunk{};

        parser.panic_mode = false;
        parser.had_error = false;

        advance();

        while (!match(TokenType::Eof))
        {
            declaration();
        }

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

    auto var_declaration() -> void
    {
        const auto global = parse_variable("Expect variable name.");

        if (match(TokenType::EQUAL))
        {
            expression();
        }
        else
        {
            emit_byte(OpCode::Nil);
        }

        consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");

        define_variable(global);
    }

    auto parse_variable(std::string_view error_message) -> std::uint8_t
    {
        consume(TokenType::IDENTIFIER, error_message);
        return identifier_constant(parser.previous);
    }

    auto identifier_constant(const Token& token) -> std::uint8_t
    {
        return make_constant(std::string{ token.get_lexme().begin(), token.get_lexme().end() });
    }

    auto define_variable(std::uint8_t global) -> void
    {
        emit_bytes(OpCode::DefineGlobal, global);
    }

    auto expression_statement() -> void
    {
        expression();
        consume(TokenType::SEMICOLON, "Expect ';' after expression.");
        emit_byte(OpCode::Pop);
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

    auto number([[maybe_unused]] bool can_assign) -> void
    {
        Number val{};
        const auto result = std::from_chars(parser.previous.get_lexme().begin(), parser.previous.get_lexme().end(), val);

        emit_constant(values::make(val));
    }

    auto literal([[maybe_unused]] bool can_assign) -> void
    {
        switch (parser.previous.get_type())
        {
        case TokenType::FALSE: emit_byte(OpCode::False); break;
        case TokenType::NIL: emit_byte(OpCode::Nil); break;
        case TokenType::TRUE: emit_byte(OpCode::True); break;
        default: return;
        }
    }

    auto string([[maybe_unused]] bool can_assign) -> void
    {
        emit_constant(std::string{ parser.previous.get_lexme().begin() + 1, parser.previous.get_lexme().end() - 1 });
    }

    auto variable(bool can_assign) -> void
    {
        named_variable(parser.previous, can_assign);
    }

    auto named_variable(const Token& token, bool can_assign) -> void
    {
        const auto arg = identifier_constant(token);

        if (can_assign && match(TokenType::EQUAL))
        {
            expression();
            emit_bytes(OpCode::SetGlobal, arg);
        }
        else
        {
            emit_bytes(OpCode::GetGlobal, arg);
        }
    }


    auto grouping([[maybe_unused]] bool can_assign) -> void
    {
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    }

    auto unary([[maybe_unused]] bool can_assign) -> void
    {
        const auto operator_type = parser.previous.get_type();

        parse_precedence(Precedence::UNARY);

        switch (operator_type)
        {
        case TokenType::MINUS: emit_byte(OpCode::Negate); break;
        case TokenType::BANG: emit_byte(OpCode::Not); break;
        default: return;
        }
    }


    auto binary([[maybe_unused]] bool can_assign) -> void
    {
        const auto operator_type = parser.previous.get_type();
        ParseRule rule = get_rule(operator_type);
        parse_precedence(static_cast<Precedence>(static_cast<std::underlying_type_t<Precedence>>(rule.precedence) + 1));

        switch (operator_type)
        {
        case TokenType::BANG_EQUAL: emit_bytes(OpCode::Equal, OpCode::Not); break;
        case TokenType::EQUAL_EQUAL: emit_byte(OpCode::Equal); break;
        case TokenType::GREATER: emit_byte(OpCode::Greater); break;
        case TokenType::GREATER_EQUAL: emit_bytes(OpCode::Less, OpCode::Not); break;
        case TokenType::LESS: emit_byte(OpCode::Less); break;
        case TokenType::LESS_EQUAL: emit_bytes(OpCode::Greater, OpCode::Not); break;
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
        const auto type = parser.previous.get_type();
        auto prefix_rule = get_rule(type).prefix;
        if (prefix_rule == nullptr)
        {
            error("Expected expression.");
            return;
        }

        const auto can_assign = precedence <= Precedence::ASSIGNMENT;

        std::invoke(prefix_rule, this, can_assign);


        while (precedence <= get_rule(parser.current.get_type()).precedence)
        {
            advance();
            auto infix_rule = get_rule(parser.previous.get_type()).infix;
            std::invoke(infix_rule, this, can_assign);
        }

        if (can_assign && match(TokenType::EQUAL))
        {
            error("Invalid assignment target.");
        }
    }

    [[nodiscard]] auto check(TokenType type) const -> bool
    {
        return parser.current.get_type() == type;
    }

    auto match(TokenType type) -> bool
    {
        if (!check(type))
        {
            return false;
        }
        advance();
        return true;
    }

    auto declaration() -> void
    {
        if (match(TokenType::VAR))
        {
            var_declaration();
        }
        else
        {
            statement();
        }

        if (parser.panic_mode)
        {
            synchronize();
        }
    }

    auto print_statement() -> void
    {
        expression();
        consume(TokenType::SEMICOLON, "Expect ';' after value.");
        emit_byte(OpCode::Print);
    }

    auto synchronize() -> void
    {
        parser.panic_mode = false;

        while (parser.current.get_type() != TokenType::Eof)
        {
            if (parser.previous.get_type() == TokenType::SEMICOLON)
            {
                return;
            }
            switch (parser.current.get_type())
            {
            case TokenType::CLASS:
            case TokenType::FUN:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
            case TokenType::RETURN: return;

            default:; // Do nothing.
            }

            advance();
        }
    }

    auto statement() -> void
    {
        if (match(TokenType::PRINT))
        {
            print_statement();
        }
        else
        {
            expression_statement();
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

    [[nodiscard]] auto current_chunk() noexcept -> Chunk&
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
        { .prefix = &Compiler::unary, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::EQUALITY },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::EQUALITY },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::variable, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::string, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::number, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::literal, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::literal, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::literal, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
    };

    Parser parser;
    Scanner scanner;
    Chunk compiling_chunk;
};