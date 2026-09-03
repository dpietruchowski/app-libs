#pragma once

#include <QString>
#include <optional>

template <typename T> class Result final
{
public:
    using value_type = T;

    static Result success(const T& value) { return Result { value, std::nullopt }; }

    static Result failure(const QString& error) { return Result { std::nullopt, error }; }

    bool isSuccess() const { return m_value.has_value(); }
    bool isFailure() const { return !isSuccess(); }

    const T& value() const { return m_value.value(); }
    const QString& error() const { return m_error.value(); }

private:
    Result(std::optional<T> value, std::optional<QString> error)
        : m_value(std::move(value))
        , m_error(std::move(error))
    {
    }

    std::optional<T> m_value;
    std::optional<QString> m_error;
};

template <> class Result<void> final
{
public:
    using value_type = void;

    static Result success() { return Result { std::nullopt }; }

    static Result failure(const QString& error) { return Result { error }; }

    bool isSuccess() const { return !m_error.has_value(); }
    bool isFailure() const { return !isSuccess(); }

    const QString& error() const { return m_error.value(); }

private:
    explicit Result(std::optional<QString> error)
        : m_error(std::move(error))
    {
    }

    std::optional<QString> m_error;
};
