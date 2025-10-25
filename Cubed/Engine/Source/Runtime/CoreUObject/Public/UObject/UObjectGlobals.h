#pragma once
#include "pch.h"

#include "Globals.h"
static auto fn = (SDK::UObject* (*)(SDK::UClass*, SDK::UObject*, const TCHAR*, const TCHAR*, uint32_t, SDK::UObject*, bool, void*)) (uint64_t(ImageBase + 0x14ad4a0));

template <typename T>
inline T* StaticFindObject(const std::string& Path, UClass* Class = UObject::StaticClass()) {
    static auto fn2 = reinterpret_cast<
        SDK::UObject* (*)(SDK::UClass*, SDK::UObject*, const wchar_t*, bool)
    >(
        ImageBase + 0xd13c3c
    );
    if (!fn2) UE_LOG(LogTemp, Fatal, "Failed to find StaticFindObject");
    
    SDK::UObject* obj = fn2 ? fn2(Class, nullptr, std::wstring(Path.begin(), Path.end()).c_str(), false) : nullptr;
    return reinterpret_cast<T*>(obj);
}

template<typename T>
T* ForceStaticLoadObject(xstring name) {
    T* Object = nullptr;
    if (!Object) {
        auto Name = xwstring(name.begin(), name.end()).c_str();
        UObject* BaseObject = fn(T::StaticClass(), nullptr, Name, nullptr, 0, nullptr, false, nullptr);
        Object = static_cast<T*>(BaseObject);
    }
    return Object;
}

template<typename T>
T* StaticLoadObject(xstring name) {
    T* Object = StaticFindObject<T>(std::string(name));
    if (!Object) {
        auto Name = xwstring(name.begin(), name.end()).c_str();
        UObject* BaseObject = fn(T::StaticClass(), nullptr, Name, nullptr, 0, nullptr, false, nullptr);
        Object = static_cast<T*>(BaseObject);
    }
    return Object;
}

template<typename T>
T* StaticLoadObject(const std::string& name) {
    T* Object = StaticFindObject<T>(name);
    if (!Object) {
        auto Name = std::wstring(name.begin(), name.end()).c_str();
        UObject* BaseObject = fn(T::StaticClass(), nullptr, Name, nullptr, 0, nullptr, false, nullptr);
        Object = static_cast<T*>(BaseObject);
    }
    return Object;
}

template<typename T>
T* StaticLoadObject(const char* name) {
    return StaticLoadObject<T>(std::string(name));
}

template<typename T>
T* StaticLoadObject(const std::string& name, UClass* ClassToLoad) {
    T* Object = StaticFindObject<T>(name);
    if (!Object) {
        auto Name = std::wstring(name.begin(), name.end()).c_str();
        UObject* BaseObject = fn(T::StaticClass(), nullptr, Name, nullptr, 0, nullptr, false, nullptr);
        Object = static_cast<T*>(BaseObject);
    }
    return Object;
}

template<typename T>
T* StaticLoadObject(const char* name, UClass* ClassToLoad) {
    return StaticLoadObject<T>(std::string(name), ClassToLoad);
}

inline UObject* StaticLoadObjectNoType(const std::string& name, UClass* ClassToLoad) {
    UObject* Object = StaticFindObject<UObject>(name);
    if (!Object) {
        auto Name = std::wstring(name.begin(), name.end()).c_str();
        UObject* BaseObject = fn(ClassToLoad, nullptr, Name, nullptr, 0, nullptr, false, nullptr);
        Object = BaseObject;
    }
    return Object;
}

inline UObject* StaticLoadObjectNoType(const char* name, UClass* ClassToLoad) {
    return StaticLoadObjectNoType(std::string(name), ClassToLoad);
}
