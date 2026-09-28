#pragma once

#include <QtCore/private/qjnihelpers_p.h>

class LaunchAction;

class LaunchActionListener final : public QtAndroidPrivate::NewIntentListener
{
public:
    explicit LaunchActionListener(LaunchAction& owner);
    ~LaunchActionListener() override;

    bool handleNewIntent(JNIEnv* env, jobject intent) override;

private:
    LaunchAction& m_owner;
};
