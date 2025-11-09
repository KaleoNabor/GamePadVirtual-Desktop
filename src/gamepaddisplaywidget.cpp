#include "gamepaddisplaywidget.h"
#include <QPainter>
#include <QPaintEvent>

// =============================================
// CONSTRUTOR E INICIALIZAÇÃO
// =============================================

GamepadDisplayWidget::GamepadDisplayWidget(QWidget* parent)
    : QWidget(parent),
    m_controllerType(ControllerType::DualShock4)
{
    memset(&m_currentState, 0, sizeof(GamepadPacket));
    setMinimumSize(400, 300);
}

// =============================================
// ATUALIZAÇÃO DE ESTADO
// =============================================

void GamepadDisplayWidget::updateState(const GamepadPacket& packet)
{
    m_currentState = packet;
    update();
}

void GamepadDisplayWidget::resetState()
{
    memset(&m_currentState, 0, sizeof(GamepadPacket));
    update();
}

void GamepadDisplayWidget::setControllerType(int typeIndex)
{
    ControllerType newType = static_cast<ControllerType>(typeIndex);
    if (m_controllerType != newType) {
        m_controllerType = newType;
        update();
    }
}

// =============================================
// RENDERIZAÇÃO DO CONTROLE
// =============================================

void GamepadDisplayWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Configuração de cores e dimensões
    const QColor colorPressed = QColor("#4CAF50");
    const QColor colorReleased = QColor("#424242");
    const QColor colorText = Qt::white;
    const QColor colorStickBase = Qt::lightGray;
    const QColor colorStickKnob = Qt::darkGray;

    int width = this->width();
    int height = this->height();
    int buttonSize = qMin(width, height) / 10;
    int stickBaseSize = buttonSize * 2.5;

    // Posicionamento dos botões principais
    QRectF yButton(width * 0.72, height * 0.31, buttonSize, buttonSize);
    QRectF bButton(width * 0.78, height * 0.41, buttonSize, buttonSize);
    QRectF aButton(width * 0.72, height * 0.51, buttonSize, buttonSize);
    QRectF xButton(width * 0.66, height * 0.41, buttonSize, buttonSize);

    // Posicionamento dos analógicos
    QRectF lsBase(width * 0.18, height * 0.65, stickBaseSize, stickBaseSize);
    QRectF rsBase(width * 0.72, height * 0.65, stickBaseSize, stickBaseSize);

    // Posicionamento do D-Pad
    int dpadCenterX = width * 0.3;
    int dpadCenterY = height * 0.45;
    int dpadArmWidth = buttonSize * 0.7;
    int dpadArmLength = buttonSize * 1.3;
    QRectF dpadUp(dpadCenterX - dpadArmWidth / 2, dpadCenterY - dpadArmLength, dpadArmWidth, dpadArmLength);
    QRectF dpadDown(dpadCenterX - dpadArmWidth / 2, dpadCenterY, dpadArmWidth, dpadArmLength);
    QRectF dpadLeft(dpadCenterX - dpadArmLength, dpadCenterY - dpadArmWidth / 2, dpadArmLength, dpadArmWidth);
    QRectF dpadRight(dpadCenterX, dpadCenterY - dpadArmWidth / 2, dpadArmLength, dpadArmWidth);
    QRectF dpadCenter(dpadCenterX - dpadArmWidth / 2, dpadCenterY - dpadArmWidth / 2, dpadArmWidth, dpadArmWidth);

    // Posicionamento dos gatilhos e botões superiores
    QRectF l1Button(width * 0.15, height * 0.12, width * 0.12, height * 0.07);
    QRectF r1Button(width * 0.73, height * 0.12, width * 0.12, height * 0.07);
    QRectF l2Button(width * 0.15, height * 0.20, width * 0.12, height * 0.05);
    QRectF r2Button(width * 0.73, height * 0.20, width * 0.12, height * 0.05);
    QRectF selectButton(width * 0.42, height * 0.18, width * 0.07, height * 0.04);
    QRectF startButton(width * 0.51, height * 0.18, width * 0.07, height * 0.04);

    // Sistema de labels dinâmicos baseado no tipo de controle
    QString yLabel, bLabel, aLabel, xLabel;
    if (m_controllerType == ControllerType::DualShock4) {
        yLabel = "\u25B3";
        bLabel = "\u25EF";
        aLabel = "\u2715";
        xLabel = "\u25A1";
    }
    else {
        yLabel = "Y";
        bLabel = "B";
        aLabel = "A";
        xLabel = "X";
    }

    // Renderização do D-Pad
    QColor dpadColor = colorReleased;
    painter.setBrush(dpadColor);
    painter.drawRect(dpadUp);
    painter.drawRect(dpadDown);
    painter.drawRect(dpadLeft);
    painter.drawRect(dpadRight);
    painter.drawRect(dpadCenter);

    if (m_currentState.buttons & DPAD_UP) {
        painter.setBrush(colorPressed);
        painter.drawRect(dpadUp);
        painter.drawRect(dpadCenter);
    }
    if (m_currentState.buttons & DPAD_DOWN) {
        painter.setBrush(colorPressed);
        painter.drawRect(dpadDown);
        painter.drawRect(dpadCenter);
    }
    if (m_currentState.buttons & DPAD_LEFT) {
        painter.setBrush(colorPressed);
        painter.drawRect(dpadLeft);
        painter.drawRect(dpadCenter);
    }
    if (m_currentState.buttons & DPAD_RIGHT) {
        painter.setBrush(colorPressed);
        painter.drawRect(dpadRight);
        painter.drawRect(dpadCenter);
    }

    // Renderização dos botões ABXY
    painter.setBrush((m_currentState.buttons & Y) ? colorPressed : colorReleased);
    painter.drawEllipse(yButton);
    painter.setBrush((m_currentState.buttons & B) ? colorPressed : colorReleased);
    painter.drawEllipse(bButton);
    painter.setBrush((m_currentState.buttons & A) ? colorPressed : colorReleased);
    painter.drawEllipse(aButton);
    painter.setBrush((m_currentState.buttons & X) ? colorPressed : colorReleased);
    painter.drawEllipse(xButton);

    // Renderização dos labels dos botões
    painter.setPen(colorText);
    QFont font = painter.font();
    font.setPointSize(buttonSize / 2.2);
    painter.setFont(font);

    painter.drawText(yButton, Qt::AlignCenter, yLabel);
    painter.drawText(bButton, Qt::AlignCenter, bLabel);
    painter.drawText(aButton, Qt::AlignCenter, aLabel);
    painter.drawText(xButton, Qt::AlignCenter, xLabel);

    // Renderização dos botões L1/R1
    painter.setBrush((m_currentState.buttons & L1) ? colorPressed : colorReleased);
    painter.drawRoundedRect(l1Button, 8, 8);
    painter.drawText(l1Button, Qt::AlignCenter, "L1");

    painter.setBrush((m_currentState.buttons & R1) ? colorPressed : colorReleased);
    painter.drawRoundedRect(r1Button, 8, 8);
    painter.drawText(r1Button, Qt::AlignCenter, "R1");

    // Renderização dos gatilhos L2/R2
    painter.setBrush(QColor(70, 70, 70));
    painter.drawRoundedRect(l2Button, 5, 5);
    painter.drawRoundedRect(r2Button, 5, 5);

    painter.setPen(colorText);
    QFont smallFont = painter.font();
    smallFont.setPointSize(buttonSize / 3.5);
    painter.setFont(smallFont);

    painter.drawText(l2Button, Qt::AlignCenter, QString("L2: %1").arg(m_currentState.leftTrigger));
    painter.drawText(r2Button, Qt::AlignCenter, QString("R2: %1").arg(m_currentState.rightTrigger));

    // Renderização dos botões Select/Start
    painter.setBrush((m_currentState.buttons & SELECT) ? colorPressed : colorReleased);
    painter.drawRoundedRect(selectButton, 4, 4);
    painter.drawText(selectButton, Qt::AlignCenter, "Select");

    painter.setBrush((m_currentState.buttons & START) ? colorPressed : colorReleased);
    painter.drawRoundedRect(startButton, 4, 4);
    painter.drawText(startButton, Qt::AlignCenter, "Start");

    // Renderização dos analógicos
    painter.setPen(Qt::NoPen);
    painter.setBrush(colorStickBase);
    painter.drawEllipse(lsBase);
    painter.drawEllipse(rsBase);

    // Cálculo da posição dos knobs dos analógicos
    QPointF lsCenter = lsBase.center();
    QPointF rsCenter = rsBase.center();
    float lsKnobX = lsCenter.x() + (m_currentState.leftStickX / 127.0f) * (lsBase.width() / 2 - 10);
    float lsKnobY = lsCenter.y() + (m_currentState.leftStickY / 127.0f) * (lsBase.height() / 2 - 10);
    float rsKnobX = rsCenter.x() + (m_currentState.rightStickX / 127.0f) * (rsBase.width() / 2 - 10);
    float rsKnobY = rsCenter.y() + (m_currentState.rightStickY / 127.0f) * (rsBase.height() / 2 - 10);

    int knobSize = stickBaseSize / 2.5;
    QRectF lsKnobRect(lsKnobX - knobSize / 2, lsKnobY - knobSize / 2, knobSize, knobSize);
    QRectF rsKnobRect(rsKnobX - knobSize / 2, rsKnobY - knobSize / 2, knobSize, knobSize);

    // Renderização dos knobs dos analógicos
    painter.setBrush((m_currentState.buttons & L3) ? colorPressed : colorStickKnob);
    painter.drawEllipse(lsKnobRect);

    painter.setBrush((m_currentState.buttons & R3) ? colorPressed : colorStickKnob);
    painter.drawEllipse(rsKnobRect);

    // Labels dos analógicos
    painter.setPen(colorText);
    painter.setFont(smallFont);
    painter.drawText(lsBase, Qt::AlignCenter, "L3");
    painter.drawText(rsBase, Qt::AlignCenter, "R3");
}