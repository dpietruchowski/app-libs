#pragma once

#include <QObject>
#include <QString>

#include <memory>

class LaunchActionListener;

// The action the app was opened for, e.g. from a notification whose intent
// carried a "launch_action" extra. An action waits in take() until someone
// handles it; received() announces each one, also while the app is running.
// Platforms without a backend never receive anything.
class LaunchAction final : public QObject
{
    Q_OBJECT

public:
    explicit LaunchAction(QObject* parent = nullptr);
    ~LaunchAction() override;

    Q_INVOKABLE QString take();
    void deliver(const QString& action);

signals:
    void received(const QString& action);

private:
    QString m_pending;
    std::unique_ptr<LaunchActionListener> m_listener;
};
