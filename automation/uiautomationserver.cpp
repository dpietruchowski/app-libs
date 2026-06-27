#include "uiautomationserver.h"

#include "utils/mocktimeprovider.h"
#include "utils/timeprovider.h"

#include <QDate>
#include <QDateTime>
#include <QGuiApplication>
#include <QHostAddress>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVariant>

#include <algorithm>

namespace
{
QList<QObject*> childObjects(QObject* obj)
{
    QList<QObject*> result = obj->children();
    if (auto* item = qobject_cast<QQuickItem*>(obj))
    {
        for (QQuickItem* child : item->childItems())
        {
            if (!result.contains(child))
                result.append(child);
        }
    }
    return result;
}

bool isClippedAway(QQuickItem* item)
{
    for (QQuickItem* p = item->parentItem(); p; p = p->parentItem())
    {
        if (p->clip() && (p->width() <= 0 || p->height() <= 0))
            return true;
    }
    return false;
}

bool isEffectivelyVisible(QQuickItem* item)
{
    return item->isVisible() && !isClippedAway(item);
}

QObject* findNamed(QObject* root, const QString& name, bool requireVisible)
{
    if (root->objectName() == name)
    {
        auto* item = qobject_cast<QQuickItem*>(root);
        if (!requireVisible || !item || isEffectivelyVisible(item))
            return root;
    }
    for (QObject* child : childObjects(root))
    {
        if (QObject* found = findNamed(child, name, requireVisible))
            return found;
    }
    return nullptr;
}

QJsonObject describe(QObject* obj)
{
    QJsonObject node;
    node["objectName"] = obj->objectName();
    node["class"] = QString::fromUtf8(obj->metaObject()->className());

    if (auto* item = qobject_cast<QQuickItem*>(obj))
    {
        const QPointF scene = item->mapToScene(QPointF(0, 0));
        node["x"] = scene.x();
        node["y"] = scene.y();
        node["width"] = item->width();
        node["height"] = item->height();
        node["visible"] = isEffectivelyVisible(item);
        node["enabled"] = item->isEnabled();
        if (QQuickWindow* window = item->window())
        {
            const QPoint global = window->mapToGlobal(scene.toPoint());
            node["globalX"] = global.x();
            node["globalY"] = global.y();
        }
    }

    QVariant text = obj->property("content");
    if (text.metaType().id() != QMetaType::QString)
        text = obj->property("text");
    if (text.isValid() && text.metaType().id() == QMetaType::QString && !text.toString().isEmpty())
        node["text"] = text.toString();

    if (obj->property("checkable").toBool())
        node["checked"] = obj->property("checked").toBool();

    const QVariant currentText = obj->property("currentText");
    if (currentText.isValid() && currentText.metaType().id() == QMetaType::QString)
    {
        node["currentText"] = currentText.toString();

        const QVariant currentIndex = obj->property("currentIndex");
        if (currentIndex.isValid() && currentIndex.metaType().id() == QMetaType::Int)
            node["currentIndex"] = currentIndex.toInt();

        const QVariant model = obj->property("model");
        if (model.canConvert<QVariantList>())
        {
            QJsonArray values;
            for (const QVariant& entry : model.toList())
                values.append(entry.toString());
            if (!values.isEmpty())
                node["values"] = values;
        }
    }

    const QVariant value = obj->property("value");
    if (value.isValid()
        && (value.metaType().id() == QMetaType::Int || value.metaType().id() == QMetaType::Double))
        node["value"] = QJsonValue::fromVariant(value);

    return node;
}

QObject* findFlickable(QObject* obj)
{
    if (obj->property("contentY").isValid())
        return obj;
    for (QObject* child : childObjects(obj))
    {
        if (QObject* found = findFlickable(child))
            return found;
    }
    return nullptr;
}

void collectNamed(QObject* obj, QJsonArray& out, QSet<QObject*>& visited)
{
    for (QObject* child : childObjects(obj))
    {
        if (visited.contains(child))
            continue;
        visited.insert(child);

        if (auto* item = qobject_cast<QQuickItem*>(child); item && !isEffectivelyVisible(item))
            continue;

        if (!child->objectName().isEmpty())
        {
            QJsonObject node = describe(child);
            QJsonArray children;
            collectNamed(child, children, visited);
            if (!children.isEmpty())
                node["children"] = children;
            out.append(node);
        }
        else
        {
            collectNamed(child, out, visited);
        }
    }
}

QJsonObject makeError(const QString& message)
{
    QJsonObject response;
    response["ok"] = false;
    response["error"] = message;
    return response;
}

QJsonObject makeOk()
{
    QJsonObject response;
    response["ok"] = true;
    return response;
}

MockTimeProvider* ensureMockTime()
{
    auto* mock = dynamic_cast<MockTimeProvider*>(&TimeProvider::instance());
    if (!mock)
    {
        auto provider = std::make_unique<MockTimeProvider>();
        provider->setCurrentDateTime(QDateTime::currentDateTime());
        mock = provider.get();
        TimeProvider::setInstance(std::move(provider));
    }
    return mock;
}

QJsonObject timeResponse()
{
    QJsonObject response = makeOk();
    response["dateTime"] = TimeProvider::instance().currentDateTime().toString(Qt::ISODate);
    response["date"] = TimeProvider::instance().currentDate().toString(Qt::ISODate);
    response["mocked"] = dynamic_cast<MockTimeProvider*>(&TimeProvider::instance()) != nullptr;
    return response;
}
}

