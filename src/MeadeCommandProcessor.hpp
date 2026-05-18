#pragma once

#include "core/MeadeParser.hpp"
#include "core/MeadeResponse.hpp"

// Forward declarations
class Mount;
class LcdMenu;

class MeadeCommandProcessor : private oat::core::meade::IMeadeGetHandlers, private oat::core::meade::IMeadeSetHandlers
{
  public:
    static MeadeCommandProcessor *createProcessor(Mount *mount, LcdMenu *lcdMenu);
    static MeadeCommandProcessor *instance();
    const char *processCommand(String inCmd);

  private:
    MeadeCommandProcessor(Mount *mount, LcdMenu *lcdMenu);

    // Persist a freshly-built response across the handler return.
    // The returned pointer is valid until the next call to `store`.
    const char *store(oat::core::meade::MeadeResponse response);
    const char *handleMeadeSetInfo(const String &inCmd);
    const char *handleMeadeMovement(const String &inCmd);
    const char *handleMeadeGetInfo(const String &inCmd);
    const char *handleMeadeGPSCommands(const String &inCmd);
    const char *handleMeadeSyncControl(const String &inCmd);
    const char *handleMeadeHome(const String &inCmd);
    const char *handleMeadeInit(const String &inCmd);
    const char *handleMeadeQuit(const String &inCmd);
    const char *handleMeadeDistance(const String &inCmd);
    const char *handleMeadeSetSlewRate(const String &inCmd);
    const char *handleMeadeExtraCommands(const String &inCmd);
    const char *handleMeadeFocusCommands(const String &inCmd);

    // IMeadeGetHandlers overrides. Each method returns a typed domain value;
    // the parser layer handles all Meade wire formatting.
    const char *onFirmwareVersion() override;
    const char *onProductName() override;
    oat::core::meade::RaCoordinate onCurrentRa() override;
    oat::core::meade::RaCoordinate onTargetRa() override;
    oat::core::meade::DecCoordinate onCurrentDec() override;
    oat::core::meade::DecCoordinate onTargetDec() override;
    const char *onMountStatus() override;
    bool onIsSlewing() override;
    bool onIsTracking() override;
    bool onIsGuiding() override;
    oat::core::meade::MeadeLatitude onSiteLatitude() override;
    oat::core::meade::MeadeLongitude onSiteLongitude() override;
    int onUtcOffset() override;
    oat::core::meade::MeadeLocalTime onLocalTime() override;
    oat::core::meade::MeadeLocalDate onLocalDate() override;
    oat::core::meade::MeadeClockFormat onClockFormat() override;
    oat::core::meade::MeadeTrackingRate onTrackingRate() override;
    const char *onSiteName(uint8_t index) override;

    // IMeadeSetHandlers overrides. Each method receives a parser-validated
    // typed value and returns whether the mount accepted it.
    bool onSetTargetDec(oat::core::meade::DecCoordinate dec) override;
    bool onSetTargetRa(oat::core::meade::RaCoordinate ra) override;
    bool onSetLocalSiderealTime(oat::core::meade::MeadeLocalTime lst) override;
    bool onSetHomePoint() override;
    bool onSetHourAngle(uint8_t hours, uint8_t minutes) override;
    bool onSyncCoordinates(oat::core::meade::DecCoordinate dec, oat::core::meade::RaCoordinate ra) override;
    bool onSetSiteLatitude(oat::core::meade::MeadeLatitude lat) override;
    bool onSetSiteLongitude(oat::core::meade::MeadeLongitude lon) override;
    bool onSetUtcOffset(int hours) override;
    bool onSetLocalTime(oat::core::meade::MeadeLocalTime t) override;
    bool onSetLocalDate(oat::core::meade::MeadeLocalDate d) override;

    Mount *_mount;
    LcdMenu *_lcdMenu;
    static MeadeCommandProcessor *_instance;
    oat::core::meade::MeadeResponse _response;

    // Storage backing the pointer-returning Get callbacks (firmware/product
    // names use string literals; mount status / site name use these buffers).
    String _mountStatusScratch;
    char _siteNameScratch[8];
};
