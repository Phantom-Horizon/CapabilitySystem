#include "CapabilityMetaHead.h"

#include "CapabilityBase.h"
#include "CapabilityComponent.h"
#include "Net/UnrealNetwork.h"

void UCapabilityMetaHead::CallEndPlay() {
    if (bHasEndPlayCalled) return;
    bHasEndPlayCalled = true;

    for (int i = CapabilityList.Num() - 1; i >= 0; i--) {
        if (CapabilityList[i].IsValid()) CapabilityList[i]->NativePreEndPlay();
    }

    for (int i = CapabilityList.Num() - 1; i >= 0; i--) {
        if (CapabilityList[i].IsValid()) CapabilityList[i]->NativeEndPlay();
    }

    for (int i = CapabilityList.Num() - 1; i >= 0; i--) {
        if (CapabilityList[i].IsValid()) CapabilityList[i]->MarkAsGarbage();
    }
}

AActor* UCapabilityMetaHead::GetOwner() const {
    const auto Comp = Cast<UCapabilityComponent>(GetOuter());
    if (!Comp) return nullptr;
    return Comp->GetOwner();
}

void UCapabilityMetaHead::PreDestroyFromReplication() {
    CallEndPlay();
}

void UCapabilityMetaHead::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, CapabilityList, COND_InitialOnly, REPNOTIFY_OnChanged);
}
