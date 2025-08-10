#include "Value.hpp"
#include "Chunk.hpp"

Function::~Function() = default;


Function::Function(const Function& other)
: arity(other.arity), chunk_ptr{ other.chunk_ptr ? std::make_unique<Chunk>(*other.chunk_ptr) : nullptr }, name{ other.name }
{
}

Function& Function::operator=(const Function& other)
{
    if (this != &other)
    {
        arity = other.arity;
        name = other.name;
        chunk_ptr = other.chunk_ptr ? std::make_unique<Chunk>(*other.chunk_ptr) : nullptr;
    }
    return *this;
}

Function::Function(Function&& other) noexcept
: chunk_ptr{ std::move(other.chunk_ptr) }, name{ std::move(other.name) }, arity{ other.arity }
{
    other.arity = 0;
}

Function& Function::operator=(Function&& other) noexcept
{
    if (this != &other)
    {
        chunk_ptr = std::move(other.chunk_ptr);
        name = std::move(other.name);
        arity = other.arity;

        other.arity = 0;
    }
    return *this;
}

bool Function::operator==(const Function& other) const
{
    return name == other.name;
}