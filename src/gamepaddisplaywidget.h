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
    // Atualiza o estado visual do gamepad
    void updateState(const GamepadPacket& packet);
    // Reseta o estado visual do gamepad
    void resetState();

protected:
    // Evento de pintura do widget
    void paintEvent(QPaintEvent* event) override;

private:
    // Estado atual do gamepad para exibição
    GamepadPacket m_currentState;
};

#endif