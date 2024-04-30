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

#include "ble.h"

#include <QDBusServiceWatcher>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDebug>

#include <arpa/inet.h>

#define BLUEZ_SERVICE_NAME           "org.bluez"
#define GATT_MANAGER_IFACE           "org.bluez.GattManager1"
#define DEVICE_MANAGER_IFACE         "org.bluez.Device1"
#define DBUS_OM_IFACE                "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES_IFACE        "org.freedesktop.DBus.Properties"

#define GATT_SERVICE_IFACE           "org.bluez.GattService1"
#define GATT_DESC_IFACE              "org.bluez.GattDescriptor1"

BLE::BLE(QObject *parent) : QObject(parent), mBus(QDBusConnection::systemBus()) {
    mCurrentMtu = -1;

    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.registerObject(SERVICE_PATH, &mService, QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllProperties);
    bus.registerObject(TX_PATH, &mTX, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);
    bus.registerObject(RX_PATH, &mRX, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);

    mApplication = new Application(&mService, &mRX, &mTX);
    bus.registerObject(APPLICATION_PATH, mApplication, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);

    mConnected = false;

    mWatcher = new QDBusServiceWatcher(BLUEZ_SERVICE_NAME, mBus);
    connect(mWatcher, SIGNAL(serviceRegistered(const QString &)),
            this, SLOT(bluezServiceRegistered(const QString &)));
    connect(mWatcher, SIGNAL(serviceUnregistered(const QString &)),
            this, SLOT(bluezServiceUnregistered(const QString &)));

    connect(this, SIGNAL(adapterChanged()), this, SLOT(onAdapterChanged()));
    connect(this, SIGNAL(connectedChanged()), this, SLOT(onConnectedChanged()));

    connect(&mRX, SIGNAL(receivedFromCompanion(QByteArray)), this, SLOT(onReceivedFromCompanion(QByteArray)));

    connect(&mRX, SIGNAL(mtuChanged(int)), this, SLOT(onMtuChanged(int)));
    connect(&mTX, SIGNAL(mtuChanged(int)), this, SLOT(onMtuChanged(int)));

    QDBusInterface remoteOm(BLUEZ_SERVICE_NAME, "/", DBUS_OM_IFACE, mBus);
    if (remoteOm.isValid())
        bluezServiceRegistered(BLUEZ_SERVICE_NAME);
    else
        bluezServiceUnregistered(BLUEZ_SERVICE_NAME);
}

void BLE::bluezServiceRegistered(const QString &name) {
    qDebug() << "Service" << name << "is running";

    mBus.connect(BLUEZ_SERVICE_NAME, "/", DBUS_OM_IFACE, "InterfacesAdded",
            this, SLOT(bluezInterfacesChanged(QDBusObjectPath, InterfaceList)));
    mBus.connect(BLUEZ_SERVICE_NAME, "/", DBUS_OM_IFACE, "InterfacesRemoved",
            this, SLOT(bluezInterfacesChanged(QDBusObjectPath, QStringList)));

    updateAdapter();
}

void BLE::bluezServiceUnregistered(const QString &name) {
    qDebug() << "Service" << name << "is not running";

    setAdapter("");
    setConnected(false);
}

void BLE::bluezInterfacesChanged(QDBusObjectPath, InterfaceList) {
    updateAdapter();
}

void BLE::bluezPropertiesChanged(QString, QMap<QString, QVariant>, QStringList) {
    updateAdapter();
}

void BLE::updateAdapter() {
    QString adapter = "";
    bool connected = false;

    QDBusInterface remoteOm(BLUEZ_SERVICE_NAME, "/", DBUS_OM_IFACE, mBus);
    QDBusMessage result = remoteOm.call("GetManagedObjects");

    const QDBusArgument argument = result.arguments().at(0).value<QDBusArgument>();
    if (argument.currentType() == QDBusArgument::MapType) {
        argument.beginMap();
        while (!argument.atEnd()) {
            QString key;
            InterfaceList value;

            argument.beginMapEntry();
            argument >> key >> value;
            argument.endMapEntry();

            if (value.contains(GATT_MANAGER_IFACE))
                adapter = key;

            if (value.contains(DEVICE_MANAGER_IFACE)) {
                 mBus.connect(BLUEZ_SERVICE_NAME, key, DBUS_PROPERTIES_IFACE, "PropertiesChanged",
                         this, SLOT(bluezPropertiesChanged(QString, QMap<QString, QVariant>, QStringList)));
                 QMap<QString, QVariant> properties = value.value(DEVICE_MANAGER_IFACE);
                 if (properties.contains("Connected"))
                    connected |= properties.value("Connected").toBool();
            }
        }
        argument.endMap();
    }

    setAdapter(adapter);
    setConnected(connected);
}

void BLE::setAdapter(QString adapter) {
    if (adapter != mAdapter) {
        mAdapter = adapter;
        emit adapterChanged();
    }
}

void BLE::setConnected(bool connected) {
    if (connected != mConnected) {
        mConnected = connected;
        emit connectedChanged();
    }
}

void BLE::onAdapterChanged() {
    if (mAdapter != "") {
        qDebug() << "BLE Adapter" << mAdapter << "found";

        QDBusInterface serviceManager(BLUEZ_SERVICE_NAME, mAdapter, GATT_MANAGER_IFACE, mBus);
        serviceManager.asyncCall("RegisterApplication",
                QVariant::fromValue(QDBusObjectPath(APPLICATION_PATH)), QVariantMap());
        qDebug() << "Service" << APPLICATION_PATH << "registered";
    }
    else
        qDebug() << "No BLE adapter found";
}

void BLE::onConnectedChanged() {
    if (mConnected) {
        qDebug() << "Connected";
    } else {
        qDebug() << "Disconnected";
    }
}

void BLE::sendToCompanion(const QByteArray &content) {
    mTX.sendToCompanion(content);
}

void BLE::onReceivedFromCompanion(const QByteArray &content) {
    emit receivedFromCompanion(content);
}

void BLE::onMtuChanged(int mtu) {
    if (mCurrentMtu != mtu) {
        mCurrentMtu = mtu;
        emit mtuChanged(mtu);
    }
}
