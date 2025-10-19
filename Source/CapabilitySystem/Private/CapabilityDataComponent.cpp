#include "CapabilitySystem/Public/CapabilityDataComponent.h"
#include "CapabilitySystem/Public/CapabilityMetaHead.h"
#include "Net/UnrealNetwork.h"

void UCapabilityDataComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ThisClass, TargetMetaHead, COND_InitialOnly);
}

void UCapabilityDataComponent::PreDestroyFromReplication() {
    if (TargetMetaHead) TargetMetaHead->CallEndPlay();
    Super::PreDestroyFromReplication();
}