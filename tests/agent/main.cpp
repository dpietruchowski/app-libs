#include <QCoreApplication>
#include <gtest/gtest.h>

// Networking (QTcpServer / QNetworkAccessManager) needs a running event loop,
// which requires a QCoreApplication. gtest_main does not create one, so this
// target provides its own entry point.
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
