#ifndef GAMEPADDISPLAYWIDGET_H
#define GAMEPADDISPLAYWIDGET_H

#include <QWidget>
#include "src/protocol/gamepad_packet.h"

class GamepadDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GamepadDisplayWidget(QWidget* parent = nullptr);

public slots:
    void updateState(const GamepadPacket& packet);
    void resetState();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    GamepadPacket m_currentState;
};

#endif