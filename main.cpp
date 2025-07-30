#include "include/Chunk.hpp"
#include "include/Debug.hpp"
#include "include/Vm.hpp"

#include <string_view>
#include <vector>

#include <cstdlib>

int main(int argc, char** argv)
{
    const auto args = std::vector<std::string_view>{ argv, argv + argc };

    Chunk chunk;

    // chunk.write(OpCode::Return, 123);

    const auto const1 = chunk.add_constant(1.2);
    chunk.write(OpCode::Constant, 123);

    const auto const2 = chunk.add_constant(2.2);
    chunk.write(OpCode::Constant, 123);

    chunk.write(const1, 123);
    chunk.write(const2, 123);

    chunk.write(OpCode::Add, 123);
    chunk.write(OpCode::Return, 123);

    debug::Debug::dissassemble_chunk(chunk, "chunk");

    Vm vm;
    const auto result = vm.interpret(chunk);

    if (result != InterpretResult::Ok)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}