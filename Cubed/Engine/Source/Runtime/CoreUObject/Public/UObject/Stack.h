#pragma once

#include "pch.h"

#include "Offsets.h"

class FOutputDevice
{
public:
    bool bSuppressEventTag;
    bool bAutoEmitLineTerminator;
    uint8_t _Padding1[0x6];
};

class FFrame : public FOutputDevice
{
public:
    void** VTable;
    UFunction* Node;
    UObject* Object;
    uint8* Code;
    uint8* Locals;
    void* MostRecentProperty;
    uint8_t* MostRecentPropertyAddress;
    uint8_t _Padding1[0x40];
    FField* PropertyChainForCompiledIn;

public:
    static inline auto Step = reinterpret_cast<void(*)(FFrame*, SDK::UObject*, void* const)>(ImageBase + Runtime::Offsets::Step);
    static inline auto StepExplicitProperty = reinterpret_cast<void(*)(FFrame*, void* const, SDK::FField*)>(ImageBase + Runtime::Offsets::StepExplicitProperty);

    void StepCompiledIn(void* const Result, bool ForceExplicitProp = false) {
        if (Code && !ForceExplicitProp)
        {
            Step(this, Object, Result);
        }
        else
        {
            FField* _Prop = PropertyChainForCompiledIn;
            PropertyChainForCompiledIn = _Prop->Next;
            StepExplicitProperty(this, Result, _Prop);
        }
    }

    template <typename T>
    T& StepCompiledInRef()
    {
        T TempVal{};
        MostRecentPropertyAddress = nullptr;

        if (Code)
        {
            Step(this, Object, &TempVal);
        }
        else
        {
            FField* _Prop = PropertyChainForCompiledIn;
            PropertyChainForCompiledIn = _Prop->Next;
            StepExplicitProperty(this, &TempVal, _Prop);
        }

        return MostRecentPropertyAddress ? *(T*)MostRecentPropertyAddress : TempVal;
    }

    void IncrementCode() {
        Code = (uint8_t*)(__int64(Code) + (bool)Code);
    }
};