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

WUPS_PLUGIN_NAME("evWii");
WUPS_PLUGIN_DESCRIPTION("Patches to enhance the vWii mode");
WUPS_PLUGIN_VERSION("v0.3");
WUPS_PLUGIN_AUTHOR("GaryOderNichts");
WUPS_PLUGIN_LICENSE("GPLv2");

WUPS_USE_STORAGE("evWii");
WUPS_USE_WUT_DEVOPTAB();

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle);
void ConfigMenuClosedCallback();

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

static bool dmcuLoadFromSD = false;

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

#define WUPS_ReadUShortWithDefault(__parent__, __key__, __value__) {{                     \
    int tmp = 0;                                                                          \
    if (WUPSStorageAPI::GetEx(__parent__, __key__, tmp) == WUPS_STORAGE_ERROR_NOT_FOUND){ \
        tmp = __value__;                                                                  \
        WUPSStorageAPI::StoreEx(__parent__, __key__, tmp);                                \
    } else                                                                                \
        __value__ = tmp;                                                                  \
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
    WUPSStorageAPI::GetOrStoreDefault("dmcuLoadFromSD", dmcuLoadFromSD, dmcuLoadFromSD);

    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_xStart", dmcuTVViewport.x.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_yStart", dmcuTVViewport.y.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_xEnd", dmcuTVViewport.x.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_yEnd", dmcuTVViewport.y.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_xSize", dmcuTVViewport.x.size);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV_ySize", dmcuTVViewport.y.size);

    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_xStart", dmcuTVViewport_576i.x.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_yStart", dmcuTVViewport_576i.y.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_xEnd", dmcuTVViewport_576i.x.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_yEnd", dmcuTVViewport_576i.y.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_xSize", dmcuTVViewport_576i.x.size);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuTV576i_ySize", dmcuTVViewport_576i.y.size);

    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_xStart", dmcuDRCViewport.x.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_yStart", dmcuDRCViewport.y.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_xEnd", dmcuDRCViewport.x.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_yEnd", dmcuDRCViewport.y.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_xSize", dmcuDRCViewport.x.size);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC_ySize", dmcuDRCViewport.y.size);

    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_xStart", dmcuDRCViewport_576i.x.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_yStart", dmcuDRCViewport_576i.y.start);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_xEnd", dmcuDRCViewport_576i.x.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_yEnd", dmcuDRCViewport_576i.y.end);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_xSize", dmcuDRCViewport_576i.x.size);
    WUPS_ReadUShortWithDefault(nullptr, "dmcuDRC576i_ySize", dmcuDRCViewport_576i.y.size);

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

void dmcuLoadFromSDCallback(ConfigItemBoolean* item, bool enabled)
{
    dmcuLoadFromSD = enabled;
    WUPSStorageAPI::Store("dmcuLoadFromSD", enabled);
}

void selectDMCUViewportCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    if (i == 0xffff) {
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
        customPair.value = 0xffff;
        customPair.name = "Custom";
    }

    return WUPSConfigItemMultipleValues::CreateFromIndex(identifier, displayName, 0, curIdx, pairs, selectDMCUViewportCallback);
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

        generalCat.add(WUPSConfigItemBoolean::Create("dmcuLoadFromSD", "Load DMCU Firmware from SD Card",
                                                     true,
                                                     dmcuLoadFromSD,
                                                     dmcuLoadFromSDCallback));


        auto dcmuCat = WUPSConfigCategory::Create("DMCU");


        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportWidth", "TV Viewport Width",
                                                     viewportWidthPresetsTV, dmcuTVViewport.x));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportHeight", "TV Viewport Height",
                                                     viewportHeightPresetsTV, dmcuTVViewport.y));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportWidth576i", "TV Viewport Width (576i)",
                                                     viewportWidthPresetsTV, dmcuTVViewport_576i.x));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuTVViewportHeight576i", "TV Viewport Height (576i)",
                                                     viewportHeightPresetsTV_576i, dmcuTVViewport_576i.y));

        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportWidth", "DRC Viewport Width",
                                                     viewportWidthPresetsDRC, dmcuDRCViewport.x));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportHeight", "DRC Viewport Height",
                                                     viewportHeightPresetsDRC, dmcuDRCViewport.y));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportWidth576i", "DRC Viewport Width (576i)",
                                                     viewportWidthPresetsDRC, dmcuDRCViewport_576i.x));
        dcmuCat.add(CreateDmcuViewportAxisConfigItem("dmcuDRCViewportHeight576i", "DRC Viewport Height (576i)",
                                                     viewportHeightPresetsDRC_576i, dmcuDRCViewport_576i.y));

        root.add(std::move(generalCat));
        root.add(std::move(dcmuCat));
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

