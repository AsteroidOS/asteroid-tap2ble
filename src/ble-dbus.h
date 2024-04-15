/*
 * Copyright (C) 2024 - AsteroidOS
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BLE_DBUS_H
#define BLE_DBUS_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDebug>

#define GATT_SERVICE_IFACE "org.bluez.GattService1"
#define GATT_CHRC_IFACE    "org.bluez.GattCharacteristic1"

#define APPLICATION_PATH "/org/asteroidos/tap2ble"
#define SERVICE_PATH     "/org/asteroidos/tap2ble/service"
#define TX_PATH          "/org/asteroidos/tap2ble/service/tx"
#define RX_PATH          "/org/asteroidos/tap2ble/service/rx"

#define SERVICE_UUID "00001071-0000-0000-0000-00A57E401D05"
#define RX_UUID      "00001001-0000-0000-0000-00A57E401D05"
#define TX_UUID      "00001002-0000-0000-0000-00A57E401D05"

// Writable characteristic for companion to watch communication
class RXChrc : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", GATT_CHRC_IFACE)
    Q_PROPERTY(QDBusObjectPath Service READ getService())
    Q_PROPERTY(QString UUID READ getUuid())
    Q_PROPERTY(QStringList Flags READ getFlags())
    Q_PROPERTY(QList<QDBusObjectPath> Descriptors READ getDescriptors())

public:
    QDBusObjectPath getService() { return QDBusObjectPath(SERVICE_PATH); }
    QString getUuid() { return RX_UUID; }
    QStringList getFlags() { return {"encrypt-authenticated-write"}; }
    QList<QDBusObjectPath> getDescriptors() { return {}; }

public slots:
    QByteArray ReadValue(QVariantMap options) { return QByteArray(); }
    void StartNotify() {}
    void StopNotify() {}
    void WriteValue(QByteArray data, QVariantMap);

signals:
    void receivedFromCompanion(const QByteArray &);
};

// Notifiable characteristic for watch to companion communication
class TXChrc : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", GATT_CHRC_IFACE)
    Q_PROPERTY(QDBusObjectPath Service READ getService())
    Q_PROPERTY(QString UUID READ getUuid())
    Q_PROPERTY(QStringList Flags READ getFlags())
    Q_PROPERTY(QList<QDBusObjectPath> Descriptors READ getDescriptors())
    Q_PROPERTY(QByteArray Value READ getValue NOTIFY valueChanged)

public:
    QDBusObjectPath getService() { return QDBusObjectPath(SERVICE_PATH); }
    QString getUuid() { return TX_UUID; }
    QStringList getFlags() { return {"encrypt-authenticated-read", "notify"}; }
    QList<QDBusObjectPath> getDescriptors() { return {}; }
    QByteArray getValue() { return m_value; }

public slots:
    void WriteValue(QByteArray value, QVariantMap options) {}
    void StartNotify() {}
    void StopNotify() {}
    QByteArray ReadValue(QVariantMap) { return m_value; }

    void sendToCompanion(QByteArray content);

signals:
    void valueChanged();

private:
    void emitPropertiesChanged() {
        QDBusConnection connection = QDBusConnection::systemBus();
        QDBusMessage message = QDBusMessage::createSignal(TX_PATH,
                                                          "org.freedesktop.DBus.Properties",
                                                          "PropertiesChanged");

        QVariantMap changedProperties;
        changedProperties[QStringLiteral("Value")] = QVariant(m_value);

        QList<QVariant> arguments;
        arguments << QVariant(GATT_CHRC_IFACE) << QVariant(changedProperties) << QVariant(QStringList());
        message.setArguments(arguments);

        if (!connection.send(message))
            qDebug() << "Failed to send DBus property notification signal";
    }

    QByteArray m_value;
};

// Service exposing a RX and a TX characteristics
class Service : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", GATT_SERVICE_IFACE)
    Q_PROPERTY(QString UUID READ getUuid())
    Q_PROPERTY(bool Primary READ getPrimary())
    Q_PROPERTY(QList<QDBusObjectPath> Characteristics READ getCharacteristics())

public:
    QString getUuid() { return SERVICE_UUID; }
    bool getPrimary() { return true; }
    QList<QDBusObjectPath> getCharacteristics();
};

typedef QMap<QString, QVariantMap> InterfaceList;
typedef QMap<QDBusObjectPath, InterfaceList> ManagedObjectList;
Q_DECLARE_METATYPE(InterfaceList)
Q_DECLARE_METATYPE(ManagedObjectList)

// DBus endpoint representing tap2ble to Bluez
class Application : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")

public:
    Application(Service *service, RXChrc *rx, TXChrc *tx, QObject *parent = 0)
        : QObject(parent), mService(service), mRX(rx), mTX(tx) { }

private:
    Service *mService;
    RXChrc *mRX;
    TXChrc *mTX;

public slots:
    ManagedObjectList GetManagedObjects();
};

#endif // BLE_DBUS_H
