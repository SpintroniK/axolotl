#pragma once

#include "Chunk.hpp"
#include "Debug.hpp"
#include "Scanner.hpp"
#include "Value.hpp"

#include <array>
#include <charconv>
#include <cstdint>
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

class Local
{
public:
    Local() : token{ TokenType::Eof }
    {
    }

    Local(const Token& token, int depth) : token{ token }, depth{ depth }
    {
    }

    auto set_depth(int desired_depth) -> void
    {
        depth = desired_depth;
    }

    [[nodiscard]] auto get_depth() const noexcept -> int
    {
        return depth;
    }

    [[nodiscard]] auto get_token() const noexcept -> Token
    {
        return token;
    }

private:
    Token token{ TokenType::Eof };
    int depth = -1;
};

class CompilerState
{
public:
    auto begin_scope() noexcept -> void
    {
        scope_depth++;
    }

    auto end_scope() noexcept -> void
    {
        scope_depth--;
    }


    template <typename F>
    auto clean_scope(F func) -> void
    {
        while (local_count > 0 && locals[local_count - 1].get_depth() > scope_depth)
        {
            std::invoke(func);
            local_count--;
        }
    }

    auto add_local(const Token& token) -> bool
    {
        if (local_count == locals.size() - 1)
        {
            return false;
        }

        locals[local_count++] = Local{ token, -1 };

        return true;
    }

    [[nodiscard]] auto find(const Token& token) const -> int
    {
        if (local_count == 0)
        {
            return -1;
        }

        for (int i = static_cast<int>(local_count - 1); i >= 0; i--)
        {
            const Local& local = locals[i];
            if (local.get_depth() != -1 && local.get_depth() < scope_depth)
            {
                break;
            }

            if (token.get_lexme() == local.get_token().get_lexme())
            {
                if (local.get_depth() == -1)
                {
                    return -2;
                }
                return i;
            }
        }

        return -1;
    }

    auto set_local_depth(std::size_t index, int depth)
    {
        locals[index].set_depth(depth);
    }

    [[nodiscard]] auto get_scope_depth() const noexcept -> std::size_t
    {
        return scope_depth;
    }

    [[nodiscard]] auto get_local_count() const noexcept -> std::size_t
    {
        return local_count;
    }

    [[nodiscard]] auto get_local(std::size_t index) const noexcept -> Local
    {
        return locals[index];
    }

private:
    std::array<Local, std::numeric_limits<std::uint8_t>::max() + 1> locals{};
    std::size_t local_count = 0;
    std::size_t scope_depth = 0;
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
        current_state = CompilerState{};

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

