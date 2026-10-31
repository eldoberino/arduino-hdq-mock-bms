#pragma once

#include "MovingAverage.h"
#include "StableDigitalInput.h"

class ArduinoBoard
{
public:
    explicit ArduinoBoard(
        int buttonSampleCount,
        int tidVoltageMovingAverageWindowSize = 10,
        int digitalInputStableWindowSize = -1)
        : stableInput(digitalInputStableWindowSize > 0 ? digitalInputStableWindowSize : buttonSampleCount)
        , batteryVoltage(tidVoltageMovingAverageWindowSize)
        , tidVoltage(tidVoltageMovingAverageWindowSize)
    {
    }

    virtual ~ArduinoBoard() = default;

    virtual void setLED(unsigned index, bool active) = 0;
    virtual void activateCharger(bool active) = 0;

    float getBatteryVoltage() const
    {
        return batteryVoltage.update(doGetBatteryVoltage());
    }

    float getTIDVoltage() const
    {
        return tidVoltage.update(doGetTIDVoltage());
    }

    bool getButtonState() const
    {
        return stableInput.update(doGetButtonState());
    }

private:
    virtual bool doGetButtonState() const = 0;
    virtual float doGetBatteryVoltage() const = 0;
    virtual float doGetTIDVoltage() const = 0;

    mutable StableDigitalInput stableInput;
    mutable MovingAverage batteryVoltage;
    mutable MovingAverage tidVoltage;
};
