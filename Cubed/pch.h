// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
// add headers that you want to pre-compile here
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <thread>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <map>
#include <intrin.h>
#include <sstream>
#include <array>
using namespace std;
#pragma warning(disable : 4099)
#pragma warning(disable : 4309)
#pragma warning(disable : 4369)
#pragma warning(disable : 4244)
#include "SDK/Basic.hpp"
#include "SDK/CoreUObject_structs.hpp"
#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/FortniteGame_structs.hpp"
#include "SDK/FortniteGame_classes.hpp"
#include "SDK/GameplayAbilities_structs.hpp"
#include "SDK/GameplayAbilities_classes.hpp"
#include <set>
using namespace SDK;
using namespace UC;
#include "MinHook.h"
inline uint64_t ImageBase = *(uint64_t*)(__readgsqword(0x60) + 0x10);

// credits to NotTacs for this below
namespace SDK
{
    //Lightweight Log library for the sdk
	class FKismetLogLibrary {
      public:
            FKismetLogLibrary() = default;
	
	  protected:
            std::ofstream* m_stream = nullptr;

      public:
          void SwitchStream( std::ofstream& NewStream );
          void SetNullStream();
		  void Log_Internal(const char* Str, ...);
	};
}

enum class ELogLevel { Fatal, Error, Warning, Log, Verbose, VeryVerbose };

inline constexpr ELogLevel Fatal = ELogLevel::Fatal;
inline constexpr ELogLevel Error = ELogLevel::Error;
inline constexpr ELogLevel Warning = ELogLevel::Warning;
inline constexpr ELogLevel Log = ELogLevel::Log;
inline constexpr ELogLevel Verbose = ELogLevel::Verbose;
inline constexpr ELogLevel VeryVerbose = ELogLevel::VeryVerbose;


inline const char *ToString( ELogLevel level ) {
        switch ( level ) {
        case ELogLevel::Verbose:
                return "Verbose";
        case ELogLevel::VeryVerbose:
                return "VeryVerbose";
        case ELogLevel::Log:
                return "Log";
        case ELogLevel::Warning:
                return "Warning";
        case ELogLevel::Error:
                return "Error";
        case ELogLevel::Fatal:
                return "Fatal";
        default:
                return "Unknown";
        }
}

inline const wchar_t *ToWString( ELogLevel level ) {
        switch ( level ) {
        case ELogLevel::Verbose:
                return L"Verbose";
        case ELogLevel::VeryVerbose:
                return L"VeryVerbose";
        case ELogLevel::Log:
                return L"Log";
        case ELogLevel::Warning:
                return L"Warning";
        case ELogLevel::Error:
                return L"Error";
        case ELogLevel::Fatal:
                return L"Fatal";
        default:
                return L"Unknown";
        }
}

inline void SetConsoleColorByLogLevel( ELogLevel Level ) {
        HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );

        WORD color = FOREGROUND_RED | FOREGROUND_GREEN |
                     FOREGROUND_BLUE; // default white

        switch ( Level ) {
        case ELogLevel::Warning:
                color = FOREGROUND_RED | FOREGROUND_GREEN; // yellow
                break;
        case ELogLevel::Error:
                color = FOREGROUND_RED; // red
                break;
        case ELogLevel::Fatal:
                color = FOREGROUND_RED | FOREGROUND_INTENSITY; // bright red
                break;
        case ELogLevel::Verbose:
                color = FOREGROUND_GREEN | FOREGROUND_BLUE; // cyan
                break;
        case ELogLevel::VeryVerbose:
                color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; // bright blue
                break;
        case ELogLevel::Log:
        default:
                color = FOREGROUND_RED | FOREGROUND_GREEN |
                        FOREGROUND_BLUE; // white
                break;
        }

        SetConsoleTextAttribute( hConsole, color );
}

inline void ResetConsoleColor() {
        SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ),
                                 FOREGROUND_RED | FOREGROUND_GREEN |
                                     FOREGROUND_BLUE );
}


struct FLogCategory {
        const char *Name;
        ELogLevel RuntimeVerbosity;
};


