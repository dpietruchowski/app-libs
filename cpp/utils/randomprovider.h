#pragma once

#include <memory>
#include <random>
#include <vector>

class RandomProvider
{
public:
    static RandomProvider& instance();
    static void setInstance(std::unique_ptr<RandomProvider> provider);

    virtual ~RandomProvider() = default;

    virtual double unit() = 0;
    virtual size_t index(size_t count) = 0;
    virtual void shuffle(std::vector<int>& values) = 0;

protected:
    RandomProvider() = default;

private:
    static std::unique_ptr<RandomProvider> s_instance;
};

class EngineRandomProvider : public RandomProvider
{
public:
    double unit() override;
    size_t index(size_t count) override;
    void shuffle(std::vector<int>& values) override;

protected:
    explicit EngineRandomProvider(std::mt19937::result_type seed);

private:
    std::mt19937 m_engine;
};

class SystemRandomProvider final : public EngineRandomProvider
{
public:
    SystemRandomProvider();
};

class SeededRandomProvider final : public EngineRandomProvider
{
public:
    explicit SeededRandomProvider(std::mt19937::result_type seed);
};
