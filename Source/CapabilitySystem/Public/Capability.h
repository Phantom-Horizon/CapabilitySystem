#pragma once

#include "CoreMinimal.h"
#include "CapabilityBase.h"
#include "CapabilityBase.inl"
#include "Components/ActorComponent.h"
#include "Capability.generated.h"

UCLASS(Abstract, Blueprintable)
class CAPABILITYSYSTEM_API UCapability : public UCapabilityBase {
    GENERATED_BODY()

public:
    UCapability(const FObjectInitializer& ObjectInitializer);

protected:

    virtual void BeginPlay() override;

    virtual void NativeInitializeCapability();

    virtual void UpdateCapabilityState() override;
};
