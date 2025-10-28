#include "CapabilitySystem/Public/Capability.h"

UCapability::UCapability(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
}

void UCapability::BeginPlay() {
    Super::BeginPlay();
    StartLife();

    if (!ShouldRunOnThisSide()) return;

    NativeInitializeCapability();

    if (!ShouldDeactivate())
        Activate();
    else
        Deactivate();
}

void UCapability::NativeInitializeCapability() { Setup(); }

void UCapability::UpdateCapabilityState() {
    if (IsCapabilityActive()) {
        if (ShouldDeactivate()) Deactivate();
    }
    else {
        if (ShouldActive()) Activate();
    }
}