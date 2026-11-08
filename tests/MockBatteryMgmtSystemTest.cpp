#include <cstddef>
#include <algorithm>

#include <gtest/gtest.h>

#include "MockBatteryMgmtSystem.h"
#include "StableDigitalInput.h"
#include "MovingAverage.h"

class TestArduinoBoard final : public ArduinoBoard
{
public:
    TestArduinoBoard(
        bool buttonState = false,
        int sampleCount = 1,
        float batteryVoltage = (MockBatteryMgmtSystem::EmptyVoltage + MockBatteryMgmtSystem::FullVoltage)/2.0F,
        int voltageMovingAverageWindowSize = 1,
        int digitalInputStableWindowSize = 1)
        : ArduinoBoard(sampleCount, voltageMovingAverageWindowSize, digitalInputStableWindowSize), buttonState(buttonState), batteryVoltage(batteryVoltage)
    {
    }

    void setLED(unsigned index, bool active) override
    {
        ledStates[index] = active;
    }

    void activateCharger(bool active) override
    {
        chargerActive = active;
    }

    bool isChargerActive() const { return chargerActive; }

    bool isActive(unsigned index) const
    {
        return ledStates[index];
    }

    void setButtonState(bool state) { buttonState = state; }
    void setBatteryVoltage(float voltage) { batteryVoltage = voltage; }
    void setTIDVoltage(float voltage) { tidVoltage = voltage; }
    void placeIntoCharger() { setTIDVoltage(5.0F); }

private:
    bool doGetButtonState() const override { return buttonState; }
    float doGetBatteryVoltage() const override { return batteryVoltage; }
    float doGetTIDVoltage() const override { return tidVoltage; }

    std::array<bool, 5> ledStates{false, false, false, false, false};
    bool buttonState{false};
    bool chargerActive{false};
    float batteryVoltage{7*3.7F};
    float tidVoltage{0.0F};
};

static void awaitStartOfCharging(MockBatteryMgmtSystem& mockBMS)
{
    for (unsigned update = 0;
         update < MockBatteryMgmtSystem::ChargerActivationDelayUpdates;
         ++update)
    {
        mockBMS.update();
    }
}

TEST(ArduinoBoard, ActivateChargerSetsChargerOutput)
{
    TestArduinoBoard board;

    board.activateCharger(true);
    EXPECT_TRUE(board.isChargerActive());

    board.activateCharger(false);
    EXPECT_FALSE(board.isChargerActive());
}

TEST(ArduinoBoard, GetTIDVoltageUsesSameMovingAverageBehaviorAsBatteryVoltage)
{
    TestArduinoBoard board{false, 3, 36.0F, 3};

    board.setTIDVoltage(30.0F);
    EXPECT_FLOAT_EQ(board.getTIDVoltage(), 30.0F);

    board.setTIDVoltage(36.0F);
    EXPECT_FLOAT_EQ(board.getTIDVoltage(), 33.0F);

    board.setTIDVoltage(42.0F);
    EXPECT_FLOAT_EQ(board.getTIDVoltage(), 36.0F);
}

void expectLEDStates(
    TestArduinoBoard& board,
    const std::array<bool, 5>& expectedStates)
{
    for (unsigned led = 0; led < expectedStates.size(); ++led) {
        EXPECT_EQ(board.isActive(led), expectedStates[led])
            << "Unexpected state for LED " << led;
    }
}

void expectDisplayOn(TestArduinoBoard& board, bool isDisplayOn = true)
{
    std::array<unsigned, 5> ledIndices{0, 1, 2, 3, 4};
    const auto activeLEDs = std::count_if(
        ledIndices.begin(), ledIndices.end(),
        [&board](unsigned index) { return board.isActive(index); });
    EXPECT_EQ(activeLEDs > 0, isDisplayOn);
}

void setSoCPercentage(TestArduinoBoard& board, int percentage)
{
    board.setBatteryVoltage(
        0.01F + percentage / 100.0F *
            (MockBatteryMgmtSystem::FullVoltage - MockBatteryMgmtSystem::EmptyVoltage) +
        MockBatteryMgmtSystem::EmptyVoltage);
}

void displayStateOfChargeAndExpectStableLEDs(
    int percentage,
    const std::array<bool, 5>& expectedStates)
{
    constexpr unsigned updatesPerBlinkPhase = 5;
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board, updatesPerBlinkPhase};
    // we set to charging mode -> SoC gets displayed all the time
    board.placeIntoCharger();
    setSoCPercentage(board, percentage);
    EXPECT_TRUE(mockBMS.isBatteryInCharger());
    EXPECT_EQ(percentage, mockBMS.getStateOfCharge());

    for (unsigned update = 0;
         update <= updatesPerBlinkPhase;
         ++update) {
        mockBMS.update();
        expectLEDStates(board, expectedStates);
    }
}

