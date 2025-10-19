#include "CapabilitySystem/Public/CapabilityBase.h"
#include "CapabilitySystem/Public/CapabilityComponent.h"
#include "CapabilitySystem/Public/CapabilityMetaHead.h"
#include "Net/UnrealNetwork.h"

UCapabilityBase::UCapabilityBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

void UCapabilityBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, TargetCapabilityComponent, COND_InitialOnly, REPNOTIFY_OnChanged);
    DOREPLIFETIME_CONDITION(ThisClass, TargetMetaHead, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(ThisClass, IndexInSet, COND_InitialOnly);
}

AActor* UCapabilityBase::GetOwner() const {
    const auto Comp = Cast<UCapabilityComponent>(GetOuter());
    if (!Comp) return nullptr;
    return Comp->GetOwner();
}

UCapabilityComponent* UCapabilityBase::GetCapabilityComponent() const {
    return Cast<UCapabilityComponent>(GetOuter());
}

FString UCapabilityBase::GetString() {
    return FString::Printf(TEXT("%s [%d]"), *GetName(), IndexInSet);
}

void UCapabilityBase::Activate() {
    if (bIsCapabilityActive) return;
    bIsCapabilityActive = true;
    if (bCanEverTick) bIsTickEnabled = true;
    OnActivated();
}

void UCapabilityBase::Deactivate() {
    if (!bIsCapabilityActive) return;
    bIsCapabilityActive = false;
    bIsTickEnabled = false;
    OnDeactivated();
}

bool UCapabilityBase::IsSideLocalControlled() const {
    AActor* Owner = GetOwner();
    if (!Owner) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::IsSideLocalControlled: Owner is null for capability %s"),
               *GetName());
        return false;
    }

    UWorld* World = Owner->GetWorld();
    if (!World) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::IsSideLocalControlled: World is null for %s - %s"),
               *Owner->GetName(), *GetName());
        return false;
    }

    APawn* PawnOwner = Cast<APawn>(Owner);

    bool bIsLocallyControlled = PawnOwner ? PawnOwner->IsLocallyControlled() : false;

    if (PawnOwner == nullptr) {
        AController* Ct = Cast<AController>(Owner);
        bIsLocallyControlled = Ct ? Ct->IsLocalPlayerController() : false;
    }

    return bIsLocallyControlled;
}

bool UCapabilityBase::IsSideAuthorityOnly() const {
    AActor* Owner = GetOwner();
    if (!Owner) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::IsSideAuthorityOnly: Owner is null for capability %s"),
               *GetName());
        return false;
    }

    return Owner->HasAuthority();
}

bool UCapabilityBase::IsSideAllClients() const {
    AActor* Owner = GetOwner();
    if (!Owner) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::ShouldRunOnThisSide: Owner is null for capability %s"),
               *GetName());
        return false;
    }

    UWorld* World = Owner->GetWorld();
    if (!World) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::ShouldRunOnThisSide: World is null for %s - %s"),
               *Owner->GetName(), *GetName());
        return false;
    }

    const ENetMode NetMode = World->GetNetMode();
    return NetMode != NM_DedicatedServer;
}

UCapabilityDataComponent* UCapabilityBase::GetC(TSubclassOf<UCapabilityDataComponent> ClassOfComponent) const {
    return GetCA(ClassOfComponent, GetOwner());
}

UCapabilityDataComponent* UCapabilityBase::GetCA(TSubclassOf<UCapabilityDataComponent> ClassOfComponent,
                                                 AActor* Target) const {
    if (!Target) return nullptr;
    return Cast<UCapabilityDataComponent>(Target->GetComponentByClass(ClassOfComponent));
}

void UCapabilityBase::BlockCapability(const FName& Tag, UObject* From) {
    if (auto Comp = GetCapabilityComponent())
        Comp->BlockCapability(Tag, From);
}

void UCapabilityBase::UnBlockCapability(const FName& Tag, UObject* From) {
    if (auto Comp = GetCapabilityComponent())
        Comp->UnBlockCapability(Tag, From);
}

void UCapabilityBase::Setup_Implementation() {}

void UCapabilityBase::StartLife_Implementation() {}

void UCapabilityBase::OnActivated_Implementation() {}

bool UCapabilityBase::ShouldActive_Implementation() { return true; }

bool UCapabilityBase::ShouldDeactivate_Implementation() { return false; }

void UCapabilityBase::OnDeactivated_Implementation() {}

void UCapabilityBase::Tick_Implementation(float DeltaTime) {}

void UCapabilityBase::EndCapability_Implementation() {}

void UCapabilityBase::EndLife_Implementation() {}

void UCapabilityBase::NativeBeginPlay() {
    if (bHasBegunPlay) return;

    const auto Comp = GetCapabilityComponent();
    if (!Comp || !Comp->HasBegunPlay()) return;

    bHasBegunPlay = true;
    BeginPlay();
}

