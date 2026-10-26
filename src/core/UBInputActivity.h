// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QElapsedTimer>

// GUI-thread activity clock shared by automatic save and preview scheduling.
// A pen-up between letters is not an idle period.
class UBInputActivity : public QObject {
public:
    static UBInputActivity *instance();
    bool isIdle(int milliseconds = 1500) const;
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    explicit UBInputActivity(QObject *parent);
    QElapsedTimer mActivity, mHeartbeat, mTabletPacket;
    bool mTabletDown = false;
};
