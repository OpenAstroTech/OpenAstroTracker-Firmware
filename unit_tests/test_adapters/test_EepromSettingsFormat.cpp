#include <gtest/gtest.h>

#include "adapters/EepromPersistentStore.hpp"
#include "adapters/EepromSettingsFormat.hpp"
#include "adapters/NullLogger.hpp"
#include "hal/InMemoryEeprom.hpp"

namespace
{

constexpr size_t kStoreSize = hal::eeprom::STORE_SIZE;

/// The defaults a real board passes in; the values only have to be distinguishable from zero.
adapters::EepromSettingsFormat::AxisDefaults testDefaults()
{
    return adapters::EepromSettingsFormat::AxisDefaults {100.0f, 200.0f, 55.0f, 66.0f, 8, 8, 64, 64, 12};
}

struct Fixture {
    hal::InMemoryEeprom<kStoreSize> eeprom;
    adapters::EepromPersistentStore store {eeprom, kStoreSize};
    adapters::NullLogger logger;
    /// The format holds the defaults by reference (they are board constants), so the test owns them.
    adapters::EepromSettingsFormat::AxisDefaults defaults = testDefaults();
    adapters::EepromSettingsFormat format {store, logger, defaults};
};

}  // namespace

TEST(EepromSettingsFormatTest, ABlankStoreAnswersWithTheBoardDefaults)
{
    Fixture f;

    EXPECT_FLOAT_EQ(100.0f, f.format.getRAStepsPerDegree());
    EXPECT_FLOAT_EQ(200.0f, f.format.getDECStepsPerDegree());
    EXPECT_FLOAT_EQ(55.0f, f.format.getAZStepsPerDegree());
    EXPECT_FLOAT_EQ(66.0f, f.format.getALTStepsPerDegree());
    EXPECT_EQ(12, f.format.getBacklashCorrectionSteps());
    EXPECT_FLOAT_EQ(1.0f, f.format.getSpeedFactor());
    EXPECT_FLOAT_EQ(45.0f, f.format.getLatitude().getTotalHours());
    EXPECT_FLOAT_EQ(100.0f, f.format.getLongitude().getTotalHours());
}

TEST(EepromSettingsFormatTest, TheStepsPerDegreeSurviveARoundTrip)
{
    Fixture f;

    f.format.storeRAStepsPerDegree(123.45f);
    f.format.storeDECStepsPerDegree(98.25f);
    f.format.storeAZStepsPerDegree(11.5f);
    f.format.storeALTStepsPerDegree(22.75f);

    EXPECT_NEAR(123.45f, f.format.getRAStepsPerDegree(), 0.01f);
    EXPECT_NEAR(98.25f, f.format.getDECStepsPerDegree(), 0.01f);
    EXPECT_NEAR(11.5f, f.format.getAZStepsPerDegree(), 0.01f);
    EXPECT_NEAR(22.75f, f.format.getALTStepsPerDegree(), 0.01f);
}

TEST(EepromSettingsFormatTest, TheSiteTimeValuesSurviveARoundTrip)
{
    Fixture f;

    f.format.storeLatitude(core::Latitude(52.5f));
    f.format.storeLongitude(core::Longitude(-13.25f));
    f.format.storeHATime(core::DayTime(7, 30, 0));
    f.format.storeUtcOffset(2);
    f.format.storeBrightness(77);

    EXPECT_NEAR(52.5f, f.format.getLatitude().getTotalHours(), 0.01f);
    EXPECT_NEAR(-13.25f, f.format.getLongitude().getTotalHours(), 0.01f);
    EXPECT_EQ(7, f.format.getHATime().getHours());
    EXPECT_EQ(30, f.format.getHATime().getMinutes());
    EXPECT_EQ(2, f.format.getUtcOffset());
    EXPECT_EQ(77, f.format.getBrightness());
}

TEST(EepromSettingsFormatTest, TheCalibrationValuesSurviveARoundTrip)
{
    Fixture f;

    // A factor above one: the legacy encoding reads the stored delta back unsigned, so a factor
    // below one has never round-tripped. Preserved, not fixed.
    f.format.storeSpeedFactor(1.02f);
    f.format.storeBacklashCorrectionSteps(-14);
    f.format.storePitchCalibrationAngle(1.25f);
    f.format.storeRollCalibrationAngle(-0.75f);

    EXPECT_NEAR(1.02f, f.format.getSpeedFactor(), 0.0005f);
    EXPECT_EQ(-14, f.format.getBacklashCorrectionSteps());
    EXPECT_FLOAT_EQ(1.25f, f.format.getPitchCalibrationAngle());
    EXPECT_FLOAT_EQ(-0.75f, f.format.getRollCalibrationAngle());
}

TEST(EepromSettingsFormatTest, TheHomingOffsetsAndLimitsSurviveARoundTrip)
{
    Fixture f;

    f.format.storeRAHomingOffset(-1234);
    f.format.storeDECHomingOffset(4321);
    f.format.storeDECLowerLimit(-15.5f);
    f.format.storeDECUpperLimit(87.25f);
    f.format.storeAZPosition(1200);
    f.format.storeALTPosition(-300);

    EXPECT_EQ(-1234, f.format.getRAHomingOffset());
    EXPECT_EQ(4321, f.format.getDECHomingOffset());
    EXPECT_FLOAT_EQ(-15.5f, f.format.getDECLowerLimit());
    EXPECT_FLOAT_EQ(87.25f, f.format.getDECUpperLimit());
    EXPECT_EQ(1200, f.format.getAZPosition());
    EXPECT_EQ(-300, f.format.getALTPosition());
}

TEST(EepromSettingsFormatTest, ClearingTheConfigurationReturnsEveryValueToItsDefault)
{
    Fixture f;

    f.format.storeRAStepsPerDegree(123.0f);
    f.format.storeBrightness(200);
    f.format.clearConfiguration();

    EXPECT_FLOAT_EQ(100.0f, f.format.getRAStepsPerDegree());
    EXPECT_EQ(10, f.format.getBrightness());  // the display's own default, as before
}

TEST(EepromSettingsFormatTest, TheLastFlashedVersionSurvivesARoundTrip)
{
    Fixture f;

    f.format.storeLastFlashedVersion(11307);

    EXPECT_EQ(11307, f.format.getLastFlashedVersion());
}

TEST(EepromSettingsFormatTest, AnUnwrittenStoreReportsVersionZero)
{
    Fixture f;

    EXPECT_EQ(0, f.format.getLastFlashedVersion());
}
