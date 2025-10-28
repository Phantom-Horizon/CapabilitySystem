// Copyright ysion(LZY). All Rights Reserved.

#pragma once
#include "CapabilitySystem/Public/Capability.h"
#include "CapabilitySystem/Public/CapabilityInput.h"
#include "CapabilitySystem/Public/CapabilityComponent.h"
#include "Modules/ModuleManager.h"

class FCapabilitySystemModule : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};