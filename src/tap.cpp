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

#include "tap.h"

#include <QProcess>
#include <QDBusServiceWatcher>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDebug>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/route.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define MY_ETH_TAP_HARD_FRAME_SIZE (14)

TAP::TAP(QObject *parent) : QObject(parent) {
    // Create the interface
    mFd = open("/dev/net/tun", O_RDWR);
    if (mFd < 0) {
        qCritical() << "Failed to open /dev/net/tun";
        exit(1);
    }

    struct ifreq ifr = {};
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (ioctl(mFd, TUNSETIFF, (void *)&ifr) < 0) {
        qCritical() << "Failed to create TAP interface";
        close(mFd);
        exit(1);
    }
    if (ioctl(mFd, TUNSETOFFLOAD, 0U) < 0) {
        qCritical() << "Failed to set the TAP offload";
    }

    mIfaceName = ifr.ifr_name;
    qDebug() << "TAP interface created:" << mIfaceName;

    iffReset(244);

    char discard[1500];
    auto drained = read(mFd, discard, 1500);
    qDebug() << "Drained" << drained << "bytes";

    // Poll the interface file descriptor
    mNotifier = new QSocketNotifier(mFd, QSocketNotifier::Read, this);
    QObject::connect(mNotifier, &QSocketNotifier::activated, this, &TAP::fdActivated);
}

void TAP::iffReset(int mtu) {
    struct ifreq ifr = {};
    auto sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        qCritical() << "Failed to create a socket";
        exit(1);
    }
    strncpy(ifr.ifr_name, mIfaceName.toLocal8Bit().data(), sizeof(ifr.ifr_name));
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        qCritical() << "Failed to get IFFLAGS";
        exit(1);
    }
    if (ifr.ifr_flags & IFF_UP) {
        ifr.ifr_flags &= ~IFF_UP;
        if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
            qCritical() << "Failed to set IFFLAGS";
            exit(1);
        }
    }
    if (mtu < 0) {
        qDebug() << "Interface down";
        close(sock);
        return;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        qCritical() << "Failed to set IFFLAGS";
        exit(1);
    }

    ifr.ifr_mtu = mtu - MY_ETH_TAP_HARD_FRAME_SIZE;
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        qCritical() << "Failed to set the MTU";
        exit(1);
    }

    close(sock);

    QDBusConnection bus = QDBusConnection::systemBus();
    QDBusInterface manager("net.connman", "/", "net.connman.Manager", bus);
    QDBusMessage result = manager.call("GetServices");

    const QDBusArgument argument = result.arguments().at(0).value<QDBusArgument>();
    QMap<QDBusObjectPath, QVariantMap> reply;
    argument >> reply;

    for (auto it = reply.keyValueBegin(); it != reply.keyValueEnd(); ++it) {
        const auto &key = it->first;
        const auto &value = it->second;

        if (value.contains("Ethernet")) {
            //QString name = value["Ethernet"].toMap()["Name"].toString();
            QDBusArgument ethernetArg = value["Ethernet"].value<QDBusArgument>();
            QVariantMap ethernetMap;
            ethernetArg >> ethernetMap;

            const auto name = ethernetMap["Interface"].toString();

            if (!strcmp(mIfaceName.toLocal8Bit().data(), name.toLocal8Bit().data())) {
                qDebug() << "Will reset the IPv4 configuration";

                QDBusInterface service("net.connman", key.path(), "net.connman.Service", bus);

                {
                    QDBusArgument propertiesArg = value["IPv4.Configuration"].value<QDBusArgument>();
                    QVariantMap propertiesMap;
                    propertiesArg >> propertiesMap;

                    propertiesMap["Method"] = "manual";
                    propertiesMap["Address"] = "10.0.2.3";
                    propertiesMap["Netmask"] = "255.255.255.0";
                    propertiesMap["Gateway"] = "10.0.2.2";

                    QVariantList args;
                    args << "IPv4.Configuration" << QVariant::fromValue(QDBusVariant(propertiesMap));
                    service.callWithArgumentList(QDBus::AutoDetect, "SetProperty", args);
                }

                {
                    QStringList propertiesMap;

                    propertiesMap << "10.0.2.2";
                    // TODO:XXX: CHANGE THIS!!!
                    propertiesMap << "8.8.4.4";
                    propertiesMap << "8.8.8.8";

                    QVariantList args;
                    args << "Nameservers.Configuration" << QVariant::fromValue(QDBusVariant(propertiesMap));
                    service.callWithArgumentList(QDBus::AutoDetect, "SetProperty", args);
                }
            }
        }
    }
}

void TAP::mtuChanged(int mtu) {
    struct ifreq ifr = {};
    auto sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        qCritical() << "Failed to create a socket";
        exit(1);
    }
    strncpy(ifr.ifr_name, mIfaceName.toLocal8Bit().data(), sizeof(ifr.ifr_name));
    ifr.ifr_mtu = mtu - MY_ETH_TAP_HARD_FRAME_SIZE;
    if (ioctl(sock, SIOCSIFMTU, &ifr) < 0) {
        qCritical() << "Failed to set the MTU";
        exit(1);
    }
    close(sock);

    qDebug() << "MTU reset to" << mtu;
}

// Called every time the watch kernel outputs ethernet frames to the network
void TAP::fdActivated() {
    char buffer[1500];
    qint64 bytesRead = read(mFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        QByteArray data(buffer, bytesRead);

        // qDebug() << "Received" << bytesRead << "bytes from TAP interface";
        // qDebug() << "Data:" << data.toHex();

        emit dataAvailable(data);
    } else
        qCritical() << "Failed to read from TAP interface";
}

// Called to forward data received from the watch back to the TAP interface
void TAP::send(const QByteArray &data) {
    qint64 bytesWritten = write(mFd, data.constData(), data.size());

    if (bytesWritten > 0) {
        // qDebug() << "Sent" << bytesWritten << "bytes to the TAP interface";
        // qDebug() << "Data:" << data.toHex();
    } else
        qCritical() << "Failed to write to TAP interface";
}