#if _DEBUG
#define DEFINE_LOG_CATEGORY( CategoryName )                                    \
        inline FLogCategory CategoryName = { #CategoryName, ELogLevel::VeryVerbose }
#else
#define DEFINE_LOG_CATEGORY( CategoryName )                                    \
        inline FLogCategory CategoryName = { #CategoryName, ELogLevel::Log }
#endif

inline void UELogImpl(FLogCategory& Category, ELogLevel Level, const char* msg) {
        if ((int)Level > (int)Category.RuntimeVerbosity)
                return;

        SetConsoleColorByLogLevel(Level);
        std::cout << Category.Name << ": "
                  << ToString(Level) << ": " << msg << std::endl;
        ResetConsoleColor();
}

inline std::string WStringToUTF8(const std::wstring& wstr) {
        return std::string(wstr.begin(), wstr.end());
}

inline void UELogWImpl(FLogCategory& Category, ELogLevel Level, const wchar_t* msg) {
        if ((int)Level > (int)Category.RuntimeVerbosity)
                return;

        std::wstring wfinal(msg);
        std::string finalbuf = WStringToUTF8(wfinal);

        SetConsoleColorByLogLevel(Level);
        std::cout << Category.Name << ": "
                  << ToString(Level) << ": " << finalbuf << std::endl;
        ResetConsoleColor();
}

inline void UE_LOG_DISPATCH(FLogCategory& Category, ELogLevel Level, const char* Format, ...) {
        va_list args;
        va_start(args, Format);

        char buffer[2048];
        vsnprintf(buffer, sizeof(buffer), Format, args);
        va_end(args);

        UELogImpl(Category, Level, buffer);
}

inline void UE_LOG_DISPATCH(FLogCategory& Category, ELogLevel Level, const wchar_t* Format, ...) {
        va_list args;
        va_start(args, Format);

        wchar_t buffer[2048];
        vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), Format, args);
        va_end(args);

        UELogWImpl(Category, Level, buffer);
}

#define UE_LOG(Category, Verbosity, Format, ...) \
UE_LOG_DISPATCH(Category, Verbosity, Format, ##__VA_ARGS__)

#define UE_LOG_W(Category, Verbosity, Format, ...) \
UE_LOG_DISPATCH(Category, Verbosity, Format, ##__VA_ARGS__)

#define DefineOriginal(_Rt, _Name, ...) static inline _Rt (*_Name##OG)(##__VA_ARGS__); static _Rt _Name(##__VA_ARGS__);
static int RetTrue() { return 1; }
static int RetFalse() { return 0; }

template <class X>
using xset = set<X, TMemoryAllocator<X>>;
template <class X>
using xvector = vector<X, TMemoryAllocator<X>>;
template <class X, class Y>
using xmap = map<X, Y, less<X>, TMemoryAllocator<pair<const X, Y>>>;

template<typename T>
        __forceinline std::vector<T*> GetObjectsOfClass()
{
        std::vector<T*> Objects;
        for (int i = 0; i < UObject::GObjects->Num(); i++)
        {
                T* Object = reinterpret_cast<T*>(UObject::GObjects->GetByIndex(i));
                if (!Object) continue;
                if (Object->IsA(T::StaticClass()))
                {
                        Objects.push_back(Object);
                }
        }
        return Objects;
}

inline bool IsValidPointer(void* ptr) {
        return ptr != nullptr && 
               ptr != (void*)0xFFFFFFFFULL && 
               ptr != (void*)0xCCCCCCCCULL && 
               ptr != (void*)0xDDDDDDDDULL &&
               (uintptr_t)ptr > 0x10000;
}

template <class X>
using xset = set<X, TMemoryAllocator<X>>;
template <class X>
using xvector = vector<X, TMemoryAllocator<X>>;
template <class X, class Y>
using xmap = map<X, Y, less<X>, TMemoryAllocator<pair<const X, Y>>>;

inline UWorld* GetWorld()
{
        return UWorld::GetWorld();
}

inline AFortGameModeAthena* GetGameMode()
{
        if (auto World = GetWorld())
        {
                return (AFortGameModeAthena*)World->AuthorityGameMode;
        }
        return nullptr;
}

inline AFortGameStateAthena* GetGameState()
{
        if (auto World = GetWorld())
        {
                return (AFortGameStateAthena*)World->GameState;
        }
        return nullptr;
}

#endif //PCH_H
