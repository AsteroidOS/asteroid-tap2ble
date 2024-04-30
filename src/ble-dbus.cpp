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

#include "ble-dbus.h"

// Called when a companion writes to the RX characteristic
void RXChrc::WriteValue(QByteArray data, QVariantMap properties) {
    if (properties.contains("mtu")) {
        emit mtuChanged(properties["mtu"].toUInt());
    } else
        qWarning() << "mtu not in WriteValue";
    emit receivedFromCompanion(data);
}

QByteArray TXChrc::ReadValue(QVariantMap properties) {
    if (properties.contains("mtu")) {
        emit mtuChanged(properties["mtu"].toUInt());
    } else
        qWarning() << "mtu not in ReadValue";
    return m_value;
}

// Forwards information to the companion by notifications on the TX characteristic
void TXChrc::sendToCompanion(QByteArray content) {
    m_value = content;
    emit valueChanged();
    emitPropertiesChanged();
}

// The Service only has two characteristics: RX and TX
QList<QDBusObjectPath> Service::getCharacteristics() {
    return { QDBusObjectPath(RX_PATH), QDBusObjectPath(TX_PATH) };
}

// Lists all the services (one) and characteristics (two) of asteroid-tap2ble
ManagedObjectList Application::GetManagedObjects() {
    InterfaceList serviceInterfaces, rxInterfaces, txInterfaces;
    QVariantMap serviceProperties, rxProperties, txProperties;
    ManagedObjectList response;

    serviceProperties.insert("UUID", mService->getUuid());
    serviceProperties.insert("Primary", mService->getPrimary());
    serviceProperties.insert("Characteristics", QVariant::fromValue(mService->getCharacteristics()));
    serviceInterfaces.insert(GATT_SERVICE_IFACE, serviceProperties);
    response.insert(QDBusObjectPath(SERVICE_PATH), serviceInterfaces);

    rxProperties.insert("Service", QVariant::fromValue(mRX->getService()));
    rxProperties.insert("UUID", mRX->getUuid());
    rxProperties.insert("Flags", mRX->getFlags());
    rxProperties.insert("Descriptors", QVariant::fromValue(mRX->getDescriptors()));
    rxInterfaces.insert(GATT_CHRC_IFACE, rxProperties);
    response.insert(QDBusObjectPath(RX_PATH), rxInterfaces);

    txProperties.insert("Service", QVariant::fromValue(mTX->getService()));
    txProperties.insert("UUID", mTX->getUuid());
    txProperties.insert("Flags", mTX->getFlags());
    txProperties.insert("Descriptors", QVariant::fromValue(mTX->getDescriptors()));
    txInterfaces.insert(GATT_CHRC_IFACE, txProperties);
    response.insert(QDBusObjectPath(TX_PATH), txInterfaces);

    return response;
}