TEST(MockBatteryMgmtSystem, SoC90PercentSetsAllFiveLedsActive)
{
    EXPECT_EQ(MockBatteryMgmtSystem::LedThresholds[4], 90);

    displayStateOfChargeAndExpectStableLEDs(
        90,
        {true, true, true, true, true});
}

TEST(MockBatteryMgmtSystem, SoC70PercentSetsFirstFourLedsActive)
{
    EXPECT_EQ(MockBatteryMgmtSystem::LedThresholds[3], 70);

    displayStateOfChargeAndExpectStableLEDs(
        70,
        {true, true, true, true, false});
}

TEST(MockBatteryMgmtSystem, SoC50PercentSetsFirstThreeLedsActive)
{
    EXPECT_EQ(MockBatteryMgmtSystem::LedThresholds[2], 50);

    displayStateOfChargeAndExpectStableLEDs(
        50,
        {true, true, true, false, false});
}

TEST(MockBatteryMgmtSystem, SoC30PercentSetsFirstTwoLedsActive)
{
    EXPECT_EQ(MockBatteryMgmtSystem::LedThresholds[1], 30);

    displayStateOfChargeAndExpectStableLEDs(
        30,
        {true, true, false, false, false});
}

TEST(MockBatteryMgmtSystem, SoC10PercentSetsFirstLedActive)
{
    EXPECT_EQ(MockBatteryMgmtSystem::LedThresholds[0], 10);
    displayStateOfChargeAndExpectStableLEDs(
        10,
        {true, false, false, false, false});
}        

TEST(MockBatteryMgmtSystem, SoCBelow10PercentBlinksFirstLed)
{
    constexpr unsigned updatesPerBlinkPhase = 5;
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board, updatesPerBlinkPhase};

    // we set to charging mode -> SoC gets displayed all the time
    // FIXME: use case where button gets pressed
    board.placeIntoCharger();
    const int percentage = 9;
    setSoCPercentage(board, percentage);
    EXPECT_TRUE(mockBMS.isBatteryInCharger());
    EXPECT_EQ(percentage, mockBMS.getStateOfCharge());
    
    for (unsigned cycle = 0; cycle < 2; ++cycle) {
        for (unsigned update = 0; update < updatesPerBlinkPhase; ++update) {
            mockBMS.update();
            expectLEDStates(board, {true, false, false, false, false});
        }

        for (unsigned update = 0; update < updatesPerBlinkPhase; ++update) {
            mockBMS.update();
            expectLEDStates(board, {false, false, false, false, false});
        }
    }
}

TEST(MockBatteryMgmtSystem, DisplayStateOfChargeDoesNotUpdateLedsUntilUpdateIsCalled)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    setSoCPercentage(board, 100);

    expectDisplayOn(board, false);
}

TEST(MockBatteryMgmtSystem, DisplayOffTurnsAllLedsOff)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    setSoCPercentage(board, 100);
    mockBMS.update();
    mockBMS.displayOff();

    EXPECT_FALSE(mockBMS.isBatteryInCharger());
    expectDisplayOn(board, false);

    // Turning the display off must also prevent a previously selected
    // display state from being rendered again by update().
    mockBMS.update();
    expectDisplayOn(board, false);
}

TEST(MockBatteryMgmtSystem, GetButtonStateReturnsDebouncedInputButtonState)
{
    TestArduinoBoard board{true, 3, 7*3.7F, 1, /*digitalInputStableWindowSize=*/ 3};
    MockBatteryMgmtSystem mockBMS{board};

    EXPECT_TRUE(mockBMS.getButtonState());

    board.setButtonState(false);
    EXPECT_TRUE(mockBMS.getButtonState());

    mockBMS.update();  // update also does a read of the button state
    EXPECT_FALSE(mockBMS.getButtonState());
}

TEST(MockBatteryMgmtSystem, GetStateOfChargeReturnsPercentageBasedOnBatteryVoltage)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.setBatteryVoltage(MockBatteryMgmtSystem::FullVoltage);
    EXPECT_EQ(mockBMS.getStateOfCharge(), 100);

    setSoCPercentage(board, 50);
    EXPECT_EQ(mockBMS.getStateOfCharge(), 50);

    board.setBatteryVoltage(MockBatteryMgmtSystem::EmptyVoltage);
    EXPECT_EQ(mockBMS.getStateOfCharge(), 0);
}

TEST(MockBatteryMgmtSystem, IsBatteryInChargerReturnsFalseIfNoVoltageAtTID)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.setTIDVoltage(0.0F);
    mockBMS.update();
    mockBMS.update();
    mockBMS.update();

    EXPECT_FALSE(mockBMS.isBatteryInCharger());
}

TEST(MockBatteryMgmtSystem, IsBatteryInChargerReturnsTrueIfVoltageAtTID)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.setTIDVoltage(0.5F);
    mockBMS.update();
    mockBMS.update();
    mockBMS.update();

    EXPECT_TRUE(mockBMS.isBatteryInCharger());
}

