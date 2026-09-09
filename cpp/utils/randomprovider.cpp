#include "randomprovider.h"

#include <algorithm>

std::unique_ptr<RandomProvider> RandomProvider::s_instance = nullptr;

RandomProvider& RandomProvider::instance()
{
    if (!s_instance)
    {
        s_instance = std::make_unique<SystemRandomProvider>();
    }
    return *s_instance;
}

void RandomProvider::setInstance(std::unique_ptr<RandomProvider> provider)
{
    s_instance = std::move(provider);
}

EngineRandomProvider::EngineRandomProvider(std::mt19937::result_type seed)
    : m_engine(seed)
{
}

double EngineRandomProvider::unit()
{
    return std::uniform_real_distribution<double>(0.0, 1.0)(m_engine);
}

size_t EngineRandomProvider::index(size_t count)
{
    if (count == 0)
    {
        return 0;
    }
    return std::uniform_int_distribution<size_t>(0, count - 1)(m_engine);
}

void EngineRandomProvider::shuffle(std::vector<int>& values)
{
    std::shuffle(values.begin(), values.end(), m_engine);
}

SystemRandomProvider::SystemRandomProvider()
    : EngineRandomProvider(std::random_device {}())
{
}

SeededRandomProvider::SeededRandomProvider(std::mt19937::result_type seed)
    : EngineRandomProvider(seed)
{
}
