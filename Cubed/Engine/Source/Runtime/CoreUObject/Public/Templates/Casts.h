#pragma once

#include "pch.h"

template<class T>
inline T* Cast(UObject* Object)
{
    return Object && (Object->IsA(T::StaticClass())) ? (T*)Object : nullptr;
}