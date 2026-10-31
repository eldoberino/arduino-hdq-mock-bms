#pragma once

#include "ArduinoBoard.h"

class MockBatteryMgmtSystem
{
public:
    static constexpr float EmptyVoltage = 7 * 3.2F;
    static constexpr float FullVoltage = 7 * 4.2F;
    static constexpr int BlinkingThresholdPercent = 10;
    static constexpr unsigned ChargerActivationDelayUpdates = 20;
    static constexpr int ChargeLimitPercent = 92;
    static constexpr int ChargeLimitHysteresisPercent = 10;
    static constexpr unsigned ButtonDisplayUpdates = 50;
    static constexpr int LedThresholds[5]{
        BlinkingThresholdPercent, 30, 50, 70, 90};

    explicit MockBatteryMgmtSystem(
        ArduinoBoard& board,
        unsigned updatesPerBlinkPhase = 5);

    void displayOff();
    void update();
    bool getButtonState() const;
    int getStateOfCharge() const;
    bool isBatteryInCharger() const;

private:
    ArduinoBoard& board;
    unsigned updatesPerBlinkPhase;
    int percentage{0};
    unsigned blinkUpdateCount{0};
    unsigned chargerActivationUpdateCount{0};
    bool chargeLimitReached{false};
    bool displayEnabled{false};
    unsigned buttonDisplayUpdateCount{0};
};
