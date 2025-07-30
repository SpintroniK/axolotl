#pragma once

#include "Scanner.hpp"

#include <iomanip>
#include <iostream>
#include <string_view>

class Compiler
{
public:
    void compile(std::string_view source)
    {
        std::size_t line = 0;
        for (;;)
        {
            const auto token = scanner.scan_token();
            const auto token_line = token.get_line();
            const auto token_type = token.get_type();

            if (token_line != line || line == 0)
            {
                std::cout << std::setw(4) << std::setfill('0') << token_line << ' ';
                line = token_line;
            }
            else
            {
                std::cout << "    | ";
            }
            // std::cout

            if (token_type == TokenType::Eof)
            {
                break;
            }
        }
    }

private:
    Scanner scanner;
};