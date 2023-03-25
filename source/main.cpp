#include <vector>
#include <string>
#include <memory>
#include <cstring>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>

#include <mocha/mocha.h>

#include <coreinit/mcp.h>

WUPS_PLUGIN_NAME("evWii");
WUPS_PLUGIN_DESCRIPTION("Patches to enhance the vWii mode");
WUPS_PLUGIN_VERSION("v0.1");
WUPS_PLUGIN_AUTHOR("GaryOderNichts");
WUPS_PLUGIN_LICENSE("GPLv2");

WUPS_USE_STORAGE("evWii");

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

#define WUPS_ReadUShortWithDefault(__parent__, __key__, __value__) {{           \
    int tmp = 0;                                                                \
    if (WUPS_GetInt(__parent__, __key__, &tmp) == WUPS_STORAGE_ERROR_NOT_FOUND) \
        WUPS_StoreInt(__parent__, __key__, __value__);                          \
    else                                                                        \
        __value__ = tmp;                                                        \
}}

INITIALIZE_PLUGIN()
{
    if (WUPS_OpenStorage() == WUPS_STORAGE_ERROR_SUCCESS) {
        // Try to get value from storage
        if (WUPS_GetBool(nullptr, "enable4sPower", &enable4sPower) == WUPS_STORAGE_ERROR_NOT_FOUND) {
            // Add the value to the storage if it's missing.
            WUPS_StoreBool(nullptr, "enable4sPower", enable4sPower);
        }

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

        WUPS_CloseStorage();
    }

    if (enable4sPower) {
        Patch_vWii_RTC_CONTROL1();
    } else {
        Restore_vWii_RTC_CONTROL1();
    }
}

void enable4sPowerCallback(ConfigItemBoolean* item, bool enabled)
{
    enable4sPower = enabled;
    WUPS_StoreBool(nullptr, "enable4sPower", enabled);

    if (enable4sPower) {
        Patch_vWii_RTC_CONTROL1();
    } else {
        Restore_vWii_RTC_CONTROL1();
    }
}

void selectDMCUViewportCallback(ConfigItemMultipleValues* item, uint32_t i)
{
    if (i == 0xffff) {
        return;
    }

    if (strcmp(item->configId, "dmcuTVViewportWidth") == 0) {
        dmcuTVViewport.x = viewportWidthPresetsTV[i].second;
    } else if (strcmp(item->configId, "dmcuTVViewportHeight") == 0) {
        dmcuTVViewport.y = viewportHeightPresetsTV[i].second;
    } else if (strcmp(item->configId, "dmcuTVViewportWidth576i") == 0) {
        dmcuTVViewport_576i.x = viewportWidthPresetsTV[i].second;
    } else if (strcmp(item->configId, "dmcuTVViewportHeight576i") == 0) {
        dmcuTVViewport_576i.y = viewportHeightPresetsTV_576i[i].second;
    } else if (strcmp(item->configId, "dmcuDRCViewportWidth") == 0) {
        dmcuDRCViewport.x = viewportWidthPresetsDRC[i].second;
    } else if (strcmp(item->configId, "dmcuDRCViewportHeight") == 0) {
        dmcuDRCViewport.y = viewportHeightPresetsDRC[i].second;
    } else if (strcmp(item->configId, "dmcuDRCViewportWidth576i") == 0) {
        dmcuDRCViewport_576i.x = viewportWidthPresetsDRC[i].second;
    } else if (strcmp(item->configId, "dmcuDRCViewportHeight576i") == 0) {
        dmcuDRCViewport_576i.y = viewportHeightPresetsDRC_576i[i].second;
    }

    WUPS_StoreInt(nullptr, "dmcuTV_xStart", dmcuTVViewport.x.start);
    WUPS_StoreInt(nullptr, "dmcuTV_yStart", dmcuTVViewport.y.start);
    WUPS_StoreInt(nullptr, "dmcuTV_xEnd", dmcuTVViewport.x.end);
    WUPS_StoreInt(nullptr, "dmcuTV_yEnd", dmcuTVViewport.y.end);
    WUPS_StoreInt(nullptr, "dmcuTV_xSize", dmcuTVViewport.x.size);
    WUPS_StoreInt(nullptr, "dmcuTV_ySize", dmcuTVViewport.y.size);

    WUPS_StoreInt(nullptr, "dmcuTV576i_xStart", dmcuTVViewport_576i.x.start);
    WUPS_StoreInt(nullptr, "dmcuTV576i_yStart", dmcuTVViewport_576i.y.start);
    WUPS_StoreInt(nullptr, "dmcuTV576i_xEnd", dmcuTVViewport_576i.x.end);
    WUPS_StoreInt(nullptr, "dmcuTV576i_yEnd", dmcuTVViewport_576i.y.end);
    WUPS_StoreInt(nullptr, "dmcuTV576i_xSize", dmcuTVViewport_576i.x.size);
    WUPS_StoreInt(nullptr, "dmcuTV576i_ySize", dmcuTVViewport_576i.y.size);

    WUPS_StoreInt(nullptr, "dmcuDRC_xStart", dmcuDRCViewport.x.start);
    WUPS_StoreInt(nullptr, "dmcuDRC_yStart", dmcuDRCViewport.y.start);
    WUPS_StoreInt(nullptr, "dmcuDRC_xEnd", dmcuDRCViewport.x.end);
    WUPS_StoreInt(nullptr, "dmcuDRC_yEnd", dmcuDRCViewport.y.end);
    WUPS_StoreInt(nullptr, "dmcuDRC_xSize", dmcuDRCViewport.x.size);
    WUPS_StoreInt(nullptr, "dmcuDRC_ySize", dmcuDRCViewport.y.size);

    WUPS_StoreInt(nullptr, "dmcuDRC576i_xStart", dmcuDRCViewport_576i.x.start);
    WUPS_StoreInt(nullptr, "dmcuDRC576i_yStart", dmcuDRCViewport_576i.y.start);
    WUPS_StoreInt(nullptr, "dmcuDRC576i_xEnd", dmcuDRCViewport_576i.x.end);
    WUPS_StoreInt(nullptr, "dmcuDRC576i_yEnd", dmcuDRCViewport_576i.y.end);
    WUPS_StoreInt(nullptr, "dmcuDRC576i_xSize", dmcuDRCViewport_576i.x.size);
    WUPS_StoreInt(nullptr, "dmcuDRC576i_ySize", dmcuDRCViewport_576i.y.size);
}

