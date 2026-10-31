#include "MockBatteryMgmtSystem.h"

MockBatteryMgmtSystem::MockBatteryMgmtSystem(
    ArduinoBoard& board,
    unsigned updatesPerBlinkPhase)
    : board(board),
      updatesPerBlinkPhase(updatesPerBlinkPhase)
{
}

bool MockBatteryMgmtSystem::getButtonState() const
{
    return board.getButtonState();
}

int MockBatteryMgmtSystem::getStateOfCharge() const
{
    const float voltage = board.getBatteryVoltage();

    if (voltage <= EmptyVoltage) {
        return 0;
    }
    if (voltage >= FullVoltage) {
        return 100;
    }

    return static_cast<int>(
        (voltage - EmptyVoltage) * 100.0F / (FullVoltage - EmptyVoltage));
}

bool MockBatteryMgmtSystem::isBatteryInCharger() const
{
    return board.getTIDVoltage() >= 0.5F;
}

void MockBatteryMgmtSystem::displayOff()
{
    displayEnabled = false;
    buttonDisplayUpdateCount = 0;

    for (unsigned led = 0; led < 5; ++led) {
        board.setLED(led, false);
    }
}

void MockBatteryMgmtSystem::update()
{
    const bool buttonPressed = getButtonState();
    const int stateOfCharge = getStateOfCharge();
    const bool batteryInCharger = isBatteryInCharger();
    const bool displayWasEnabled = displayEnabled;

    if (!batteryInCharger) {
        if (buttonPressed && !displayEnabled) {
            displayEnabled = true;
            buttonDisplayUpdateCount = ButtonDisplayUpdates + 1;
        }
        chargerActivationUpdateCount = 0;
        chargeLimitReached = false;
        board.activateCharger(false);
    } else if (stateOfCharge >= ChargeLimitPercent) {
        chargerActivationUpdateCount = 0;
        chargeLimitReached = true;
        board.activateCharger(false);
    } else if (chargeLimitReached &&
               stateOfCharge >= ChargeLimitPercent - ChargeLimitHysteresisPercent) {
        chargerActivationUpdateCount = 0;
        board.activateCharger(false);
    } else {
        chargeLimitReached = false;
        ++chargerActivationUpdateCount;
        if (chargerActivationUpdateCount >= ChargerActivationDelayUpdates) {
            board.activateCharger(true);
        }
    }

    if (batteryInCharger && !displayWasEnabled) {
        displayEnabled = true;
        buttonDisplayUpdateCount = 0;
    } else if (!batteryInCharger && displayWasEnabled && buttonDisplayUpdateCount == 0) {
        displayOff();
    }

    if (!displayEnabled) {
        return;
    }

    if (stateOfCharge < BlinkingThresholdPercent) {
        const bool ledOn = blinkUpdateCount < updatesPerBlinkPhase;

        board.setLED(0, ledOn);

        for (unsigned led = 1; led < 5; ++led) {
            board.setLED(led, false);
        }

        ++blinkUpdateCount;

        if (blinkUpdateCount >= updatesPerBlinkPhase * 2) {
            blinkUpdateCount = 0;
        }
    } else {
        blinkUpdateCount = 0;

        for (unsigned led = 0; led < 5; ++led) {
            board.setLED(led, stateOfCharge >= LedThresholds[led]);
        }
    }

    if (buttonDisplayUpdateCount > 0 && --buttonDisplayUpdateCount == 0) {
        displayOff();
    }
}
