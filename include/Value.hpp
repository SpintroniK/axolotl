#pragma once

#include <string>
#include <variant>
#include <vector>

using Number = double;
using Boolean = bool;
using String = std::string;

using Value = std::variant<Boolean, Number, String>;
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