    auto block() -> void
    {
        while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::Eof))
        {
            declaration();
        }

        consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
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

        declare_variable();

        if (current_state.get_scope_depth() > 0)
        {
            return 0;
        }

        return identifier_constant(parser.previous);
    }

    auto identifier_constant(const Token& token) -> std::uint8_t
    {
        return make_constant(std::string{ token.get_lexme().begin(), token.get_lexme().end() });
    }

    auto add_local(const Token& token) -> void
    {
        if (!current_state.add_local(token))
        {
            error("Too many local variables in function.");
        }
    }

    auto declare_variable() -> void
    {
        if (current_state.get_scope_depth() == 0)
        {
            return;
        }

        if (current_state.find(parser.previous) != -1)
        {
            error("Already a variable with this name in this scope.");
        }

        add_local(parser.previous);
    }

    auto mark_initialized() -> void
    {
        current_state.set_local_depth(current_state.get_local_count() - 1, static_cast<int>(current_state.get_scope_depth()));
    }

    auto define_variable(std::uint8_t global) -> void
    {
        if (current_state.get_scope_depth() > 0)
        {
            mark_initialized();
            return;
        }

        emit_bytes(OpCode::DefineGlobal, global);
    }

    auto and_([[maybe_unused]] bool can_assign) -> void
    {
        const auto end_jump = emit_jump(OpCode::JumpIfFalse);

        emit_byte(OpCode::Pop);
        parse_precedence(Precedence::AND);

        patch_jump(static_cast<int>(end_jump));
    }

    auto or_([[maybe_unused]] bool can_assign) -> void
    {
        const auto else_jump = emit_jump(OpCode::JumpIfFalse);
        const auto end_jump = emit_jump(OpCode::Jump);

        patch_jump(static_cast<int>(else_jump));
        emit_jump(OpCode::Pop);

        parse_precedence(Precedence::OR);
        patch_jump(static_cast<int>(end_jump));
    }

    auto expression_statement() -> void
    {
        expression();
        consume(TokenType::SEMICOLON, "Expect ';' after expression.");
        emit_byte(OpCode::Pop);
    }

    auto emit_jump(OpCode instruction) -> std::size_t
    {
        emit_byte(instruction);
        emit_byte(static_cast<std::uint8_t>(0xFF));
        emit_byte(static_cast<std::uint8_t>(0xFF));

        return current_chunk().size() - 2;
    }

    auto patch_jump(int offset) -> void
    {
        const auto jump = current_chunk().size() - offset - 2;

        if (jump > std::numeric_limits<std::uint16_t>::max())
        {
            error("Too much code to jump over.");
        }

        current_chunk().set(offset, static_cast<std::byte>((jump >> 8) & 0xFF));
        current_chunk().set(offset + 1, static_cast<std::byte>(jump & 0xFF));
    }

    auto if_statement() -> void
    {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

        const auto then_jump = emit_jump(OpCode::JumpIfFalse);
        emit_byte(OpCode::Pop);
        statement();

        const auto else_jump = emit_jump(OpCode::Jump);

        patch_jump(static_cast<int>(then_jump));
        emit_byte(OpCode::Pop);

        if (match(TokenType::ELSE))
        {
            statement();
        }

        patch_jump(else_jump);
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
        auto arg = resolve_local(token);

        auto get_op = static_cast<std::uint8_t>(OpCode::GetLocal);
        auto set_op = static_cast<std::uint8_t>(OpCode::Setlocal);

        if (arg == -1)
        {
            arg = identifier_constant(token);
            get_op = static_cast<std::uint8_t>(OpCode::GetGlobal);
            set_op = static_cast<std::uint8_t>(OpCode::SetGlobal);
        }

        if (can_assign && match(TokenType::EQUAL))
        {
            expression();
            emit_bytes(set_op, arg);
        }
        else
        {
            emit_bytes(get_op, arg);
        }
    }

    auto resolve_local(const Token& token) -> int
    {
        const auto found = current_state.find(token);

        if (found == -2)
        {
            error("Can't read local variable in its own initializer.");
        }

        return found;
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

    auto while_statement() -> void
    {
        const auto loop_start = current_chunk().size();
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

        const auto exit_jump = emit_jump(OpCode::JumpIfFalse);
        emit_byte(OpCode::Pop);
        statement();
        emit_loop(loop_start);

        patch_jump(static_cast<int>(exit_jump));
        emit_byte(OpCode::Pop);
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
        else if (match(TokenType::IF))
        {
            if_statement();
        }
        else if (match(TokenType::WHILE))
        {
            while_statement();
        }
        else if (match(TokenType::LEFT_BRACE))
        {
            begin_scope();
            block();
            end_scope();
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

    auto begin_scope() -> void
    {
        current_state.begin_scope();
    }

    auto end_scope() -> void
    {
        current_state.end_scope();
        current_state.clean_scope([&] { emit_byte(OpCode::Pop); });
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

    auto emit_loop(int loop_start) -> void
    {
        emit_byte(OpCode::Loop);
        const auto offset = current_chunk().size() - loop_start + 2;

        if (offset > std::numeric_limits<std::uint16_t>::max())
        {
            error("Loop body too large.");
        }

        emit_byte((offset >> 8) & 0XFF);
        emit_byte(offset & 0XFF);
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
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::EQUALITY },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = &Compiler::binary, .precedence = Precedence::COMPARISON },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::variable, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::string, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::number, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = &Compiler::and_, .precedence = Precedence::AND },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::literal, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = &Compiler::literal, .infix = nullptr, .precedence = Precedence::NONE },
        { .prefix = nullptr, .infix = &Compiler::or_, .precedence = Precedence::OR },
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
    CompilerState current_state;
};