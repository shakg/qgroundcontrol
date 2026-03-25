#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtWebSockets/QWebSocket>

Q_DECLARE_LOGGING_CATEGORY(ROS2ManagerLog)

class Vehicle;

/// Manages a rosbridge WebSocket connection to a mission computer,
/// allowing QML to subscribe to ROS2 topics and call ROS2 services
/// via the standard rosbridge v2.0 protocol.
///
/// Exposed on Vehicle as ros2Manager so QML can use:
///   activeVehicle.ros2Manager.connected
///   activeVehicle.ros2Manager.subscribe("/gimbal/status", ...)
///   activeVehicle.ros2Manager.latestMessage("/gimbal/status")
class ROS2Manager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    explicit ROS2Manager(Vehicle* vehicle);
    ~ROS2Manager() override;

    Q_PROPERTY(bool     connected   READ connected  NOTIFY connectedChanged)
    Q_PROPERTY(QString  serverUrl   READ serverUrl  WRITE setServerUrl NOTIFY serverUrlChanged)

    bool    connected   () const { return _connected; }
    QString serverUrl   () const { return _serverUrl; }
    void    setServerUrl(const QString& url);

    /// Connect to the rosbridge server. Uses serverUrl property.
    Q_INVOKABLE void connectToServer();

    /// Connect to a specific rosbridge server URL (e.g. "ws://192.168.1.10:9090").
    Q_INVOKABLE void connectToServer(const QString& url);

    /// Disconnect from the rosbridge server.
    Q_INVOKABLE void disconnectFromServer();

    /// Subscribe to a ROS2 topic. Messages arrive via the topicMessageReceived signal.
    /// @param topic    The ROS2 topic name, e.g. "/gimbal/status"
    /// @param type     The ROS2 message type, e.g. "sensor_msgs/msg/JointState"
    /// @param throttleRateMs  Optional: throttle rate in ms (0 = no throttle)
    Q_INVOKABLE void subscribe(const QString& topic, const QString& type, int throttleRateMs = 0);

    /// Unsubscribe from a ROS2 topic.
    Q_INVOKABLE void unsubscribe(const QString& topic);

    /// Publish a message to a ROS2 topic via rosbridge.
    /// @param topic  The ROS2 topic name
    /// @param type   The ROS2 message type
    /// @param msg    The message payload as a JSON object
    Q_INVOKABLE void publish(const QString& topic, const QString& type, const QJsonObject& msg);

    /// Call a ROS2 service via rosbridge.
    /// @param service  The service name, e.g. "/gimbal/set_angle"
    /// @param type     The service type
    /// @param args     The service request arguments as a JSON object
    /// Response arrives via serviceResponseReceived signal.
    Q_INVOKABLE void callService(const QString& service, const QString& type, const QJsonObject& args = QJsonObject());

    /// Returns the latest message received on the given topic, or an empty QJsonObject.
    Q_INVOKABLE QJsonObject latestMessage(const QString& topic) const;

signals:
    void connectedChanged();
    void serverUrlChanged();

    /// Emitted when a subscribed topic receives a message.
    /// @param topic  The ROS2 topic name
    /// @param msg    The message payload as a JSON object
    void topicMessageReceived(const QString& topic, const QJsonObject& msg);

    /// Emitted when a service call response is received.
    /// @param service  The service name
    /// @param result   True if the service call succeeded
    /// @param values   The response payload
    void serviceResponseReceived(const QString& service, bool result, const QJsonObject& values);

private slots:
    void _onConnected();
    void _onDisconnected();
    void _onTextMessageReceived(const QString& message);
    void _onError(QAbstractSocket::SocketError error);
    void _reconnect();

private:
    void _sendJson(const QJsonObject& json);
    void _resubscribeAll();

    Vehicle*    _vehicle    = nullptr;
    QWebSocket  _webSocket;
    bool        _connected  = false;
    QString     _serverUrl  = QStringLiteral("ws://localhost:9090");
    QTimer      _reconnectTimer;

    /// Tracks active subscriptions: topic -> type
    QHash<QString, QString>     _subscriptions;
    /// Caches the latest message per topic
    QHash<QString, QJsonObject> _latestMessages;
};