#define Config_addDmcuViewportAxisPresets(__config__, __cat__, __configId__, __displayName__, __presets__, __cur__) {{                                          \
    uint32_t numPairs = sizeof(__presets__) / sizeof(__presets__[0]);                                                                                           \
    int curIdx = -1;                                                                                                                                            \
    ConfigItemMultipleValuesPair pairs[numPairs + 1];                                                                                                           \
    for (uint32_t i = 0; i < numPairs; i++) {                                                                                                                   \
        pairs[i].value = i;                                                                                                                                     \
        pairs[i].valueName = (char*) __presets__[i].first;                                                                                                      \
        if (__cur__ == __presets__[i].second)                                                                                                                   \
            curIdx = i;                                                                                                                                         \
    }                                                                                                                                                           \
    if (curIdx == -1) {                                                                                                                                         \
        curIdx = numPairs;                                                                                                                                      \
        pairs[numPairs].value = 0xffff;                                                                                                                         \
        pairs[numPairs].valueName = (char*) "Custom";                                                                                                           \
        numPairs++;                                                                                                                                             \
    }                                                                                                                                                           \
    WUPSConfigItemMultipleValues_AddToCategoryHandled(__config__, __cat__, __configId__, __displayName__, curIdx, pairs, numPairs, selectDMCUViewportCallback); \
}}

WUPS_GET_CONFIG()
{
    if (WUPS_OpenStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        return 0;
    }

    WUPSConfigHandle config;
    WUPSConfig_CreateHandled(&config, "evWii");

    WUPSConfigCategoryHandle catGeneral;
    WUPSConfig_AddCategoryByNameHandled(config, "General", &catGeneral);

    WUPSConfigItemBoolean_AddToCategoryHandled(config, catGeneral, "enable4sPower", "Enable 4 second power press", enable4sPower, enable4sPowerCallback);

    WUPSConfigCategoryHandle catDmcu;
    WUPSConfig_AddCategoryByNameHandled(config, "DMCU", &catDmcu);

    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuTVViewportWidth", "TV Viewport Width", viewportWidthPresetsTV, dmcuTVViewport.x);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuTVViewportHeight", "TV Viewport Height", viewportHeightPresetsTV, dmcuTVViewport.y);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuTVViewportWidth576i", "TV Viewport Width (576i)", viewportWidthPresetsTV, dmcuTVViewport_576i.x);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuTVViewportHeight576i", "TV Viewport Height (576i)", viewportHeightPresetsTV_576i, dmcuTVViewport_576i.y);

    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuDRCViewportWidth", "DRC Viewport Width", viewportWidthPresetsDRC, dmcuDRCViewport.x);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuDRCViewportHeight", "DRC Viewport Height", viewportHeightPresetsDRC, dmcuDRCViewport.y);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuDRCViewportWidth576i", "DRC Viewport Width (576i)", viewportWidthPresetsDRC, dmcuDRCViewport_576i.x);
    Config_addDmcuViewportAxisPresets(config, catDmcu, "dmcuDRCViewportHeight576i", "DRC Viewport Height (576i)", viewportHeightPresetsDRC_576i, dmcuDRCViewport_576i.y);

    return config;
}

WUPS_CONFIG_CLOSED()
{
    WUPS_CloseStorage();
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
        patch += string_format("@1E4B CE %02X %02X\n", SPLIT_USHORT(dmcuTVViewport_576i.x.end));
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

DECL_FUNCTION(MCPError, MCP_CompatLoadAVFile, int32_t handle, void *ptr, uint32_t *size, MCPCompatAVFile file)
{
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
