#include "chat/application/unavailablebackend.h"

#include <QMetaObject>

namespace chat
{

namespace
{

    class UnavailableTurn final : public AgentTurn
    {
    public:
        UnavailableTurn()
        {
            QMetaObject::invokeMethod(
                this, [this] { emit failed(turn_error::kUnavailable); }, Qt::QueuedConnection);
        }

        void cancel() override { }
    };

}

bool UnavailableBackend::available() const { return false; }

std::unique_ptr<AgentTurn> UnavailableBackend::start(const TurnRequest&, const AgentCatalog&)
{
    return std::make_unique<UnavailableTurn>();
}

}
