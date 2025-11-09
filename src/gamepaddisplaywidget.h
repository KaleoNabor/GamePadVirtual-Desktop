#ifndef GAMEPADDISPLAYWIDGET_H
#define GAMEPADDISPLAYWIDGET_H

#include <QWidget>
#include "src/protocol/gamepad_packet.h"
#include "controller_types.h"

class GamepadDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GamepadDisplayWidget(QWidget* parent = nullptr);

public slots:
    // Controle de estado e configuração
    void updateState(const GamepadPacket& packet);
    void resetState();
    void setControllerType(int typeIndex);

protected:
    // Sistema de renderização
    void paintEvent(QPaintEvent* event) override;

private:
    // Dados do controle atual
    GamepadPacket m_currentState;

    // Tipo de controle para exibição visual
    ControllerType m_controllerType;
};

#endif