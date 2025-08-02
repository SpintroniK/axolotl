#include "include/Chunk.hpp"
#include "include/Compiler.hpp"
#include "include/Debug.hpp"
#include "include/Vm.hpp"

#include <string_view>
#include <vector>

#include <cstdlib>


// namespace
// {
//     void repl(Vm& vm)
//     {
//         std::string line;

//         for (;;)
//         {
//             std::cout << "> ";
//             std::getline(std::cin, line);

//             if (line.empty())
//             {
//                 std::cout << '\n';
//             }

//             vm.interpret(line);
//         }
//     }
// } // namespace


int main(int argc, char** argv)
{
    const auto args = std::vector<std::string_view>{ argv, argv + argc };

    const auto chunk = Compiler{ R"===(var abc = 1;)===" }.compile().value();

    // debug::Debug::dissassemble_chunk(chunk, "chunk");

    Vm vm;
    const auto result = vm.interpret(chunk);

    if (result != InterpretResult::Ok)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}