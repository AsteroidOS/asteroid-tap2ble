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

#ifndef BLE_H
#define BLE_H

#include <QObject>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QDBusConnection>

#include "ble-dbus.h"

typedef QMap<QString, QMap<QString, QVariant>> InterfaceList;

class BLE : public QObject
{
    Q_OBJECT
public:
    explicit BLE(QObject *parent = 0);
    void updateConnected();

    void sendToCompanion(const QByteArray &data);

private:
    Application *mApplication;
    Service mService;
    RXChrc mRX;
    TXChrc mTX;

    QDBusServiceWatcher *mWatcher;
    QDBusConnection mBus;
    QString mAdapter;
    bool mConnected;

    void updateAdapter();
    void setAdapter(QString adatper);
    void setConnected(bool connected);

    QByteArray mAccumulatedRecv;

signals:
    void connectedChanged();
    void adapterChanged();

    void receivedFromCompanion(const QByteArray &data);

public slots:
    void bluezServiceRegistered(const QString &name);
    void bluezServiceUnregistered(const QString &name);
    void bluezInterfacesChanged(QDBusObjectPath, InterfaceList);
    void bluezPropertiesChanged(QString, QMap<QString, QVariant>, QStringList);

    void onConnectedChanged();
    void onAdapterChanged();

private slots:
    void onReceivedFromCompanion(const QByteArray &data);
};

#endif // BLE_H
