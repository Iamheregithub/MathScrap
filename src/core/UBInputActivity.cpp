// SPDX-License-Identifier: GPL-3.0-only
#include "UBInputActivity.h"
#include <QApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QDebug>

UBInputActivity *UBInputActivity::instance()
{
    static UBInputActivity *activity = new UBInputActivity(qApp);
    return activity;
}

UBInputActivity::UBInputActivity(QObject *parent) : QObject(parent)
{
    mActivity.start(); mHeartbeat.start();
    qApp->installEventFilter(this);
    auto timer = new QTimer(this);
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, [this] {
        const auto gap = mHeartbeat.restart();
        if (gap > 250 && mActivity.elapsed() < 5000)
            qWarning() << "mathscrap.performance UI event-loop gap ms:" << gap;
    });
    timer->start();
}

bool UBInputActivity::isIdle(int milliseconds) const
{
    return !mTabletDown && QApplication::mouseButtons() == Qt::NoButton
        && mActivity.elapsed() >= milliseconds;
}

bool UBInputActivity::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::TabletPress:
        mTabletDown = true; mTabletPacket.start(); mActivity.restart(); break;
    case QEvent::TabletMove:
        if (mTabletDown) {
            const auto gap = mTabletPacket.restart();
            if (gap > 250)
                qWarning() << "mathscrap.performance stylus delivery gap ms:" << gap;
            mActivity.restart();
        }
        break;
    case QEvent::TabletRelease:
    case QEvent::TabletLeaveProximity:
        mTabletDown = false; mActivity.restart(); break;
    case QEvent::MouseMove:
        if (static_cast<QMouseEvent *>(event)->buttons() != Qt::NoButton) mActivity.restart();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::KeyPress:
    case QEvent::Wheel:
        mActivity.restart(); break;
    case QEvent::ApplicationDeactivate:
        mTabletDown = false; mActivity.restart(); break;
    default: break;
    }
    return false;
}
