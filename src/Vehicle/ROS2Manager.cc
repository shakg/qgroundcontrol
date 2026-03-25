#include "ROS2Manager.h"
#include "Vehicle.h"
#include "QGCLoggingCategory.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

QGC_LOGGING_CATEGORY(ROS2ManagerLog, "Vehicle.ROS2Manager")

ROS2Manager::ROS2Manager(Vehicle* vehicle)
    : QObject(vehicle)
    , _vehicle(vehicle)
{
    connect(&_webSocket, &QWebSocket::connected,            this, &ROS2Manager::_onConnected);
    connect(&_webSocket, &QWebSocket::disconnected,         this, &ROS2Manager::_onDisconnected);
    connect(&_webSocket, &QWebSocket::textMessageReceived,  this, &ROS2Manager::_onTextMessageReceived);
    connect(&_webSocket, &QWebSocket::errorOccurred,        this, &ROS2Manager::_onError);

    _reconnectTimer.setInterval(5000);
    _reconnectTimer.setSingleShot(true);
    connect(&_reconnectTimer, &QTimer::timeout, this, &ROS2Manager::_reconnect);
}

ROS2Manager::~ROS2Manager()
{
    _reconnectTimer.stop();
    _webSocket.close();
}

void ROS2Manager::setServerUrl(const QString& url)
{
    if (_serverUrl != url) {
        _serverUrl = url;
        emit serverUrlChanged();
    }
}

void ROS2Manager::connectToServer()
{
    if (_connected) {
        return;
    }
    if (_vehicle->isOfflineEditingVehicle()) {
        return;
    }
    qCDebug(ROS2ManagerLog) << "Connecting to rosbridge at" << _serverUrl;
    _webSocket.open(QUrl(_serverUrl));
}

void ROS2Manager::connectToServer(const QString& url)
{
    setServerUrl(url);
    connectToServer();
}

void ROS2Manager::disconnectFromServer()
{
    _reconnectTimer.stop();
    _webSocket.close();
}

void ROS2Manager::subscribe(const QString& topic, const QString& type, int throttleRateMs)
{
    _subscriptions[topic] = type;

    if (!_connected) {
        return;
    }

    QJsonObject json;
    json[QStringLiteral("op")]    = QStringLiteral("subscribe");
    json[QStringLiteral("topic")] = topic;
    json[QStringLiteral("type")]  = type;
    if (throttleRateMs > 0) {
        json[QStringLiteral("throttle_rate")] = throttleRateMs;
    }
    _sendJson(json);

    qCDebug(ROS2ManagerLog) << "Subscribed to" << topic << "(" << type << ")";
}

void ROS2Manager::unsubscribe(const QString& topic)
{
    _subscriptions.remove(topic);
    _latestMessages.remove(topic);

    if (!_connected) {
        return;
    }

    QJsonObject json;
    json[QStringLiteral("op")]    = QStringLiteral("unsubscribe");
    json[QStringLiteral("topic")] = topic;
    _sendJson(json);

    qCDebug(ROS2ManagerLog) << "Unsubscribed from" << topic;
}

void ROS2Manager::publish(const QString& topic, const QString& type, const QJsonObject& msg)
{
    if (!_connected) {
        qCWarning(ROS2ManagerLog) << "Cannot publish, not connected";
        return;
    }

    QJsonObject json;
    json[QStringLiteral("op")]    = QStringLiteral("publish");
    json[QStringLiteral("topic")] = topic;
    json[QStringLiteral("type")]  = type;
    json[QStringLiteral("msg")]   = msg;
    _sendJson(json);
}

void ROS2Manager::callService(const QString& service, const QString& type, const QJsonObject& args)
{
    if (!_connected) {
        qCWarning(ROS2ManagerLog) << "Cannot call service, not connected";
        return;
    }

    QJsonObject json;
    json[QStringLiteral("op")]      = QStringLiteral("call_service");
    json[QStringLiteral("service")] = service;
    json[QStringLiteral("type")]    = type;
    if (!args.isEmpty()) {
        json[QStringLiteral("args")] = args;
    }
    _sendJson(json);

    qCDebug(ROS2ManagerLog) << "Calling service" << service;
}

QJsonObject ROS2Manager::latestMessage(const QString& topic) const
{
    return _latestMessages.value(topic);
}

// ----------------------------------------------------------------------------
//  Private slots
// ----------------------------------------------------------------------------

void ROS2Manager::_onConnected()
{
    qCDebug(ROS2ManagerLog) << "Connected to rosbridge at" << _serverUrl;
    _connected = true;
    _reconnectTimer.stop();
    emit connectedChanged();

    _resubscribeAll();
}

void ROS2Manager::_onDisconnected()
{
    qCDebug(ROS2ManagerLog) << "Disconnected from rosbridge";
    _connected = false;
    emit connectedChanged();

    // Auto-reconnect unless this is an offline vehicle
    if (!_vehicle->isOfflineEditingVehicle()) {
        _reconnectTimer.start();
    }
}

void ROS2Manager::_onTextMessageReceived(const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qCWarning(ROS2ManagerLog) << "Received non-JSON message from rosbridge";
        return;
    }

    const QJsonObject json = doc.object();
    const QString op = json.value(QStringLiteral("op")).toString();

    if (op == QStringLiteral("publish")) {
        // Incoming topic message
        const QString topic = json.value(QStringLiteral("topic")).toString();
        const QJsonObject msg = json.value(QStringLiteral("msg")).toObject();
        _latestMessages[topic] = msg;
        emit topicMessageReceived(topic, msg);
    } else if (op == QStringLiteral("service_response")) {
        const QString service = json.value(QStringLiteral("service")).toString();
        const bool result = json.value(QStringLiteral("result")).toBool();
        const QJsonObject values = json.value(QStringLiteral("values")).toObject();
        emit serviceResponseReceived(service, result, values);
    } else {
        qCDebug(ROS2ManagerLog) << "Unhandled rosbridge op:" << op;
    }
}

void ROS2Manager::_onError(QAbstractSocket::SocketError error)
{
    qCWarning(ROS2ManagerLog) << "WebSocket error:" << error << _webSocket.errorString();
}

void ROS2Manager::_reconnect()
{
    if (!_connected && !_vehicle->isOfflineEditingVehicle()) {
        qCDebug(ROS2ManagerLog) << "Attempting reconnect to" << _serverUrl;
        _webSocket.open(QUrl(_serverUrl));
    }
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

void ROS2Manager::_sendJson(const QJsonObject& json)
{
    const QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    _webSocket.sendTextMessage(QString::fromUtf8(data));
}

void ROS2Manager::_resubscribeAll()
{
    for (auto it = _subscriptions.constBegin(); it != _subscriptions.constEnd(); ++it) {
        QJsonObject json;
        json[QStringLiteral("op")]    = QStringLiteral("subscribe");
        json[QStringLiteral("topic")] = it.key();
        json[QStringLiteral("type")]  = it.value();
        _sendJson(json);
        qCDebug(ROS2ManagerLog) << "Re-subscribed to" << it.key();
    }
}
