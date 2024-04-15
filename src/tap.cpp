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
#include <QDebug>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>

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

    QString ifaceName = ifr.ifr_name;
    qDebug() << "TAP interface created:" << ifaceName;

    // Bring the interface up
    if (QProcess::execute("ip", {"link", "set", "dev", ifaceName, "up"}) != 0) {
        qCritical() << "Failed to bring up TAP interface";
        exit(1);
    }
    qDebug() << "TAP interface" << ifaceName << "is up";
    
    // Configure routing tables
    if (QProcess::execute("ip", {"route", "add", "default", "dev", ifaceName}) != 0) {
        qCritical() << "Failed to configure routing table";
        exit(1);
    }
    qDebug() << "Routing table configured to send all traffic to" << ifaceName;

    // Poll the interface file descriptor
    mNotifier = new QSocketNotifier(mFd, QSocketNotifier::Read, this);
    QObject::connect(mNotifier, &QSocketNotifier::activated, this, &TAP::fdActivated);
}

// Called every time the watch kernel outputs ethernet frames to the network
void TAP::fdActivated() {
    char buffer[1500];
    qint64 bytesRead = read(mFd, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        QByteArray data(buffer, bytesRead);

        qDebug() << "Received" << bytesRead << "bytes from TAP interface";
        qDebug() << "Data:" << data.toHex();

        emit dataAvailable(data);
    } else
        qCritical() << "Failed to read from TAP interface";
}

// Called to forward data received from the watch back to the TAP interface
void TAP::send(const QByteArray &data) {
    qint64 bytesWritten = write(mFd, data.constData(), data.size());

    if (bytesWritten > 0) {
        qDebug() << "Sent" << bytesWritten << "bytes to the TAP interface";
        qDebug() << "Data:" << data.toHex();
    } else
        qCritical() << "Failed to write to TAP interface";
}

