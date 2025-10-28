// Copyright ysion(LZY). All Rights Reserved.
#pragma once

#include "CapabilityBase.h"
#include "CapabilityComponent.h"

template <typename T>
T* UCapabilityBase::GetOwner() const {
    const auto Manager = Cast<UCapabilityComponent>(GetOuter());
    if (!Manager) return nullptr;
    return Cast<T>(Manager->GetOwner());
}
