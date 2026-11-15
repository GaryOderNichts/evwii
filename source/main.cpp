#include <vector>
#include <string>
#include <memory>
#include <cstring>

#include <wups.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>

#include <mocha/mocha.h>

#include <coreinit/mcp.h>
#include <coreinit/debug.h>

#include <dmcu_d_hex.h>
#include <OpenDMCUFirm/source/custom.h>

WUPS_PLUGIN_NAME("evWii");
WUPS_PLUGIN_DESCRIPTION("Patches to enhance the vWii mode");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("GaryOderNichts");
WUPS_PLUGIN_LICENSE("GPLv2");

WUPS_USE_STORAGE("evWii");
WUPS_USE_WUT_DEVOPTAB();

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle);
void ConfigMenuClosedCallback();

enum class DMCUSource : int {
    OpenDMCUFirm,
    Stock,
    SDCard,
};

struct DMCUViewportAxis {
    uint16_t start;
    uint16_t size;
    uint16_t end;

    auto operator<=>(const DMCUViewportAxis&) const = default;
};

struct DMCUViewport {
    DMCUViewportAxis x;
    DMCUViewportAxis y;
};

static std::pair<const char*, DMCUViewportAxis> viewportWidthPresetsTV[] = {
    { "720", { 0, 720, 720 } },
    { "704", { 8, 704, 720 } },
    { "640", { 40, 640, 720 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static std::pair<const char*, DMCUViewportAxis> viewportHeightPresetsTV[] = {
    { "480", { 0, 480, 480 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static std::pair<const char*, DMCUViewportAxis> viewportHeightPresetsTV_576i[] = {
    { "576", { 0, 576, 576 } },
    { "528", { 24, 528, 576 } },
    { "480", { 48, 480, 576 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static std::pair<const char*, DMCUViewportAxis> viewportWidthPresetsDRC[] = {
    { "720", { 0, 720, 720 } },
    { "704", { 8, 704, 720 } },
    { "640", { 40, 640, 720 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static std::pair<const char*, DMCUViewportAxis> viewportHeightPresetsDRC[] = {
    { "480", { 0, 480, 480 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static std::pair<const char*, DMCUViewportAxis> viewportHeightPresetsDRC_576i[] = {
    { "576", { 0, 576, 576 } },
    { "528", { 24, 528, 576 } },
    { "480", { 48, 480, 576 } },
    { "Keep default", { 0xffff, 0xffff, 0xffff } }
};

static bool enable4sPower = true;

static DMCUSource dmcuSource = DMCUSource::OpenDMCUFirm;

static DMCUViewport dmcuTVViewport = {
    { 8, 704, 720 },
    { 0, 480, 480 },
};

static DMCUViewport dmcuTVViewport_576i = {
    { 8, 704, 720 },
    { 0, 576, 576 },
};

static DMCUViewport dmcuDRCViewport = {
    { 8, 704, 720 },
    { 0, 480, 480 },
};

static DMCUViewport dmcuDRCViewport_576i = {
    { 8, 704, 720 },
    { 0, 576, 576 },
};

static ScalerOptions dmcuTVScalerOptions = {
    .enabled = 0xff, .nearest = 0xff,
};

static ScalerOptions dmcuDRCScalerOptions = {
    .enabled = 0xff, .nearest = 0xff,
};

void Patch_vWii_RTC_CONTROL1(void)
{
    if(Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        return;
    }

    uint32_t val;
    Mocha_IOSUKernelRead32(0x0501f838, &val);
    if (val != 0x238000db) {
        Mocha_DeInitLibrary();
        return;
    }

    // write a small stub which loads the RTC_CONTROL1 value into r3
    Mocha_IOSUKernelWrite32(0x05020044, 0x4b004770); // ldr r3, [pc, #0]; bx lr
    Mocha_IOSUKernelWrite32(0x05020048, 0x401); // (r3 = 0x401)

    // jump to the stub
    Mocha_IOSUKernelWrite32(0x0501f838, 0xf000fc04); // bl #0x80c

    Mocha_DeInitLibrary();
}

void Restore_vWii_RTC_CONTROL1(void)
{
    if(Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        return;
    }

    uint32_t val;
    Mocha_IOSUKernelRead32(0x0501f838, &val);
    if (val == 0x238000db) {
        Mocha_DeInitLibrary();
        return;
    }

    // restore the original instruction
    Mocha_IOSUKernelWrite32(0x0501f838, 0x238000db); // mov r3,#0x80; lsl r3,r3,#0x3 (r3 = 0x400)

    Mocha_DeInitLibrary();
}

#define WUPS_ReadIntLikeWithDefault(__parent__, __key__, __value__) {{                    \
    int tmp = 0;                                                                          \
    if (WUPSStorageAPI::GetEx(__parent__, __key__, tmp) == WUPS_STORAGE_ERROR_NOT_FOUND){ \
        tmp = static_cast<int>(__value__);                                                \
        WUPSStorageAPI::StoreEx(__parent__, __key__, tmp);                                \
    } else                                                                                \
        __value__ = static_cast<decltype(__value__)>(tmp);                                \
}}

INITIALIZE_PLUGIN()
{
    WUPSConfigAPIOptionsV1 configOptions = {.name = "evWii"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) !=
        WUPSCONFIG_API_RESULT_SUCCESS)
    {
        OSReport("evWii: Failed to init config api\n");
    }

    WUPSStorageAPI::GetOrStoreDefault("enable4sPower", enable4sPower, enable4sPower);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuSource", dmcuSource);

    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_xStart", dmcuTVViewport.x.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_yStart", dmcuTVViewport.y.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_xEnd", dmcuTVViewport.x.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_yEnd", dmcuTVViewport.y.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_xSize", dmcuTVViewport.x.size);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV_ySize", dmcuTVViewport.y.size);

    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_xStart", dmcuTVViewport_576i.x.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_yStart", dmcuTVViewport_576i.y.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_xEnd", dmcuTVViewport_576i.x.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_yEnd", dmcuTVViewport_576i.y.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_xSize", dmcuTVViewport_576i.x.size);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTV576i_ySize", dmcuTVViewport_576i.y.size);

    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_xStart", dmcuDRCViewport.x.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_yStart", dmcuDRCViewport.y.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_xEnd", dmcuDRCViewport.x.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_yEnd", dmcuDRCViewport.y.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_xSize", dmcuDRCViewport.x.size);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC_ySize", dmcuDRCViewport.y.size);

    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_xStart", dmcuDRCViewport_576i.x.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_yStart", dmcuDRCViewport_576i.y.start);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_xEnd", dmcuDRCViewport_576i.x.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_yEnd", dmcuDRCViewport_576i.y.end);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_xSize", dmcuDRCViewport_576i.x.size);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRC576i_ySize", dmcuDRCViewport_576i.y.size);

    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTVScalerEnabled", dmcuTVScalerOptions.enabled);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuTVScalerNearest", dmcuTVScalerOptions.nearest);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRCScalerEnabled", dmcuDRCScalerOptions.enabled);
    WUPS_ReadIntLikeWithDefault(nullptr, "dmcuDRCScalerNearest", dmcuDRCScalerOptions.nearest);

    WUPSStorageAPI::SaveStorage();

    if (enable4sPower) {
        Patch_vWii_RTC_CONTROL1();
    } else {
        Restore_vWii_RTC_CONTROL1();
    }
}

void enable4sPowerCallback(ConfigItemBoolean* item, bool enabled)
{
    enable4sPower = enabled;
    WUPSStorageAPI::Store("enable4sPower", enabled);

    if (enable4sPower) {
        Patch_vWii_RTC_CONTROL1();
    } else {
        Restore_vWii_RTC_CONTROL1();
    }
}

void dmcuSourceCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    dmcuSource = static_cast<DMCUSource>(i);
    WUPSStorageAPI::Store<int>("dmcuSource", i);
}

void selectDMCUViewportCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    if (i == 0xffffffff) {
        return;
    }

    if (strcmp(item->identifier, "dmcuTVViewportWidth") == 0) {
        dmcuTVViewport.x = viewportWidthPresetsTV[i].second;

        WUPSStorageAPI::Store<int>("dmcuTV_xStart", dmcuTVViewport.x.start);
        WUPSStorageAPI::Store<int>("dmcuTV_xEnd", dmcuTVViewport.x.end);
        WUPSStorageAPI::Store<int>("dmcuTV_xSize", dmcuTVViewport.x.size);
    } else if (strcmp(item->identifier, "dmcuTVViewportHeight") == 0) {
        dmcuTVViewport.y = viewportHeightPresetsTV[i].second;

        WUPSStorageAPI::Store<int>("dmcuTV_yStart", dmcuTVViewport.y.start);
        WUPSStorageAPI::Store<int>("dmcuTV_yEnd", dmcuTVViewport.y.end);
        WUPSStorageAPI::Store<int>("dmcuTV_ySize", dmcuTVViewport.y.size);
    } else if (strcmp(item->identifier, "dmcuTVViewportWidth576i") == 0) {
        dmcuTVViewport_576i.x = viewportWidthPresetsTV[i].second;

        WUPSStorageAPI::Store<int>("dmcuTV576i_xStart", dmcuTVViewport_576i.x.start);
        WUPSStorageAPI::Store<int>("dmcuTV576i_xEnd", dmcuTVViewport_576i.x.end);
        WUPSStorageAPI::Store<int>("dmcuTV576i_xSize", dmcuTVViewport_576i.x.size);
    } else if (strcmp(item->identifier, "dmcuTVViewportHeight576i") == 0) {
        dmcuTVViewport_576i.y = viewportHeightPresetsTV_576i[i].second;

        WUPSStorageAPI::Store<int>("dmcuTV576i_yStart", dmcuTVViewport_576i.y.start);
        WUPSStorageAPI::Store<int>("dmcuTV576i_yEnd", dmcuTVViewport_576i.y.end);
        WUPSStorageAPI::Store<int>("dmcuTV576i_ySize", dmcuTVViewport_576i.y.size);
    } else if (strcmp(item->identifier, "dmcuDRCViewportWidth") == 0) {
        dmcuDRCViewport.x = viewportWidthPresetsDRC[i].second;

        WUPSStorageAPI::Store<int>("dmcuDRC_xStart", dmcuDRCViewport.x.start);
        WUPSStorageAPI::Store<int>("dmcuDRC_xEnd", dmcuDRCViewport.x.end);
        WUPSStorageAPI::Store<int>("dmcuDRC_xSize", dmcuDRCViewport.x.size);
    } else if (strcmp(item->identifier, "dmcuDRCViewportHeight") == 0) {
        dmcuDRCViewport.y = viewportHeightPresetsDRC[i].second;

        WUPSStorageAPI::Store<int>("dmcuDRC_yStart", dmcuDRCViewport.y.start);
        WUPSStorageAPI::Store<int>("dmcuDRC_yEnd", dmcuDRCViewport.y.end);
        WUPSStorageAPI::Store<int>("dmcuDRC_ySize", dmcuDRCViewport.y.size);
    } else if (strcmp(item->identifier, "dmcuDRCViewportWidth576i") == 0) {
        dmcuDRCViewport_576i.x = viewportWidthPresetsDRC[i].second;

        WUPSStorageAPI::Store<int>("dmcuDRC576i_xStart", dmcuDRCViewport_576i.x.start);
        WUPSStorageAPI::Store<int>("dmcuDRC576i_xEnd", dmcuDRCViewport_576i.x.end);
        WUPSStorageAPI::Store<int>("dmcuDRC576i_xSize", dmcuDRCViewport_576i.x.size);
    } else if (strcmp(item->identifier, "dmcuDRCViewportHeight576i") == 0) {
        dmcuDRCViewport_576i.y = viewportHeightPresetsDRC_576i[i].second;
    
        WUPSStorageAPI::Store<int>("dmcuDRC576i_yStart", dmcuDRCViewport_576i.y.start);
        WUPSStorageAPI::Store<int>("dmcuDRC576i_yEnd", dmcuDRCViewport_576i.y.end);
        WUPSStorageAPI::Store<int>("dmcuDRC576i_ySize", dmcuDRCViewport_576i.y.size);
    }
}

void selectDMCUScalerEnabledCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    if (strcmp(item->identifier, "dmcuTVScalerEnabled") == 0) {
        dmcuTVScalerOptions.enabled = i;
        WUPSStorageAPI::Store<int>("dmcuTVScalerEnabled", i);
    } else if (strcmp(item->identifier, "dmcuDRCScalerEnabled") == 0) {
        dmcuDRCScalerOptions.enabled = i;
        WUPSStorageAPI::Store<int>("dmcuDRCScalerEnabled", i);
    }
}

void selectDMCUScalerNearestCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    if (strcmp(item->identifier, "dmcuTVScalerNearest") == 0) {
        dmcuTVScalerOptions.nearest = i;
        WUPSStorageAPI::Store<int>("dmcuTVScalerNearest", i);
    } else if (strcmp(item->identifier, "dmcuDRCScalerNearest") == 0) {
        dmcuDRCScalerOptions.nearest = i;
        WUPSStorageAPI::Store<int>("dmcuDRCScalerNearest", i);
    }
}

WUPSConfigItemMultipleValues CreateDmcuViewportAxisConfigItem(const char* identifier, const char* displayName,
                                                              const std::span<const std::pair<const char*, DMCUViewportAxis>> presets,
                                                              DMCUViewportAxis curValue)
{
    uint32_t numPairs = presets.size();
    int curIdx = -1;
    std::vector<WUPSConfigItemMultipleValues::ValuePair> pairs(numPairs);

    for (uint32_t i = 0; i < numPairs; i++) {
        pairs[i].value = i;
        pairs[i].name = presets[i].first;
        if (curValue == presets[i].second) {
            curIdx = i;
        }
    }

    if (curIdx == -1) {
        curIdx = numPairs;

        auto& customPair = pairs.emplace_back();
        customPair.value = 0xffffffff;
        customPair.name = "Custom";
    }

    return WUPSConfigItemMultipleValues::CreateFromIndex(identifier, displayName, 0, curIdx, pairs, selectDMCUViewportCallback);
}

WUPSConfigItemMultipleValues CreateDmcuBoolConfigItem(const char* identifier, const char* displayName, uint8_t current,
                                                      MultipleValuesChangedCallback callback)
{
    std::vector<WUPSConfigItemMultipleValues::ValuePair> pairs(3);
    pairs[0].name = "Keep default";
    pairs[0].value = 0xff;
    pairs[1].name = "Enabled";
    pairs[1].value = 1;
    pairs[2].name = "Disabled";
    pairs[2].value = 0;
    return WUPSConfigItemMultipleValues::CreateFromValue(identifier, displayName, 0, current, pairs, callback);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

    try {
        auto generalCat = WUPSConfigCategory::Create("General");

        generalCat.add(WUPSConfigItemBoolean::Create("enable4sPower", "Enable 4 second power press",
                                                     true,
                                                     enable4sPower,
                                                     enable4sPowerCallback));

        std::vector<WUPSConfigItemMultipleValues::ValuePair> sourcePairs(3);
        sourcePairs[0].name = "OpenDMCUFirm";
        sourcePairs[0].value = static_cast<uint32_t>(DMCUSource::OpenDMCUFirm);
        sourcePairs[1].name = "Stock";
        sourcePairs[1].value = static_cast<uint32_t>(DMCUSource::Stock);
        sourcePairs[2].name = "SD Card";
        sourcePairs[2].value = static_cast<uint32_t>(DMCUSource::SDCard);
        generalCat.add(WUPSConfigItemMultipleValues::CreateFromValue("dmcuSource", "DMCU Source",
                                                     static_cast<int>(DMCUSource::OpenDMCUFirm), static_cast<int>(dmcuSource),
                                                     sourcePairs, dmcuSourceCallback));

        auto viewportCat = WUPSConfigCategory::Create("DMCU Viewport");

        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportWidth", "TV Viewport Width",
                                                     viewportWidthPresetsTV, dmcuTVViewport.x));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportHeight", "TV Viewport Height",
                                                     viewportHeightPresetsTV, dmcuTVViewport.y));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportWidth576i", "TV Viewport Width (576i)",
                                                     viewportWidthPresetsTV, dmcuTVViewport_576i.x));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportHeight576i", "TV Viewport Height (576i)",
                                                     viewportHeightPresetsTV_576i, dmcuTVViewport_576i.y));

        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportWidth", "DRC Viewport Width",
                                                     viewportWidthPresetsDRC, dmcuDRCViewport.x));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportHeight", "DRC Viewport Height",
                                                     viewportHeightPresetsDRC, dmcuDRCViewport.y));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportWidth576i", "DRC Viewport Width (576i)",
                                                     viewportWidthPresetsDRC, dmcuDRCViewport_576i.x));
        viewportCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportHeight576i", "DRC Viewport Height (576i)",
                                                     viewportHeightPresetsDRC_576i, dmcuDRCViewport_576i.y));

        auto scalerCat = WUPSConfigCategory::Create("DMCU Scaler");

        scalerCat.add(CreateDmcuBoolConfigItem("dmcuTVScalerEnabled", "TV Scaler Enabled",
            dmcuTVScalerOptions.enabled, selectDMCUScalerEnabledCallback));

        scalerCat.add(CreateDmcuBoolConfigItem("dmcuTVScalerNearest", "TV Scaler Nearest",
            dmcuTVScalerOptions.nearest, selectDMCUScalerNearestCallback));

        scalerCat.add(CreateDmcuBoolConfigItem("dmcuDRCScalerEnabled", "DRC Scaler Enabled",
            dmcuDRCScalerOptions.enabled, selectDMCUScalerEnabledCallback));

        scalerCat.add(CreateDmcuBoolConfigItem("dmcuDRCScalerNearest", "DRC Scaler Nearest",
            dmcuDRCScalerOptions.nearest, selectDMCUScalerNearestCallback));

        root.add(std::move(generalCat));
        root.add(std::move(viewportCat));
        root.add(std::move(scalerCat));
    }
    catch (std::exception& e) {
        OSReport("evWii: Creating config menu failed: %s\n", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback()
{
    WUPSStorageAPI::SaveStorage();
}

template<typename ...Args> std::string string_format(const std::string& format, Args ...args)
{
    int size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;

    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);

    return std::string(buf.get(), buf.get() + size - 1);
}

static std::string GenerateCustomDMCUOptionsBlock()
{
    std::string magic = string_format("\n@3500 %02X %02X", CUSTOM_MAGIC >> 8, CUSTOM_MAGIC & 0xff);
    if (!std::strstr((const char*)dmcu_d_hex, magic.c_str())) {
        OSFatal("evWii: Bundled firmware is unsupported");
        return "";
    }

    // Fill OpenDMCUFirm options
    Customizations custom;
    custom.magic = CUSTOM_MAGIC;

    // TV Viewport
    custom.tvViewport.x.start = dmcuTVViewport.x.start;
    custom.tvViewport.x.size = dmcuTVViewport.x.size;
    custom.tvViewport.x.end = dmcuTVViewport.x.end;
    custom.tvViewport.y.start = dmcuTVViewport.y.start;
    custom.tvViewport.y.size = dmcuTVViewport.y.size;
    custom.tvViewport.y.end = dmcuTVViewport.y.end;

    // TV Viewport 576i
    custom.tvViewport_576i.x.start = dmcuTVViewport_576i.x.start;
    custom.tvViewport_576i.x.size = dmcuTVViewport_576i.x.size;
    custom.tvViewport_576i.x.end = dmcuTVViewport_576i.x.end;
    custom.tvViewport_576i.y.start = dmcuTVViewport_576i.y.start;
    custom.tvViewport_576i.y.size = dmcuTVViewport_576i.y.size;
    custom.tvViewport_576i.y.end = dmcuTVViewport_576i.y.end;

    // DRC Viewport
    custom.drcViewport.x.start = dmcuDRCViewport.x.start;
    custom.drcViewport.x.size = dmcuDRCViewport.x.size;
    custom.drcViewport.x.end = dmcuDRCViewport.x.end;
    custom.drcViewport.y.start = dmcuDRCViewport.y.start;
    custom.drcViewport.y.size = dmcuDRCViewport.y.size;
    custom.drcViewport.y.end = dmcuDRCViewport.y.end;

    // DRC Viewport 576i
    custom.drcViewport_576i.x.start = dmcuDRCViewport_576i.x.start;
    custom.drcViewport_576i.x.size = dmcuDRCViewport_576i.x.size;
    custom.drcViewport_576i.x.end = dmcuDRCViewport_576i.x.end;
    custom.drcViewport_576i.y.start = dmcuDRCViewport_576i.y.start;
    custom.drcViewport_576i.y.size = dmcuDRCViewport_576i.y.size;
    custom.drcViewport_576i.y.end = dmcuDRCViewport_576i.y.end;

    // Scaler Options
    custom.tvScaler = dmcuTVScalerOptions;
    custom.drcScaler = dmcuDRCScalerOptions;

    // Write to string
    const uint8_t* optionsBytes = reinterpret_cast<const uint8_t*>(&custom);
    std::string optionsString;
    size_t i = 0;
    size_t total = sizeof(custom);
    while (i < total) {
        const size_t rowEnd = std::min(total, i + 16);

        optionsString += string_format("@%04X", 0x3500 + i);
        for (; i < rowEnd; i++) {
            optionsString += string_format(" %02X", static_cast<unsigned>(optionsBytes[i]));
        }

        optionsString += "\n";
    }

    return optionsString;
}

DECL_FUNCTION(MCPError, MCP_CompatLoadAVFile, int32_t handle, void *ptr, uint32_t *size, MCPCompatAVFile file)
{
    if (file != MCP_COMPAT_AV_FILE_DMCU) {
        return real_MCP_CompatLoadAVFile(handle, ptr, size, file);
    }

    // Check if we should replace the DMCU firmware completely
    if (dmcuSource == DMCUSource::SDCard) {
        FILE* f = fopen("/vol/external01/wiiu/dmcu.d.hex", "rb");
        if (!f) {
            OSFatal("evWii: Cannot load wiiu/dmcu.d.hex from SD Card");
            return -1;
        }

        fseek(f, 0, SEEK_END);
        size_t dmcu_fw_size = ftell(f);
        rewind(f);

        if (ptr) {
            if (fread(ptr, 1, dmcu_fw_size, f) != dmcu_fw_size) {
                fclose(f);
                return -1;
            }
        }

        *size = dmcu_fw_size;

        fclose(f);
        return 0;
    } else if (dmcuSource == DMCUSource::OpenDMCUFirm) {
        if (ptr) {
            memcpy(ptr, dmcu_d_hex, dmcu_d_hex_size);
        }

        *size = dmcu_d_hex_size;

        std::string options = GenerateCustomDMCUOptionsBlock();
        if (!options.empty()) {
            if (ptr) {
                char* buf = (char*) ptr + strlen((char*) ptr);

                // copy options to the end of the AV file
                strcpy(buf, options.c_str());
                buf += options.length();

                // Null terminate
                *buf = '\0';
            }

            *size += options.length();
        }

        return 0;
    }

    return real_MCP_CompatLoadAVFile(handle, ptr, size, file);
}

WUPS_MUST_REPLACE(MCP_CompatLoadAVFile, WUPS_LOADER_LIBRARY_COREINIT, MCP_CompatLoadAVFile);