void UCapabilityBase::NativePreEndPlay() {
    if (bHasPreEndedPlay) return;
    bHasPreEndedPlay = true;
    if (bIsCapabilityActive) {
        bIsCapabilityActive = false;
        OnDeactivated();
    }
}

void UCapabilityBase::NativeEndPlay() {
    if (bHasEndedPlay) return;
    bHasEndedPlay = true;
    bIsTickEnabled = false;
    if (ShouldRunOnThisSide()) {
        EndCapability();
    }
    EndLife();
    EndPlay();
}

void UCapabilityBase::NativeTick(float DeltaTime) {
    // 当 tickInterval <= 0 表示每帧 Tick
    if (tickInterval <= 0.0f) {
        UpdateCapabilityState();
        if (bIsCapabilityActive) Tick(DeltaTime);
        return;
    }

    tickTimeSum += DeltaTime;
    if (tickTimeSum >= tickInterval) {
        // 使用减法而非清零，降低相位漂移
        tickTimeSum -= tickInterval;
        UpdateCapabilityState();
        if (bIsCapabilityActive) Tick(DeltaTime);
    }
}

void UCapabilityBase::SetEnable(bool bEnable) {
    if (bCanEverTick == bEnable) return;
    bCanEverTick = bEnable;
    if (auto Manager = GetCapabilityComponent()) {
        Manager->NotifyShouldUpdateTickStatusNextFrame();
    }
}

bool UCapabilityBase::ShouldRunOnThisSide() const {
    AActor* Owner = GetOwner();
    if (!Owner) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::ShouldRunOnThisSide: Owner is null for capability %s"),
               *GetName());
        return false;
    }

    UWorld* World = Owner->GetWorld();
    if (!World) {
        UE_LOG(CapabilitySystemLog, Warning, TEXT("UCapabilityBase::ShouldRunOnThisSide: World is null for %s - %s"),
               *Owner->GetName(), *GetName());
        return false;
    }

    switch (executeSide) {
    case ECapabilityExecuteSide::Always:
        return true;
    case ECapabilityExecuteSide::AuthorityOnly:
        return Owner->HasAuthority();

    case ECapabilityExecuteSide::LocalControlledOnly: {
        APawn* PawnOwner = Cast<APawn>(Owner);
        bool bIsLocallyControlled = PawnOwner ? PawnOwner->IsLocallyControlled() : false;
        if (PawnOwner == nullptr) {
            AController* Ct = Cast<AController>(Owner);
            bIsLocallyControlled = Ct ? Ct->IsLocalPlayerController() : false;
        }
        return bIsLocallyControlled;
    }

    case ECapabilityExecuteSide::AuthorityAndLocalControlled: {
        APawn* PawnOwner = Cast<APawn>(Owner);
        bool bIsLocallyControlled = PawnOwner ? PawnOwner->IsLocallyControlled() : false;
        if (PawnOwner == nullptr) {
            AController* Ct = Cast<AController>(Owner);
            bIsLocallyControlled = Ct ? Ct->IsLocalPlayerController() : false;
        }
        return bIsLocallyControlled || Owner->HasAuthority();
    }


    case ECapabilityExecuteSide::OwnerLocalControlledOnly: {
        APawn* PawnOwner = Cast<APawn>(Owner);
        bool bIsOwnerLocallyControlled = PawnOwner ? PawnOwner->IsLocallyControlled() : false;
        if (PawnOwner) return bIsOwnerLocallyControlled;

        if (!bIsOwnerLocallyControlled) {
            AController* Ct = Cast<AController>(Owner);
            bIsOwnerLocallyControlled = Ct ? Ct->IsLocalPlayerController() : false;
            if (Ct) return bIsOwnerLocallyControlled;
        }

        if (!bIsOwnerLocallyControlled) {
            const auto OuterOwner = Cast<APawn>(Owner->GetOwner());

            bIsOwnerLocallyControlled = OuterOwner ? OuterOwner->IsLocallyControlled() : false;
            if (OuterOwner) return bIsOwnerLocallyControlled;
        }

        if (!bIsOwnerLocallyControlled) {
            const auto OuterOwner = Cast<APlayerController>(Owner->GetOwner());
            bIsOwnerLocallyControlled = OuterOwner ? OuterOwner->IsLocalController() : false;
        }
        return bIsOwnerLocallyControlled;
    }
    case ECapabilityExecuteSide::AllClients: {
        return World->GetNetMode() != NM_DedicatedServer;
    }

    default:
        UE_LOG(CapabilitySystemLog, Warning, TEXT("Unknown executeSide for %s - %s"),
               *Owner->GetName(), *GetName());
        return false;
    }
}

void UCapabilityBase::PreDestroyFromReplication() {
    if (!bHasPreEndedPlay) {
        if (TargetMetaHead) {
            TargetMetaHead->CallEndPlay();
        }
    }
}