static std::string GenerateDMCUPatch()
{
#define SPLIT_USHORT(x) x >> 8, x & 0xff
    std::string patch;

    // TODO find a better way to do those viewport patches
    // Maybe replace the setviewport function and check for yEnd to determine 576i or not?

    // TV
    if (dmcuTVViewport.y.start != 0xffff) {
        patch += string_format("@0A0D CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.start));
        patch += string_format("@0ADF 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.start));
        patch += string_format("@1C5D CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.start));
        patch += string_format("@1DDE CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.start));
    }

    if (dmcuTVViewport.x.start != 0xffff) {
        patch += string_format("@0A11 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.start));
        patch += string_format("@0AE5 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.start));
        patch += string_format("@1C61 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.start));
        patch += string_format("@1DE3 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.start));
    }

    if (dmcuTVViewport.y.size != 0xffff) {
        patch += string_format("@0A21 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.size));
        patch += string_format("@0AF3 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.size));
        patch += string_format("@1C71 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.size));
        patch += string_format("@1DF4 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.size));
    }

    if (dmcuTVViewport.x.size != 0xffff) {
        patch += string_format("@0A24 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.size));
        patch += string_format("@0AF6 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.size));
        patch += string_format("@1C74 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.size));
        patch += string_format("@1DF7 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.size));
    }

    if (dmcuTVViewport.y.end != 0xffff) {
        patch += string_format("@0A15 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.end));
        patch += string_format("@0AEB CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.end));
        patch += string_format("@1C65 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.end));
        patch += string_format("@1DE8 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.y.end));
    }

    if (dmcuTVViewport.x.end != 0xffff) {
        patch += string_format("@0A1B 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.end));
        patch += string_format("@0AEF CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.end));
        patch += string_format("@1C6B 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.end));
        patch += string_format("@1DEE 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport.x.end));
    }

    if (dmcuTVViewport_576i.y.start != 0xffff) {
        patch += string_format("@0A9C CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.start));
        patch += string_format("@1CE3 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.start));
        patch += string_format("@1E3E CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.start));
    }

    if (dmcuTVViewport_576i.x.start != 0xffff) {
        patch += string_format("@0AA1 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.start));
        patch += string_format("@1CE7 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.start));
        patch += string_format("@1E42 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.start));
    }

    if (dmcuTVViewport_576i.y.size != 0xffff) {
        patch += string_format("@0AB2 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.size));
        patch += string_format("@1CF7 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.size));
        patch += string_format("@1E50 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.size));
    }

    if (dmcuTVViewport_576i.x.size != 0xffff) {
        patch += string_format("@0AB5 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.size));
        patch += string_format("@1CFA CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.size));
        patch += string_format("@1E53 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.size));
    }

    if (dmcuTVViewport_576i.y.end != 0xffff) {
        patch += string_format("@0AA6 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.end));
        patch += string_format("@1CEB 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.end));
        patch += string_format("@1E46 CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.y.end));
    }

    if (dmcuTVViewport_576i.x.end != 0xffff) {
        patch += string_format("@0AAC 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.end));
        patch += string_format("@1CF1 18 CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.end));
        patch += string_format("@1E4B CC %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.end));
    }

    // DRC
    if (dmcuDRCViewport.y.start != 0xffff) {
        patch += string_format("@0965 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.start));
        patch += string_format("@1C7A CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.start));
        patch += string_format("@1DFD CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.start));
    }

    if (dmcuDRCViewport.x.start != 0xffff) {
        patch += string_format("@0969 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.start));
        patch += string_format("@1C7E CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.start));
        patch += string_format("@1E01 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.start));
    }

    if (dmcuDRCViewport.y.size != 0xffff) {
        patch += string_format("@0977 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.size));
        patch += string_format("@1C8C CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.size));
        patch += string_format("@1E0F CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.size));
    }

    if (dmcuDRCViewport.x.size != 0xffff) {
        patch += string_format("@097A CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.size));
        patch += string_format("@1C8F CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.size));
        patch += string_format("@1E12 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.size));
    }

    if (dmcuDRCViewport.y.end != 0xffff) {
        patch += string_format("@096D CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.end));
        patch += string_format("@1C82 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.end));
        patch += string_format("@1E05 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.y.end));
    }

    if (dmcuDRCViewport.x.end != 0xffff) {
        patch += string_format("@0972 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.end));
        patch += string_format("@1C87 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.end));
        patch += string_format("@1E0A CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport.x.end));
    }

    if (dmcuDRCViewport_576i.y.start != 0xffff) {
        patch += string_format("@094E CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.start));
        patch += string_format("@1D00 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.start));
        patch += string_format("@1E59 18 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.start));
    }

    if (dmcuDRCViewport_576i.x.start != 0xffff) {
        patch += string_format("@0952 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.start));
        patch += string_format("@1D04 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.start));
        patch += string_format("@1E5F 18 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.start));
    }

    if (dmcuDRCViewport_576i.y.size != 0xffff) {
        patch += string_format("@0960 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.size));
        patch += string_format("@1D12 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.size));
        patch += string_format("@1E6D CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.size));
    }

    if (dmcuDRCViewport_576i.x.size != 0xffff) {
        //this conflicts with the non-576i xsize
        //patch += string_format("@097A CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.size));
        patch += string_format("@1D15 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.size));
        patch += string_format("@1E70 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.size));
    }

    if (dmcuDRCViewport_576i.y.end != 0xffff) {
        patch += string_format("@0956 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.end));
        patch += string_format("@1D08 CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.end));
        patch += string_format("@1E65 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.y.end));
    }

    if (dmcuDRCViewport_576i.x.end != 0xffff) {
        patch += string_format("@095B CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.end));
        patch += string_format("@1D0D CC %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.end));
        patch += string_format("@1E69 CE %02X %02X\n", SPLIT_USHORT(dmcuDRCViewport_576i.x.end));
    }

#undef SPLIT_USHORT
    return patch;
}

// #define LOAD_BUNDLED_DMCU_FW
#ifdef LOAD_BUNDLED_DMCU_FW
#include <dmcu_d_hex.h>
#endif

DECL_FUNCTION(MCPError, MCP_CompatLoadAVFile, int32_t handle, void *ptr, uint32_t *size, MCPCompatAVFile file)
{
#ifdef LOAD_BUNDLED_DMCU_FW
    if (file == MCP_COMPAT_AV_FILE_DMCU) {
        if (ptr) {
            memcpy(ptr, dmcu_d_hex, dmcu_d_hex_size);
        }

        *size = dmcu_d_hex_size;
        return 0;
    }
#endif

    // Check if we should replace the DMCU firmware completely
    if (file == MCP_COMPAT_AV_FILE_DMCU && dmcuLoadFromSD) {
        FILE* f = fopen("/vol/external01/wiiu/dmcu.d.hex", "rb");
        if (!f) {
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
    }

    MCPError res = real_MCP_CompatLoadAVFile(handle, ptr, size, file);
    if (res < 0 || file != MCP_COMPAT_AV_FILE_DMCU) {
        return res;
    }

    std::string patch = GenerateDMCUPatch();
    if (!patch.empty()) {
        if (ptr) {
            char* buf = (char*) ptr + strlen((char*) ptr);

            // copy patch to the end of the AV file
            strcpy(buf, patch.c_str());
            buf += patch.length();

            // Null terminate
            *buf = '\0';
        }

        *size += patch.length();
    }

    return res;
}

WUPS_MUST_REPLACE(MCP_CompatLoadAVFile, WUPS_LOADER_LIBRARY_COREINIT, MCP_CompatLoadAVFile);
