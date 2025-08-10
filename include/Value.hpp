#pragma once


#include <memory>
#include <string>
#include <variant>
#include <vector>

using Number = double;
using Boolean = bool;
using String = std::string;

class Function
{
public:
    Function(const Function& other);
    Function& operator=(const Function& other);

    Function(Function&& other) noexcept;
    Function& operator=(Function&& other) noexcept;

    bool operator==(const Function& other) const;


    ~Function();

    [[nodiscard]] auto get_name() const noexcept -> std::string
    {
        return name;
    }


private:
    std::size_t arity = 0;
    std::unique_ptr<class Chunk> chunk_ptr;
    std::string name;
};

using Value = std::variant<Boolean, Number, String, Function>;
using ValueArray = std::vector<Value>;

namespace values
{
    template <typename T, typename... Ts>
    struct IsTypeInVariant;

    template <typename T, typename... Ts>
    struct IsTypeInVariant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...>
    {
    };

    template <typename T>
    concept IsInVariant = IsTypeInVariant<T, Value>::value;

    template <typename T>
        requires IsInVariant<T>
    static constexpr auto as(const Value& value)
    {
        return std::get<T>(value);
    }

    template <typename T>
        requires IsInVariant<T>
    static constexpr auto make(const T& val) -> Value
    {
        Value value = val;
        return value;
    }

    template <typename T>
        requires IsInVariant<T>
    static constexpr bool is(const Value& value)
    {
        return std::get_if<T>(&value) != nullptr;
    }

} // namespace values