TEST(MockBatteryMgmtSystem, ChargeModeKeepsStateOfChargeAlwaysDisplayed)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    EXPECT_TRUE(mockBMS.isBatteryInCharger());

    mockBMS.update();

    expectDisplayOn(board);
    mockBMS.update();
    expectDisplayOn(board);
}

TEST(MockBatteryMgmtSystem, DisplaysStateOfChargeWhenChargerBecomesActive)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    mockBMS.update();

    expectDisplayOn(board);
}

TEST(MockBatteryMgmtSystem, TurnsDisplayOffWhenBatteryGetsRemovedFromCharger)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    mockBMS.update();
    ASSERT_TRUE(board.isActive(0));

    board.setTIDVoltage(0.0F);
    mockBMS.update();

    expectDisplayOn(board, false);
}

TEST(MockBatteryMgmtSystem, TurnsDisplayOffWhenBatteryIsCharged)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    setSoCPercentage(board, 90);
    mockBMS.update();
    ASSERT_TRUE(board.isActive(0));

    setSoCPercentage(board, 100);
    mockBMS.update();

    expectDisplayOn(board, false);
}

TEST(MockBatteryMgmtSystem, ActivatesChargerWhenBatteryIsInChargerAndNeedsCharging)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    EXPECT_FALSE(board.isChargerActive());
    board.placeIntoCharger();
    awaitStartOfCharging(mockBMS);

    EXPECT_TRUE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, DelaysChargerActivationForTenUpdates)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    for (unsigned update = 0;
         update < MockBatteryMgmtSystem::ChargerActivationDelayUpdates - 1;
         ++update) {
        mockBMS.update();
        EXPECT_FALSE(board.isChargerActive());
    }

    mockBMS.update();
    EXPECT_TRUE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, DeactivatesChargerWhenBatteryReaches92Percent)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    board.setBatteryVoltage(7 * 3.5F);
    awaitStartOfCharging(mockBMS);
    EXPECT_TRUE(board.isChargerActive());

    board.setBatteryVoltage(7 * 4.12F);
    mockBMS.update();

    EXPECT_FALSE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, KeepsChargerOffWithinChargeLimitHysteresis)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    setSoCPercentage(board, 100);
    mockBMS.update();
    ASSERT_FALSE(board.isChargerActive());

    board.setBatteryVoltage(7 * 4.05F);
    awaitStartOfCharging(mockBMS);

    EXPECT_FALSE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, ReactivatesChargerBelowChargeLimitHysteresis)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    board.setBatteryVoltage(7 * 4.1F);
    mockBMS.update();
    ASSERT_FALSE(board.isChargerActive());

    board.setBatteryVoltage(7 * 3.8F);
    awaitStartOfCharging(mockBMS);

    EXPECT_TRUE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, DeactivatesChargerWhenBatteryIsRemovedFromCharger)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.placeIntoCharger();
    awaitStartOfCharging(mockBMS);
    ASSERT_TRUE(board.isChargerActive());

    board.setTIDVoltage(0.0F);
    mockBMS.update();

    EXPECT_FALSE(board.isChargerActive());
}

TEST(MockBatteryMgmtSystem, ButtonDisplaysStateOfChargeFor50UpdatesWhenNotCharging)
{
    TestArduinoBoard board;
    MockBatteryMgmtSystem mockBMS{board};

    board.setTIDVoltage(0.0F);
    board.setButtonState(true);
    mockBMS.update();
    expectDisplayOn(board);

    board.setButtonState(false);
    for (unsigned update = 1;
         update < MockBatteryMgmtSystem::ButtonDisplayUpdates;
         ++update) {
        // std::cout << "iteration: " << update << '\n';
        mockBMS.update();
        expectDisplayOn(board);
    }

    mockBMS.update();
    expectDisplayOn(board, false);
}

TEST(StableDigitalInput, ItOnlyChangesStateWhenAllValuesAreEqual) {
  StableDigitalInput stableDigital(3);
  EXPECT_EQ(stableDigital.update(true), true);
  EXPECT_EQ(stableDigital.update(true), true);
  EXPECT_EQ(stableDigital.update(false), true);
  EXPECT_EQ(stableDigital.update(false), true);
  EXPECT_EQ(stableDigital.update(false), false);
  EXPECT_EQ(stableDigital.update(true), false);
  EXPECT_EQ(stableDigital.update(true), false);
  EXPECT_EQ(stableDigital.update(true), true);
}

TEST(MovingAverage, ItReturnsAverageOverPreviousNValues) {
  MovingAverage ma3(3);
  EXPECT_DOUBLE_EQ(ma3.update(1.0), 1.0);
  EXPECT_DOUBLE_EQ(ma3.update(2.0), 1.5);
  EXPECT_DOUBLE_EQ(ma3.update(3.0), 2.0);
  EXPECT_DOUBLE_EQ(ma3.update(4.0), 3.0);
  EXPECT_DOUBLE_EQ(ma3.update(5.0), 4.0);
}
