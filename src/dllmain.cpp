#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

#include "SDK/Engine_classes.hpp"
#include "SDK/BP_HUD_classes.hpp"

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "MandragoraFix";
std::string sFixVersion = "0.0.1";
std::filesystem::path sFixPath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

// Aspect ratio / FOV / HUD
std::pair DesktopDimensions = { 0,0 };
const float fPi = 3.1415926535f;
const float fNativeAspect = 16.00f / 9.00f;
float fAspectRatio;
float fAspectMultiplier;
float fHUDWidth;
float fHUDWidthOffset;
float fHUDHeight;
float fHUDHeightOffset;

// Ini variables
bool bFixAspect;
bool bFixFOV;
bool bSpanHUD;
float fSpanHUDAspect;

// Variables
int iCurrentResX;
int iCurrentResY;
SDK::UEngine* Engine = nullptr;

void CalculateAspectRatio(bool bLog)
{
    if (iCurrentResX <= 0 || iCurrentResY <= 0)
        return;

    // Calculate aspect ratio
    fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    // HUD 
    fHUDWidth = (float)iCurrentResY * fNativeAspect;
    fHUDHeight = (float)iCurrentResY;
    fHUDWidthOffset = (float)(iCurrentResX - fHUDWidth) / 2.00f;
    fHUDHeightOffset = 0.00f;
    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = (float)iCurrentResX;
        fHUDHeight = (float)iCurrentResX / fNativeAspect;
        fHUDWidthOffset = 0.00f;
        fHUDHeightOffset = (float)(iCurrentResY - fHUDHeight) / 2.00f;
    }

    // Log details about current resolution
    if (bLog) {
        spdlog::info("----------");
        spdlog::info("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
        spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
        spdlog::info("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        spdlog::info("Current Resolution: fHUDWidth: {}", fHUDWidth);
        spdlog::info("Current Resolution: fHUDHeight: {}", fHUDHeight);
        spdlog::info("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        spdlog::info("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        spdlog::info("----------");
    }
}

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[_MAX_PATH] = {0};
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    sFixPath = dllPath;
    sFixPath = sFixPath.remove_filename();

    // Get game name and exe path
    WCHAR exePath[_MAX_PATH] = {0};
    GetModuleFileNameW(exeModule, exePath, MAX_PATH);
    sExePath = exePath;
    sExeName = sExePath.filename().string();
    sExePath = sExePath.remove_filename();

    // Spdlog initialisation
    try
    {
        // Truncate existing log file
        std::ofstream file(sExePath.string() + sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();

        // Create single log file that's size-limited to 10MB
        logger = std::make_shared<spdlog::logger>(sFixName, std::make_shared<spdlog::sinks::rotating_file_sink_st>(sExePath.string() + sLogFile, 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::debug);

        spdlog::info("----------");
        spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
        spdlog::info("----------");
        spdlog::info("Log file: {}", sFixPath.string() + sLogFile);
        spdlog::info("----------");
        spdlog::info("Module Name: {:s}", sExeName);
        spdlog::info("Module Path: {:s}", sExePath.string());
        spdlog::info("Module Address: 0x{:x}", (uintptr_t)exeModule);
        spdlog::info("Module Timestamp: {:d}", Memory::ModuleTimestamp(exeModule));
        spdlog::info("----------");
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        AllocConsole();
        FILE *dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "Log initialisation failed: " << ex.what() << std::endl;
        FreeLibraryAndExitThread(thisModule, 1);
    }
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(sFixPath / sConfigFile);
    if (!iniFile)
    {
        AllocConsole();
        FILE *dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << "" << sFixName.c_str() << " v" << sFixVersion.c_str() << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sFixPath.string().c_str() << std::endl;
        spdlog::error("ERROR: Could not locate config file {}", sConfigFile);
        spdlog::shutdown();
        FreeLibraryAndExitThread(thisModule, 1);
    }
    else
    {
        spdlog::info("Config file: {}", sFixPath.string() + sConfigFile);
        ini.parse(iniFile);
    }

    // Parse config
    ini.strip_trailing_comments();
    spdlog::info("----------");

    // Load settings from ini
    inipp::get_value(ini.sections["Fix Aspect Ratio"], "Enabled", bFixAspect);
    inipp::get_value(ini.sections["Fix FOV"], "Enabled", bFixFOV);
    inipp::get_value(ini.sections["Span HUD"], "Enabled", bSpanHUD);
    inipp::get_value(ini.sections["Span HUD"], "AspectRatio", fSpanHUDAspect);
    
    // Clamp settings
    fSpanHUDAspect = std::clamp(fSpanHUDAspect, 0.00f, 10.00f);

    // Log ini parse
    spdlog_confparse(bFixAspect);
    spdlog_confparse(bFixFOV);
    spdlog_confparse(bSpanHUD);
    spdlog_confparse(fSpanHUDAspect);

    spdlog::info("----------");
}

void UpdateOffsets()
{
    // GObjects
    std::uint8_t* GObjectsScanResult = Memory::PatternScan(exeModule, "48 8B ?? ?? ?? ?? ?? 48 8B ?? ?? 48 8D ?? ?? EB ?? 33 ??");
    if (GObjectsScanResult) {
        spdlog::info("Offsets: GObjects: Address is {:s}+{:x}", sExeName.c_str(), GObjectsScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        std::uint8_t* GObjectsAddr = Memory::GetAbsolute(GObjectsScanResult + 0x3);
        SDK::Offsets::GObjects = static_cast<UC::uint32>(GObjectsAddr - reinterpret_cast<std::uint8_t*>(exeModule));
        spdlog::info("Offsets: GObjects: {:x}", SDK::Offsets::GObjects);
    }
    else {
        spdlog::error("Offsets: GObjects: Pattern scan failed.");
    }

    // AppendString
    std::uint8_t* AppendStringScanResult = Memory::PatternScan(exeModule, "48 8D ?? ?? ?? 48 8B ?? 48 89 ?? ?? ?? E8 ?? ?? ?? ?? 48 8B ?? ?? 48 85 ?? 74 ?? E8 ?? ?? ?? ?? 48 8B ?? ?? 4C ?? ?? ??");
    if (AppendStringScanResult) {
        spdlog::info("Offsets: AppendString: Address is {:s}+{:x}", sExeName.c_str(), AppendStringScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        std::uint8_t* AppendStringAddr = Memory::GetAbsolute(AppendStringScanResult + 0xE);
        SDK::Offsets::AppendString = static_cast<UC::uint32>(AppendStringAddr - reinterpret_cast<std::uint8_t*>(exeModule));
        spdlog::info("Offsets: AppendString: 0x{:x}", SDK::Offsets::AppendString);
    }
    else {
        spdlog::error("Offsets: AppendString: Pattern scan failed.");
    }

    // ProcessEvent
    std::uint8_t* ProcessEventScanResult = Memory::PatternScan(exeModule, "40 ?? ?? ?? 41 ?? 41 ?? 41 ?? 41 ?? 48 81 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? 48 89 ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? 48 33 ?? 48 89 ?? ?? ?? ?? ?? 8B ?? ??");
    if (ProcessEventScanResult) {
        spdlog::info("Offsets: ProcessEvent: Address is {:s}+{:x}", sExeName.c_str(), ProcessEventScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        SDK::Offsets::ProcessEvent = static_cast<UC::uint32>(ProcessEventScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        spdlog::info("Offsets: ProcessEvent: 0x{:x}", SDK::Offsets::ProcessEvent);
    }
    else {
        spdlog::error("Offsets: ProcessEvent: Pattern scan failed.");
    }

    spdlog::info("----------");
}

void CurrentResolution()
{
    // Current resolution
    std::uint8_t* CurrentResolutionScanResult = Memory::PatternScan(exeModule, "44 89 ?? ?? ?? ?? ?? 48 8D ?? ?? 44 89 ?? ?? ?? ?? ?? 4D ?? ?? 89 ?? ?? ?? ?? ?? 48 8B ?? 48 ?? ?? ?? E8 ?? ?? ?? ??");
    if (CurrentResolutionScanResult) {
        spdlog::info("Current Resolution: Address is {:s}+{:x}", sExeName.c_str(), CurrentResolutionScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                // Get current resolution
                int iResX = static_cast<int>(ctx.r12);
                int iResY = static_cast<int>(ctx.r15);
  
                // Log current resolution
                if (iCurrentResX != iResX || iCurrentResY != iResY) {
                    iCurrentResX = iResX;
                    iCurrentResY = iResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else {
        spdlog::error("Current Resolution: Pattern scan failed.");
    }
}

void AspectRatioFOV()
{
    if (bFixAspect || bFixFOV) 
    {
        // Aspect ratio / FOV
        std::uint8_t* AspectRatioFOVScanResult = Memory::PatternScan(exeModule, "F3 0F ?? ?? ?? 8B ?? ?? ?? ?? ?? 89 ?? ?? 0F ?? ?? ?? ?? ?? ?? 33 ?? ?? 83 ?? 01 31 ?? ??");
        if (AspectRatioFOVScanResult) {
            spdlog::info("Aspect Ratio/FOV: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioFOVScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid FOVMidHook{};
            FOVMidHook = safetyhook::create_mid(AspectRatioFOVScanResult,
                [](SafetyHookContext& ctx) {
                    // Fix cropped FOV when wider than 16:9
                    if (bFixAspect && fAspectRatio > fNativeAspect)
                        ctx.xmm0.f32[0] = atanf(tanf(ctx.xmm0.f32[0] * (fPi / 360)) / fNativeAspect * fAspectRatio) * (360 / fPi);
                });

            static SafetyHookMid AspectRatioMidHook{};
            AspectRatioMidHook = safetyhook::create_mid(AspectRatioFOVScanResult + 0xB,
                [](SafetyHookContext& ctx) {
                    if (bFixAspect)
                        ctx.rax = std::bit_cast<uint32_t>(fAspectRatio);
                });
        }
        else {
            spdlog::error("Aspect Ratio / FOV: Pattern scan failed.");
        }
    }
}

void HUD()
{
    if (bSpanHUD) 
    {
        // GameplayHUD
        std::uint8_t* GameplayHUDScanResult = Memory::PatternScan(exeModule, "E8 ?? ?? ?? ?? 48 8B ?? ?? 33 ?? 44 0F ?? ?? ?? ?? 48 85 ?? 0F ?? ?? ?? ?? 0F 95 ?? 48 ?? ?? 48 89 ?? ?? 48 8B ?? 48 8B ?? FF 90 ?? ?? ?? ??");
        if (GameplayHUDScanResult) {
            spdlog::info("HUD: Gameplay HUD: Address is {:s}+{:x}", sExeName.c_str(), GameplayHUDScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            
            static SDK::UBP_HUD_C* BP_HUD = nullptr;
            static SDK::UObject* Object = nullptr;
            static SDK::UObject* OldObject = nullptr;
            static std::string ObjectName;
            
            static SafetyHookMid GameplayHUDMidHook{};
            GameplayHUDMidHook = safetyhook::create_mid(GameplayHUDScanResult + 0x5,
                [](SafetyHookContext& ctx) {
                    if (!ctx.rdi) return;

                    Object = reinterpret_cast<SDK::UObject*>(ctx.rdi);

                    // Check if UObject has changed
                    if (Object != OldObject) {
                        OldObject = Object;

                        // Store name of UObject
                        ObjectName = Object->GetName();

                        // Get address of "BP_HUD_C"
                        if (ObjectName.contains("BP_HUD_C") && BP_HUD != Object) {
                            #ifdef _DEBUG
                            spdlog::info("HUD: Widgets: BP_HUD_C: {}", ObjectName);
                            spdlog::info("HUD: Widgets: BP_HUD_C: Address: {:x}", (uintptr_t)Object);
                            #endif
                            
                            // Store address of "BP_HUD_C"
                            BP_HUD = static_cast<SDK::UBP_HUD_C*>(Object);
                        }

                        // Adjust HUD
                        if (BP_HUD && Object == BP_HUD) {
                            // Get scalebox and sizebox
                            auto FullscreenScaleBox = static_cast<SDK::UFullScreenScaleBox*>(BP_HUD->WidgetTree->RootWidget);
                            auto SizeBox = static_cast<SDK::USizeBox*>(FullscreenScaleBox->Slots[0]->Content);

                            // Span HUD
                            if (fSpanHUDAspect != 0.00f) {
                                // User-defined
                                if (fSpanHUDAspect > fNativeAspect) {
                                    SizeBox->SetWidthOverride(1080.00f * fSpanHUDAspect);
                                    SizeBox->SetHeightOverride(1080.00f);
                                }
                                else if (fSpanHUDAspect < fNativeAspect) {
                                    SizeBox->SetWidthOverride(1920.00f);
                                    SizeBox->SetHeightOverride(1920.00f / fSpanHUDAspect);
                                }
                            }
                            else {
                                // Automatic (full span)
                                if (fAspectRatio > fNativeAspect) {
                                    SizeBox->SetWidthOverride(1920.00f * fAspectMultiplier);
                                    SizeBox->SetHeightOverride(1080.00f);
                                }
                                else if (fAspectRatio < fNativeAspect) {
                                    SizeBox->SetWidthOverride(1920.00f);
                                    SizeBox->SetHeightOverride(1080.00f / fAspectMultiplier);
                                }
                            }
                        }
                    }
                });
        }
        else {
            spdlog::error("HUD: Gameplay HUD: Pattern scan failed.");
        }
    }
}

void EnableConsole()
{ 
    #ifdef _DEBUG
    // Get GEngine
    for (int i = 0; i < 200; ++i) { // 20s
        Engine = SDK::UEngine::GetEngine();

        if (Engine && Engine->ConsoleClass && Engine->GameViewport)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!Engine || !Engine->ConsoleClass || !Engine->GameViewport) {
        spdlog::error("Enable Console: Failed to find GEngine address after 20 seconds.");
        return;
    }

    spdlog::info("Enable Console: GEngine address = {:x}", (uintptr_t)Engine);

    // Construct console
    SDK::UObject* NewObject = SDK::UGameplayStatics::SpawnObject(Engine->ConsoleClass, Engine->GameViewport);
    if (NewObject) {
        Engine->GameViewport->ViewportConsole = static_cast<SDK::UConsole*>(NewObject);
        spdlog::info("Enable Console: Console object constructed.");
    }
    else {
        spdlog::error("Enable Console: Failed to construct console object.");
        return;
    }

    // Get input settings
    SDK::UInputSettings* InputSettings = SDK::UInputSettings::GetDefaultObj();

    // Set keybind
    InputSettings->ConsoleKeys[0].KeyName = SDK::UKismetStringLibrary::Conv_StringToName(L"Tilde");

    if (InputSettings) {
        if (InputSettings->ConsoleKeys && InputSettings->ConsoleKeys.Num() > 0) {
            spdlog::info("Enable Console: Console enabled - access it using key: {}.", InputSettings->ConsoleKeys[0].KeyName.ToString().c_str());
        }
    }
    else {
        spdlog::error("Enable Console: Failed to retreive input settings.");
    }
    #endif
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();
    UpdateOffsets();
    CurrentResolution();
    AspectRatioFOV();
    HUD();
    EnableConsole();

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