UiAutomationServer::UiAutomationServer(QQmlApplicationEngine* engine, quint16 port, QObject* parent)
    : QObject(parent), m_engine(engine), m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &UiAutomationServer::onNewConnection);
    if (m_server->listen(QHostAddress::AnyIPv4, port))
        qInfo() << "UiAutomationServer listening on 0.0.0.0:" << m_server->serverPort();
    else
        qWarning() << "UiAutomationServer failed to listen on port" << port << ":"
                   << m_server->errorString();
}

void UiAutomationServer::onNewConnection()
{
    while (QTcpSocket* socket = m_server->nextPendingConnection())
    {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &UiAutomationServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this,
            [this, socket]()
            {
                m_buffers.remove(socket);
                socket->deleteLater();
            });
    }
}

void UiAutomationServer::onReadyRead()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_buffers[socket];
    buffer.append(socket->readAll());

    int newline;
    while ((newline = buffer.indexOf('\n')) != -1)
    {
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (line.trimmed().isEmpty())
            continue;

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        QJsonObject response;
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            response = makeError("invalid json: " + parseError.errorString());
        else
            response = handleCommand(doc.object());

        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact));
        socket->write("\n");
        socket->flush();
    }
}

QObject* UiAutomationServer::findByObjectName(const QString& name) const
{
    for (QObject* root : m_engine->rootObjects())
    {
        if (QObject* found = findNamed(root, name, true))
            return found;
    }
    return nullptr;
}

