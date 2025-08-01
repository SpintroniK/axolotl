#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string_view>

enum class TokenType : std::uint8_t
{
    // Single-character tokens.
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    MINUS,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,
    // One or two character tokens.
    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    // Literals.
    IDENTIFIER,
    STRING,
    NUMBER,
    // Keywords.
    AND,
    CLASS,
    ELSE,
    FALSE,
    FOR,
    FUN,
    IF,
    NIL,
    OR,
    PRINT,
    RETURN,
    SUPER,
    THIS,
    TRUE,
    VAR,
    WHILE,

    ERROR,
    Eof
};


class Token
{
public:
    explicit Token(TokenType type) : type{ type }
    {
    }

    Token(TokenType type, std::string_view lexme) : type{ type }, lexme{ lexme }
    {
    }

    Token(TokenType type, std::string_view lexme, std::size_t line) : type{ type }, lexme{ lexme }, line{ line }
    {
    }

    [[nodiscard]] auto get_type() const noexcept -> TokenType
    {
        return type;
    }

    [[nodiscard]] auto get_lexme() const noexcept -> std::string_view
    {
        return lexme;
    }

    [[nodiscard]] auto get_line() const noexcept -> std::size_t
    {
        return line;
    }

private:
    TokenType type;
    std::string_view lexme;
    std::size_t line = 0;
};


class Scanner
{
public:
    explicit Scanner(std::string_view source) : source{ source }
    {
    }

    Token scan_token()
    {

        skip_white_space();
        start = current;

        if (is_at_end())
        {
            return Token{ TokenType::Eof };
        }

        const auto c = advance();

        if (is_alpha(c))
        {
            return identifier();
        }

        if (std::isdigit(c) != 0)
        {
            return number();
        }

        switch (c)
        {
        case '(': return make_token(TokenType::LEFT_PAREN);
        case ')': return make_token(TokenType::RIGHT_PAREN);
        case '{': return make_token(TokenType::LEFT_BRACE);
        case '}': return make_token(TokenType::RIGHT_BRACE);
        case ';': return make_token(TokenType::SEMICOLON);
        case ',': return make_token(TokenType::COMMA);
        case '.': return make_token(TokenType::DOT);
        case '-': return make_token(TokenType::MINUS);
        case '+': return make_token(TokenType::PLUS);
        case '/': return make_token(TokenType::SLASH);
        case '*': return make_token(TokenType::STAR);
        case '!': return make_token(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
        case '=': return make_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
        case '<': return make_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
        case '>': return make_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
        case '"': return string();
        }

        return error_token("Unexpected character.");
    }

private:
    [[nodiscard]] auto make_token(TokenType type) const -> Token
    {
        return Token{ type, std::string_view{ source.begin() + start, source.begin() + current }, line };
    }

    [[nodiscard]] auto error_token(std::string_view message) const -> Token
    {
        return Token{ TokenType::ERROR, message, line };
    }

    [[nodiscard]] auto is_at_end() const noexcept -> bool
    {
        return source[current] == '\0';
    }

    auto advance() noexcept -> std::string_view::value_type
    {
        return source[current++];
    }


    [[nodiscard]] auto match(char expected) noexcept -> bool
    {
        if (is_at_end())
        {
            return false;
        }

        if (source[current] != expected)
        {
            return false;
        }

        ++current;
        return true;
    }

    [[nodiscard]] auto peek() const noexcept -> char
    {
        return source[current];
    }

    [[nodiscard]] auto peek_next() const noexcept -> char
    {
        if (is_at_end())
        {
            return '\0';
        }

        return source[current + 1];
    }

    void skip_white_space()
    {
        for (;;)
        {
            const auto c = peek();
            switch (c)
            {
            case ' ':
            case '\r':
            case '\t': advance(); break;
            case '\n':
                ++line;
                advance();
                break;
            case '/':
                if (peek_next() == '/')
                {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !is_at_end())
                    {
                        advance();
                    }
                }
                else
                {
                    return;
                }
                break;
            default: return;
            }
        }
    }

    [[nodiscard]] auto string() -> Token
    {
        while (peek() != '"' && !is_at_end())
        {
            if (peek() == '\n')
            {
                ++line;
            }
            advance();
        }

        if (is_at_end())
        {
            return error_token("Unterminated string.");
        }

        advance();
        return make_token(TokenType::STRING);
    }

    [[nodiscard]] auto number() -> Token
    {
        while (std::isdigit(peek()) != 0)
        {
            advance();
        }

        if (peek() == '.' && (std::isdigit(peek_next()) != 0))
        {
            advance();
        }

        while (std::isdigit(peek()) != 0)
        {
            advance();
        }

        return make_token(TokenType::NUMBER);
    }

    auto identifier() -> Token
    {
        while (is_alpha(peek()) || (std::isdigit(peek()) != 0))
        {
            advance();
        }
        return make_token(identifier_type());
    }

    auto check_keyword(std::size_t beg, std::string_view rest, TokenType type) -> TokenType
    {
        const auto expected = std::string_view{ source.begin() + start + beg, source.begin() + start + beg + rest.length() };
        if (current - start == start + rest.length() && rest == expected)
        {
            return type;
        }

        return type;
    }

    auto identifier_type() -> TokenType
    {
        switch (source[start])
        {
        case 'a': return check_keyword(1, "nd", TokenType::AND);
        case 'c': return check_keyword(1, "lass", TokenType::CLASS);
        case 'e': return check_keyword(1, "lse", TokenType::ELSE);
        case 'f':
            if (current - start > 1)
            {
                switch (source[start + 1])
                {
                case 'a': return check_keyword(2, "lse", TokenType::FALSE);
                case 'o': return check_keyword(2, "r", TokenType::FOR);
                case 'u': return check_keyword(2, "n", TokenType::FUN);
                }
            }
            break;
        case 'i': return check_keyword(1, "f", TokenType::IF);
        case 'n': return check_keyword(1, "il", TokenType::NIL);
        case 'o': return check_keyword(1, "r", TokenType::OR);
        case 'p': return check_keyword(1, "rint", TokenType::PRINT);
        case 'r': return check_keyword(1, "eturn", TokenType::RETURN);
        case 's': return check_keyword(1, "uper", TokenType::SUPER);
        case 't':
            if (current - start > 1)
            {
                switch (source[start + 1])
                {
                case 'h': return check_keyword(2, "is", TokenType::THIS);
                case 'r': return check_keyword(2, "ue", TokenType::TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, "ar", TokenType::VAR);
        case 'w': return check_keyword(1, "hile", TokenType::WHILE);
        }

        return TokenType::IDENTIFIER;
    }

    static auto is_alpha(char c) -> bool
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }


    std::string_view source;

    std::size_t start = 0;
    std::size_t current = 0;
    std::size_t line = 0;
};