QJsonObject UiAutomationServer::handleCommand(const QJsonObject& request)
{
    const QString cmd = request["cmd"].toString();

    if (cmd == "ping")
    {
        QJsonObject response = makeOk();
        response["pong"] = true;
        return response;
    }

    if (cmd == "dump")
    {
        QJsonArray tree;
        QSet<QObject*> visited;
        for (QObject* root : m_engine->rootObjects())
        {
            if (visited.contains(root))
                continue;
            visited.insert(root);
            if (!root->objectName().isEmpty())
            {
                QJsonObject node = describe(root);
                QJsonArray children;
                collectNamed(root, children, visited);
                if (!children.isEmpty())
                    node["children"] = children;
                tree.append(node);
            }
            else
            {
                collectNamed(root, tree, visited);
            }
        }
        QJsonObject response = makeOk();
        response["tree"] = tree;
        return response;
    }

    if (cmd == "get_time")
        return timeResponse();

    if (cmd == "set_time")
    {
        MockTimeProvider* mock = ensureMockTime();
        const QString dateTimeStr = request["dateTime"].toString();
        const QString dateStr = request["date"].toString();
        if (!dateTimeStr.isEmpty())
        {
            const QDateTime dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODate);
            if (!dateTime.isValid())
                return makeError("invalid dateTime (expected ISO 8601): " + dateTimeStr);
            mock->setCurrentDateTime(dateTime);
        }
        else if (!dateStr.isEmpty())
        {
            const QDate date = QDate::fromString(dateStr, Qt::ISODate);
            if (!date.isValid())
                return makeError("invalid date (expected yyyy-MM-dd): " + dateStr);
            mock->setCurrentDate(date);
        }
        else
        {
            return makeError("set_time requires 'dateTime' or 'date'");
        }
        return timeResponse();
    }

    if (cmd == "advance_time")
    {
        if (!request.contains("days"))
            return makeError("advance_time requires 'days'");
        ensureMockTime()->advanceDays(request["days"].toInt());
        return timeResponse();
    }

    if (cmd == "reset_time")
    {
        TimeProvider::setInstance(std::make_unique<SystemTimeProvider>());
        return timeResponse();
    }

    const QString name = request["name"].toString();

    if (cmd == "click")
    {
        QObject* target = findByObjectName(name);
        if (!target)
            return makeError("object not found: " + name);
        auto* item = qobject_cast<QQuickItem*>(target);
        if (!item)
            return makeError("object is not a visual item: " + name);
        QQuickWindow* window = item->window();
        if (!window)
            return makeError("object has no window: " + name);

        const QPointF center = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
        const QPointF global = QPointF(window->mapToGlobal(center.toPoint()));

        QMouseEvent press(QEvent::MouseButtonPress, center, center, global, Qt::LeftButton,
            Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, center, center, global, Qt::LeftButton,
            Qt::NoButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &release);
        return makeOk();
    }

    if (cmd == "scroll")
    {
        QObject* target = findByObjectName(name);
        if (!target)
            return makeError("object not found: " + name);
        QObject* flickable = findFlickable(target);
        if (!flickable)
            return makeError("no flickable found for: " + name);

        const auto applyAxis = [&](const char* posKey, const char* contentKey, const char* sizeKey,
                                   double delta)
        {
            const double pos = flickable->property(posKey).toDouble();
            const double content = flickable->property(contentKey).toDouble();
            const double size = flickable->property(sizeKey).toDouble();
            const double maxPos = std::max(0.0, content - size);
            flickable->setProperty(posKey, std::clamp(pos + delta, 0.0, maxPos));
        };
        applyAxis("contentY", "contentHeight", "height", request["dy"].toDouble());
        applyAxis("contentX", "contentWidth", "width", request["dx"].toDouble());

        QJsonObject response = makeOk();
        response["contentY"] = flickable->property("contentY").toDouble();
        response["contentX"] = flickable->property("contentX").toDouble();
        response["atBeginning"] = flickable->property("atYBeginning").toBool();
        response["atEnd"] = flickable->property("atYEnd").toBool();
        return response;
    }

    if (cmd == "get")
    {
        QObject* target = findByObjectName(name);
        if (!target)
            return makeError("object not found: " + name);
        const QString property = request["property"].toString();
        QJsonObject response = makeOk();
        response["value"] = QJsonValue::fromVariant(target->property(property.toUtf8().constData()));
        return response;
    }

    if (cmd == "set")
    {
        QObject* target = findByObjectName(name);
        if (!target)
            return makeError("object not found: " + name);
        const QString property = request["property"].toString();
        const bool done =
            target->setProperty(property.toUtf8().constData(), request["value"].toVariant());
        if (!done)
            return makeError("could not set property: " + property);
        return makeOk();
    }

    if (cmd == "invoke")
    {
        QObject* target = findByObjectName(name);
        if (!target)
            return makeError("object not found: " + name);
        const QString method = request["method"].toString();
        const QJsonArray argsArray = request["args"].toArray();
        if (argsArray.size() > 10)
            return makeError("too many arguments (max 10)");

        const QMetaObject* meta = target->metaObject();
        QMetaMethod chosen;
        for (int i = 0; i < meta->methodCount(); ++i)
        {
            const QMetaMethod candidate = meta->method(i);
            if (candidate.methodType() == QMetaMethod::Signal)
                continue;
            if (candidate.name() != method.toUtf8())
                continue;
            if (candidate.parameterCount() != argsArray.size())
                continue;
            chosen = candidate;
            break;
        }
        if (!chosen.isValid())
            return makeError("no matching method: " + method);

        QVariantList converted;
        for (int i = 0; i < argsArray.size(); ++i)
        {
            QVariant value = argsArray[i].toVariant();
            const QMetaType paramType(chosen.parameterType(i));
            if (!value.convert(paramType))
                return makeError("cannot convert argument to " + QString::fromUtf8(paramType.name()));
            converted.append(value);
        }

        QGenericArgument args[10];
        for (int i = 0; i < converted.size(); ++i)
            args[i] = QGenericArgument(
                QMetaType(chosen.parameterType(i)).name(), converted.at(i).constData());

        const bool ok = chosen.invoke(target, Qt::DirectConnection, args[0], args[1], args[2],
            args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
        if (!ok)
            return makeError("invoke failed: " + method);
        return makeOk();
    }

    if (cmd == "tap")
    {
        if (m_engine->rootObjects().isEmpty())
            return makeError("no root object");
        auto* window = qobject_cast<QQuickWindow*>(m_engine->rootObjects().first());
        if (!window)
            return makeError("root is not a window");
        const QPointF scene(request["x"].toDouble(), request["y"].toDouble());
        const QPointF global = QPointF(window->mapToGlobal(scene.toPoint()));
        QMouseEvent press(QEvent::MouseButtonPress, scene, scene, global, Qt::LeftButton,
            Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, scene, scene, global, Qt::LeftButton,
            Qt::NoButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &release);
        return makeOk();
    }

    if (cmd == "screenshot")
    {
        if (m_engine->rootObjects().isEmpty())
            return makeError("no root object");
        auto* window = qobject_cast<QQuickWindow*>(m_engine->rootObjects().first());
        if (!window)
            return makeError("root is not a window");
        const QImage image = window->grabWindow();
        const QString path = request["path"].toString();
        if (!image.save(path))
            return makeError("could not save screenshot to: " + path);
        return makeOk();
    }

    return makeError("unknown command: " + cmd);
}
