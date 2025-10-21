#include "CapabilitySystem/Public/CapabilityComponent.h"
#include "CapabilitySystem/Public/CapabilityAsset.h"
#include "CapabilitySystem/Public/CapabilityInput.h"
#include "CapabilitySystem/Public/CapabilityMetaHead.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameFramework/PlayerController.h"

UCapabilityComponent::UCapabilityComponent() {
    SetIsReplicatedByDefault(true);
    bReplicateUsingRegisteredSubObjectList = true;
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCapabilityComponent::AddCapabilitySetCollection(TSoftObjectPtr<UCapabilitySetCollection> TargetCollection) {
    if (bIsShuttingDown) return;
    auto Ptr = TargetCollection.LoadSynchronous();
    if (!Ptr) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::AddCapabilitySetCollection Failed to Load CapabilitySetCollection Class at \"{0}\" - \"{1}\"",
                  GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    int idx = 0;
    bool success = true;
    for (auto& CapabilitySet : Ptr->CapabilitySets) {
        auto SetPtr = CapabilitySet.LoadSynchronous();
        if (!SetPtr) {
            UE_LOGFMT(CapabilitySystemLog, Warning,
                      "UCapabilityComponent::AddCapabilitySetCollection Failed to Load CapabilitySet in Collection at \"{0}\" - \"{1}\", Collection \"{2}\" At [{3}]",
                      GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"), TargetCollection->GetName(), idx);
            success = false;
        }

        idx++;
    }

    if (success) {
        for (auto& CapabilitySet : Ptr->CapabilitySets) {
            AddCapabilitySet(CapabilitySet);
        }
    }
}

void UCapabilityComponent::RemoveCapabilitySetCollection(TSoftObjectPtr<UCapabilitySetCollection> TargetCollection) {
    if (bIsShuttingDown) return;
    auto Ptr = TargetCollection.LoadSynchronous();
    if (!Ptr) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::RemoveCapabilitySetCollection Failed to Load CapabilitySetCollection Class at \"{0}\" - \"{1}\"",
                  GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    int idx = 0;
    bool success = true;
    for (auto& CapabilitySet : Ptr->CapabilitySets) {
        auto SetPtr = CapabilitySet.LoadSynchronous();
        if (!SetPtr) {
            UE_LOGFMT(CapabilitySystemLog, Warning,
                      "UCapabilityComponent::RemoveCapabilitySetCollection Failed to Load CapabilitySet in Collection at \"{0}\" - \"{1}\", Collection \"{2}\" At [{3}]",
                      GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"), TargetCollection->GetName(), idx);
            success = false;
        }

        idx++;
    }

    if (success) {
        bool ShouldRemove = true;
        for (auto& CapabilitySet : Ptr->CapabilitySets) {
            if (!IsCapabilitySetExist(CapabilitySet)) {
                UE_LOGFMT(CapabilitySystemLog, Warning,
                          "UCapabilityComponent::RemoveCapabilitySetCollection The collection in the activity capability queue is incomplete; stop removal. at \"{0}\" - \"{1}\", Collection \"{2}\", NotFind CapabilitySet \"{3}\"",
                          GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"), TargetCollection->GetName(), CapabilitySet->GetName());

                ShouldRemove = false;
            }
        }

        if (ShouldRemove) {
            for (auto& CapabilitySet : Ptr->CapabilitySets) {
                RemoveCapabilitySet(CapabilitySet);
            }
        }
    }
}

void UCapabilityComponent::BeginPlay() {
    Super::BeginPlay();

    if (ComponentMode == ECapabilityComponentMode::Local) {
        // 本地模式：直接在当前端加载预设与集合（不依赖服务器）
        for (auto& Collection : CapabilitySetCollection) {
            AddCapabilitySetCollection(Collection);
        }
        for (auto& CapabilitySet : CapabilitySetPresets) {
            AddCapabilitySet(CapabilitySet);
        }
        UpdateTickStatus();
        return;
    }

    // Authority 模式：保持原有逻辑（仅服务器创建并同步）
    if (GetOwner() && GetOwner()->HasAuthority()) {
        for (auto& CapabilitySet : CapabilitySetCollection) {
            AddCapabilitySetCollection(CapabilitySet);
        }
        for (auto& CapabilitySet : CapabilitySetPresets) {
            AddCapabilitySet(CapabilitySet);
        }
    }
}

void UCapabilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    bIsShuttingDown = true;
    if (ComponentMode == ECapabilityComponentMode::Local) {
        RemoveAllCapabilitySet();
    } else {
        if (GetOwner() && GetOwner()->HasAuthority()) {
            RemoveAllCapabilitySet();
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UCapabilityComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction) {
    SCOPE_CYCLE_COUNTER(STAT_Capability_Tick)
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bNeedSyncClientCaps) SyncCapabilityClient();
    if (bShouldTickUpdateThisFrame) UpdateTickStatus();

    for (const auto& Capability : TickList) {
        if (Capability) {
            bool bBlocked = false;
            if (!BlockInfo.IsEmpty() && !Capability->Tags.IsEmpty()) {
                for (auto Tag : Capability->Tags) {
                    if (BlockInfo.Contains(Tag)) { bBlocked = true; break; }
                }
            }

            if (bBlocked) {
                if (Capability->bIsCapabilityActive) Capability->Deactivate();
            } else {
                Capability->NativeTick(DeltaTime);
            }
        }
    }
}

void UCapabilityComponent::OnControllerChanged(APlayerController* NewController, UEnhancedInputComponent* InputComponent) {
    if (bIsShuttingDown) return;
    if (!IsValid(NewController) || !IsValid(InputComponent)) return;
    if (!NewController->IsLocalController()) return;

    CachedController = NewController;
    CachedInputComponent = InputComponent;

    auto& Capabilities = GetSideCapabilityArray();

    for (auto& CapSet : Capabilities) {
        for (auto& Cap : CapSet.ObjectRefs) {
            if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Cap)) {
                if (IsValid(InputCap))
                    InputCap->OnGetControllerAndInputComponent(CachedController, CachedInputComponent);
            }
        }
    }
}

void UCapabilityComponent::OnControllerRemoved() {
    if (!CachedController) return;

    const auto& Capabilities = GetSideCapabilityArray();

    for (auto& Cap : Capabilities) {
        for (int i = Cap.ObjectRefs.Num() - 1; i >= 0; --i) {
            if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Cap.ObjectRefs[i])) {
                InputCap->OnMissingController();
            }
        }
    }

    CachedController = nullptr;
    CachedInputComponent = nullptr;
}

void UCapabilityComponent::UpdateTickStatus() {
    DEC_DWORD_STAT_BY(STAT_TickingCapabilityCount, TickList.Num());
    TickList.Reset();
    bShouldTickUpdateThisFrame = false;

    const auto& Caps = GetSideCapabilityArray();

    for (const auto& CapSet : Caps) {
        for (auto Cap : CapSet.ObjectRefs) {
            if (Cap && Cap->bCanEverTick && Cap->ShouldRunOnThisSide()) {
                TickList.Add(Cap);
            }
        }
    }
    INC_DWORD_STAT_BY(STAT_TickingCapabilityCount, TickList.Num());
    SetComponentTickEnabled(!TickList.IsEmpty() || bNeedSyncClientCaps);
}

void UCapabilityComponent::BlockCapability(const FName& Tag, UObject* From) {
    if (!IsValid(From)) return;
    if (bIsShuttingDown) return;
    if (ComponentMode == ECapabilityComponentMode::Local) {
        for (auto& TagInfo : BlockInfo) {
            if (TagInfo.BlockTargetTag == Tag) {
                TagInfo.From.AddUnique(From);
                return;
            }
        }
        BlockInfo.Emplace(Tag, TArray<TWeakObjectPtr<UObject>>{TWeakObjectPtr<UObject>(From)});
        return;
    }
    if (GetOwner() && GetOwner()->HasAuthority()) {
        for (auto& TagInfo : BlockInfo) {
            if (TagInfo.BlockTargetTag == Tag) {
                TagInfo.From.AddUnique(From);
                MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, BlockInfo, this);
                return;
            }
        }

        BlockInfo.Emplace(Tag, TArray<TWeakObjectPtr<UObject>>{TWeakObjectPtr<UObject>(From)});
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, BlockInfo, this);
    } else {
        ServerBlockCapability(Tag, From);
        for (auto& TagInfo : BlockInfo) {
            if (TagInfo.BlockTargetTag == Tag) {
                TagInfo.From.AddUnique(From);
                return;
            }
        }
        BlockInfo.Emplace(Tag, TArray<TWeakObjectPtr<UObject>>{TWeakObjectPtr<UObject>(From)});
    }
}

void UCapabilityComponent::UnBlockCapability(const FName& Tag, UObject* From) {
    if (!IsValid(From)) return;
    if (ComponentMode == ECapabilityComponentMode::Local) {
        for (int i = BlockInfo.Num() - 1; i >= 0; --i) {
            auto& Info = BlockInfo[i];
            if (Info.BlockTargetTag == Tag) {
                Info.From.RemoveAll([From](const TWeakObjectPtr<UObject>& Ptr) {
                    return !Ptr.IsValid() || Ptr.Get() == From;
                });
                if (Info.From.IsEmpty()) {
                    BlockInfo.RemoveAt(i);
                    break;
                }
            }
        }
        return;
    }
    if (GetOwner() && GetOwner()->HasAuthority()) {
        int Changed = 0;
        for (int i = BlockInfo.Num() - 1; i >= 0; --i) {
            auto& Info = BlockInfo[i];
            if (Info.BlockTargetTag == Tag) {
                Changed += Info.From.RemoveAll([From](const TWeakObjectPtr<UObject>& Ptr) {
                    return !Ptr.IsValid() || Ptr.Get() == From;
                });

                if (Info.From.IsEmpty()) {
                    Changed++;
                    BlockInfo.RemoveAt(i);
                    break;
                }
            }
        }

        if (Changed > 0) {
            MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, BlockInfo, this);
        }
    } else {
        ServerUnBlockCapability(Tag, From);
        for (int i = BlockInfo.Num() - 1; i >= 0; --i) {
            auto& Info = BlockInfo[i];
            if (Info.BlockTargetTag == Tag) {
                Info.From.RemoveAll([From](const TWeakObjectPtr<UObject>& Ptr) {
                    return !Ptr.IsValid() || Ptr.Get() == From;
                });

                if (Info.From.IsEmpty()) {
                    BlockInfo.RemoveAt(i);
                    break;
                }
            }
        }
    }
}

void UCapabilityComponent::ServerBlockCapability_Implementation(const FName& Tag, UObject* From) {
    BlockCapability(Tag, From);
}

void UCapabilityComponent::ServerUnBlockCapability_Implementation(const FName& Tag, UObject* From) {
    UnBlockCapability(Tag, From);
}

void UCapabilityComponent::UpdateInputCapabilities() {
    if (GetWorld()->GetNetMode() == ENetMode::NM_DedicatedServer) return;

    if (!CachedController || !CachedInputComponent) return;

    const auto& Caps = GetSideCapabilityArray();

    for (auto& CapSet : Caps) {
        for (auto Cap : CapSet.ObjectRefs) {
            if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Cap)) {
                if (InputCap->ShouldRunOnThisSide()) {
                    InputCap->OnGetControllerAndInputComponent(CachedController, CachedInputComponent);
                }
            }
        }
    }
}

void UCapabilityComponent::SyncCapabilityClient() {
    if (ToAddCollect.IsEmpty() && ToRemoveCollect.IsEmpty()) {
        bNeedSyncClientCaps = false;
        return;
    }

    int AdditionNum = 0;

    //找出需要删除的
    for (auto& Capability : ToRemoveCollect) { ToAddCollect.Remove(Capability); }

    int RemoveCount = CapabilitiesOnClient.RemoveAll([this](const FCapabilityObjectRefSet& Capability) {
        if (ToRemoveCollect.Contains(Capability)) {
            if (CachedController) {
                for (int i = Capability.ObjectRefs.Num() - 1; i >= 0; --i) {
                    if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Capability.ObjectRefs[i])) {
                        InputCap->OnMissingController();
                    }
                }
            }
            return true;
        }
        return false;
    });

    ToRemoveCollect.Reset();

    TSet<FCapabilityObjectRefSet> NotReadyAdd;

    for (auto& Capability : ToAddCollect) {
        if (Capability.MetaHead) {
            auto Ready = true;

            for (auto i : Capability.ClassOfComponents) {
                if (!IsValid(i)) {
                    UE_LOG(CapabilitySystemLog, Warning,
                           TEXT("UCapabilityComponent::SyncCapabilityClient Find Invalid Component Class on %s"),
                           GetOwner() ? *GetOwner()->GetName() : TEXT("UNKNOWN"));

                    Ready = false;
                    break;
                }

                auto Comp = GetOwner() ? GetOwner()->GetComponentByClass(i) : nullptr;
                if (!Comp) {
                    UE_LOG(CapabilitySystemLog, Warning,
                           TEXT("UCapabilityComponent::SyncCapabilityClient Find Missing Component %s on %s"),
                           *i->GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("UNKNOWN"));

                    Ready = false;
                    break;
                }
                if (!Comp->HasBegunPlay()) {
                    Ready = false;
                    break;
                }
            }

            if (Ready) {
                CapabilitiesOnClient.Add(Capability);
                Capability.CallBeginPlay();
                AdditionNum++;
            } else {
                NotReadyAdd.Add(Capability);
            }
        }
    }

    ToAddCollect.Reset();

    ToAddCollect = MoveTemp(NotReadyAdd);
    if (!ToAddCollect.IsEmpty()) {
        bNeedSyncClientCaps = true;
        SetComponentTickEnabled(true);
    } else {
        bNeedSyncClientCaps = false;
    }

    if (RemoveCount > 0 || AdditionNum > 0) {
        UpdateTickStatus();
        UpdateInputCapabilities();
    }
}


void UCapabilityComponent::OnRep_CapabilitySetListOnServer() {
    if (!GetOwner() || GetOwner()->HasAuthority()) return;

    for (auto& Cap : LastClientCapabilities) { //找出移除的
        if (!CapabilitySetListOnServer.Contains(Cap)) {
            ToRemoveCollect.Add(Cap);
        }
    }

    for (const auto& Cap : CapabilitySetListOnServer) { //找出新增的
        if (!LastClientCapabilities.Contains(Cap)) {
            ToAddCollect.Add(Cap);
        }
    }

    for (auto& Cap : ToAddCollect) { // 补全delta信息
        LastClientCapabilities.Add(Cap);
    }

    for (auto& Cap : ToRemoveCollect) { //补全delta信息
        LastClientCapabilities.Remove(Cap);
    }

    bNeedSyncClientCaps = true; //通知需要同步
    SetComponentTickEnabled(true);
}

void UCapabilityComponent::AddCapabilitySet(TSoftObjectPtr<UCapabilitySet> TargetSet) {
    if (bIsShuttingDown) return;
    auto TheOwner = GetOwner();
    if (!TheOwner) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::AddCapabilitySet called on client - TheOwner is null at %s - Unknown",
                  GetName());
    }

    // Local 模式：允许在本地端直接添加
    if (ComponentMode == ECapabilityComponentMode::Local) {
        if (TheOwner->IsActorBeingDestroyed()) {
            UE_LOGFMT(CapabilitySystemLog, Warning,
                      "UCapabilityComponent::AddCapabilitySet(Local) Add CapabilitySet to A Actor Being Destroyed is Unsafe. Terminate Add Operator, at {0} - {1}"
                      , GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
            return;
        }

        UCapabilitySet* Ptr = TargetSet.LoadSynchronous();
        if (!Ptr) {
            UE_LOGFMT(CapabilitySystemLog, Warning,
                      "UCapabilityComponent::AddCapabilitySet(Local) Failed to Load CapabilitySet Class at {0} - {1}",
                      GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
            return;
        }

        if (IsCapabilitySetExist(TargetSet)) return;

        bool ValidSet = true;
        for (auto& Capability : Ptr->ClassOfCapability) {
            if (!IsValid(Capability)) {
                UE_LOG(CapabilitySystemLog, Error,
                       TEXT("UCapabilityComponent::AddCapabilitySet(Local) InValid Capability Class in Set %s, at %s - %s"),
                       *TargetSet.GetAssetName(), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
                ValidSet = false;
            }
        }

        for (auto& Comp : Ptr->ClassOfComponent) {
            if (!IsValid(Comp)) {
                UE_LOG(CapabilitySystemLog, Error,
                       TEXT("UCapabilityComponent::AddCapabilitySet(Local) InValid Component Class in Set %s, at %s - %s"),
                       *TargetSet.GetAssetName(), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
                ValidSet = false;
            }
        }

        if (!ValidSet) return;

        bool CreateSuccess = true;

        TArray<TObjectPtr<UCapabilityDataComponent>> NewComps{};
        for (const auto& Comp : Ptr->ClassOfComponent) {
            auto Temp = Cast<UCapabilityDataComponent>(GetOwner()->AddComponentByClass(Comp, true, FTransform(), false));
            NewComps.Add(Temp);
        }

        TArray<TObjectPtr<UCapabilityBase>> NewCapabilityObjects{};
        for (const auto& Capability : Ptr->ClassOfCapability) {
            auto NewCapability = NewObject<UCapabilityBase>(this, Capability);
            if (!NewCapability) {
                UE_LOG(CapabilitySystemLog, Error,
                       TEXT("UCapabilityComponent::AddCapabilitySet(Local) Failed to create capability %s at Set %s, at %s - %s"),
                       *Capability->GetName(), *TargetSet.GetAssetName(), *GetName(),
                       GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
                CreateSuccess = false;
                break;
            }
            NewCapability->TargetCapabilityComponent = this;
            NewCapabilityObjects.Emplace(NewCapability);
        }

        if (CreateSuccess) {
            if (NewCapabilityObjects.IsEmpty()) return;

            FCapabilityObjectRefSet& TempSet = LocalCapabilities.Emplace_GetRef();
            TempSet.TargetSet = TargetSet;
            TempSet.InstanceID = InstanceGen;
            TempSet.ObjectRefs.Reserve(NewCapabilityObjects.Num());
            TempSet.ComponentRefs.Reserve(NewComps.Num());
            TempSet.ClassOfComponents.Reserve(NewComps.Num());
            InstanceGen++;

            for (auto& Comp : NewComps) {
                TempSet.ComponentRefs.Add(Comp);
                TempSet.ClassOfComponents.Add(Comp.GetClass());
            }

            for (auto& Capability : NewCapabilityObjects) {
                TempSet.ObjectRefs.Add(Capability);
            }

            TempSet.CallBeginPlay();

            if (CachedController && CachedInputComponent) {
                for (auto& Capability : NewCapabilityObjects) {
                    if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Capability)) {
                        InputCap->OnGetControllerAndInputComponent(CachedController, CachedInputComponent);
                    }
                }
            }

            UpdateTickStatus();
        } else {
            for (auto Comp : NewComps) {
                if (Comp) Comp->DestroyComponent();
            }
            for (auto Capability : NewCapabilityObjects) {
                if (Capability) Capability->MarkAsGarbage();
            }
            NewCapabilityObjects.Reset();
        }
        return;
    }

    // Authority 模式：仅服务器允许添加
    if (!TheOwner->HasAuthority()) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::AddCapabilitySet called on client - Only Server Can Add CapabilitySet at {0} - {1}"
                  , GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    if (TheOwner->IsActorBeingDestroyed()) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::AddCapabilitySet called on client - Add CapabilitySet to A Actor Being Destroyed is Unsafe. Terminate Add Operator, at {0} - {1}"
                  , GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    UCapabilitySet* Ptr = TargetSet.LoadSynchronous();
    if (!Ptr) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::AddCapabilitySet Failed to Load CapabilitySet Class at {0} - {1}",
                  GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    } else {
        // UE_LOG(CapabilitySystemLog, Log, TEXT("UCapabilityComponent::AddCapabilitySet Add CapabilitySet %s at %s - %s"),
        //        *TargetSet.GetAssetName(), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
    }

    if (IsCapabilitySetExist(TargetSet)) return;

    bool ValidSet = true;
    for (auto& Capability : Ptr->ClassOfCapability) {
        if (!IsValid(Capability)) {
            UE_LOG(CapabilitySystemLog, Error,
                   TEXT(
                       "UCapabilityComponent::AddCapabilitySet InValid Capability Class in Set %s, at %s - %s"
                   ),
                   *TargetSet.GetAssetName(),
                   *GetName(),
                   GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
            ValidSet = false;
        }
    }

    for (auto& Comp : Ptr->ClassOfComponent) {
        if (!IsValid(Comp)) {
            UE_LOG(CapabilitySystemLog, Error,
                   TEXT(
                       "UCapabilityComponent::AddCapabilitySet InValid Component Class in Set %s, at %s - %s"
                   ),
                   *TargetSet.GetAssetName(),
                   *GetName(),
                   GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
            ValidSet = false;
        }
    }

    if (!ValidSet) return;

    bool CreateSuccess = true;

    TObjectPtr<UCapabilityMetaHead> MetaHead = NewObject<UCapabilityMetaHead>(this);

    TArray<TObjectPtr<UCapabilityDataComponent>> NewComps{};
    for (const auto& Comp : Ptr->ClassOfComponent) {
        auto Temp = Cast<UCapabilityDataComponent>(GetOwner()->AddComponentByClass(Comp, true, FTransform(), false));
        Temp->SetIsReplicated(true);
        Temp->TargetMetaHead = MetaHead;
        NewComps.Add(Temp);
    }

    TArray<TObjectPtr<UCapabilityBase>> NewCapabilityObjects{};

    for (const auto& Capability : Ptr->ClassOfCapability) {
        auto NewCapability = NewObject<UCapabilityBase>(this, Capability);
        if (!NewCapability) {
            UE_LOG(CapabilitySystemLog, Error,
                   TEXT("UCapabilityComponent::AddCapabilitySet Failed to create capability %s at Set %s, at %s - %s"),
                   *Capability->GetName(), *TargetSet.GetAssetName(), *GetName(),
                   GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
            CreateSuccess = false;
            break;
        }

        NewCapability->TargetCapabilityComponent = this;
        NewCapability->TargetMetaHead = MetaHead;
        NewCapabilityObjects.Emplace(NewCapability);
    }

    if (CreateSuccess) {
        if (NewCapabilityObjects.IsEmpty()) return;

        FCapabilityObjectRefSet& TempSet = CapabilitySetListOnServer.Emplace_GetRef();
        TempSet.TargetSet = TargetSet;
        TempSet.InstanceID = InstanceGen;
        TempSet.MetaHead = MetaHead;
        TempSet.ObjectRefs.Reserve(NewCapabilityObjects.Num());
        TempSet.ComponentRefs.Reserve(NewComps.Num());
        TempSet.ClassOfComponents.Reserve(NewComps.Num());
        InstanceGen++;

        for (auto& Comp : NewComps) {
            TempSet.ComponentRefs.Add(Comp);
            TempSet.ClassOfComponents.Add(Comp.GetClass());
        }

        for (auto& Capability : NewCapabilityObjects) {
            TempSet.ObjectRefs.Add(Capability);
            MetaHead->CapabilityList.Add(Capability);
            AddReplicatedSubObject(Capability);
        }
        AddReplicatedSubObject(MetaHead);

        TempSet.CallBeginPlay();

        // 检查新添加的Capability是否需要输入
        if (CachedController && CachedInputComponent) {
            for (auto& Capability : NewCapabilityObjects) {
                if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(Capability)) {
                    InputCap->OnGetControllerAndInputComponent(CachedController, CachedInputComponent);
                }
            }
        }

        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CapabilitySetListOnServer, this);

        UpdateTickStatus();
    } else {
        for (auto& Comp : NewComps) {
            Comp->DestroyComponent();
        }
        for (auto Capability : NewCapabilityObjects) {
            Capability->MarkAsGarbage();
        }
        MetaHead->MarkAsGarbage();
        NewCapabilityObjects.Reset();
    }
}

void UCapabilityComponent::RemoveCapabilitySet(TSoftObjectPtr<UCapabilitySet> TargetSet) {
    if (bIsShuttingDown) return;
    if (ComponentMode == ECapabilityComponentMode::Local) {
        if (!TargetSet.LoadSynchronous()) {
            UE_LOG(CapabilitySystemLog, Warning,
                   TEXT("UCapabilityComponent::RemoveCapabilitySet(Local) Failed to Load CapabilitySet Class at %s - %s"),
                   *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        }

        int FindIndex = -1;
        for (int i = 0; i < LocalCapabilities.Num(); i++) {
            if (LocalCapabilities[i].TargetSet == TargetSet) {
                FindIndex = i;
                break;
            }
        }

        if (FindIndex == -1) return;

        auto CapabilitySetRef = LocalCapabilities[FindIndex];

        if (CachedController) {
            for (int i = CapabilitySetRef.ObjectRefs.Num() - 1; i >= 0; --i) {
                if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(CapabilitySetRef.ObjectRefs[i])) {
                    InputCap->OnMissingController();
                }
            }
        }

        CapabilitySetRef.CallPreEndPlay();
        CapabilitySetRef.CallEndPlay();

        for (auto& Ref : CapabilitySetRef.ComponentRefs) {
            if (Ref) Ref->DestroyComponent();
        }

        CapabilitySetRef.MarkGC();
        LocalCapabilities.RemoveAt(FindIndex);
        UpdateTickStatus();
        return;
    }

    if (!GetOwner() || !GetOwner()->HasAuthority()) {
        UE_LOG(CapabilitySystemLog, Warning,
               TEXT(
                   "UCapabilityComponent::RemoveCapabilitySet called on client - only server can remove capabilities at %s - %s"
               ),
               *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    if (!TargetSet.LoadSynchronous()) {
        UE_LOG(CapabilitySystemLog, Warning,
               TEXT("UCapabilityComponent::RemoveCapabilitySet Failed to Load CapabilitySet Class at %s - %s"),
               *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
    } else {
        UE_LOG(CapabilitySystemLog, Log,
               TEXT("UCapabilityComponent::RemoveCapabilitySet Removing Set %s, at %s - %s"),
               *TargetSet.GetAssetName(), *GetName(),
               GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
    }

    int FindIndex = -1;
    for (int i = 0; i < CapabilitySetListOnServer.Num(); i++) {
        if (CapabilitySetListOnServer[i].TargetSet == TargetSet) {
            FindIndex = i;
            break;
        }
    }

    if (FindIndex == -1) return;

    auto CapabilitySetRef = CapabilitySetListOnServer[FindIndex];

    if (CachedController) {
        for (int i = CapabilitySetRef.ObjectRefs.Num() - 1; i >= 0; --i) {
            if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(CapabilitySetRef.ObjectRefs[i])) {
                InputCap->OnMissingController();
            }
        }
    }

    CapabilitySetRef.CallPreEndPlay();

    CapabilitySetRef.CallEndPlay();

    for (auto& Ref : CapabilitySetRef.ObjectRefs)
        if (Ref) RemoveReplicatedSubObject(Ref);

    RemoveReplicatedSubObject(CapabilitySetRef.MetaHead);

    for (int i = CapabilitySetRef.ComponentRefs.Num() - 1; i >= 0; --i) {
        if (CapabilitySetRef.ComponentRefs[i]) CapabilitySetRef.ComponentRefs[i]->DestroyComponent();
    }

    CapabilitySetRef.MarkGC();

    CapabilitySetListOnServer.RemoveAt(FindIndex);

    UpdateTickStatus();

    MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CapabilitySetListOnServer, this);
}

bool UCapabilityComponent::IsCapabilitySetExist(TSoftObjectPtr<UCapabilitySet> TargetSet) {
    if (bIsShuttingDown) return false;
    if (!TargetSet.LoadSynchronous()) {
        check(false);
        return false;
    }

    const auto& Caps = GetSideCapabilityArray();

    for (auto& Capability : Caps)
        if (Capability.TargetSet == TargetSet)
            return true;

    return false;
}

void UCapabilityComponent::RemoveAllCapabilitySet() {
    if (ComponentMode == ECapabilityComponentMode::Local) {
        if (LocalCapabilities.Num() == 0) return;

        auto MovedArray = MoveTemp(LocalCapabilities);

        if (CachedController) {
            for (int m = MovedArray.Num() - 1; m >= 0; --m) {
                auto& CapabilitySet = MovedArray[m];
                for (int i = CapabilitySet.ObjectRefs.Num() - 1; i >= 0; --i) {
                    if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(CapabilitySet.ObjectRefs[i])) {
                        InputCap->OnMissingController();
                    }
                }
            }
        }

        for (int m = MovedArray.Num() - 1; m >= 0; --m) { MovedArray[m].CallPreEndPlay(); }
        for (int m = MovedArray.Num() - 1; m >= 0; --m) { MovedArray[m].CallEndPlay(); }

        for (int m = MovedArray.Num() - 1; m >= 0; --m) {
            auto& CapabilitySet = MovedArray[m];
            for (int i = CapabilitySet.ComponentRefs.Num() - 1; i >= 0; --i) {
                if (CapabilitySet.ComponentRefs[i]) CapabilitySet.ComponentRefs[i]->DestroyComponent();
            }
        }

        for (int m = MovedArray.Num() - 1; m >= 0; --m) { MovedArray[m].MarkGC(); }

        UpdateTickStatus();
        return;
    }

    if (!GetOwner() || !GetOwner()->HasAuthority()) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityComponent::RemoveAllCapabilitySet called on client - only server can remove capabilities at {0} - {1}",
                  GetName(), GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    if (CapabilitySetListOnServer.Num() == 0) return;

    auto MovedArray = MoveTemp(CapabilitySetListOnServer);

    if (CachedController) {
        for (int m = MovedArray.Num() - 1; m >= 0; --m) {
            auto& CapabilitySet = MovedArray[m];
            for (int i = CapabilitySet.ObjectRefs.Num() - 1; i >= 0; --i) {
                if (UCapabilityInput* InputCap = Cast<UCapabilityInput>(CapabilitySet.ObjectRefs[i])) {
                    InputCap->OnMissingController();
                }
            }
        }
    }

    for (int m = MovedArray.Num() - 1; m >= 0; --m) {
        MovedArray[m].CallPreEndPlay();
    }

    for (int m = MovedArray.Num() - 1; m >= 0; --m) {
        MovedArray[m].CallEndPlay();
    }

    for (int m = MovedArray.Num() - 1; m >= 0; --m) {
        auto& CapabilitySet = MovedArray[m];
        for (auto CapabilityRef : CapabilitySet.ObjectRefs) {
            if (CapabilityRef) RemoveReplicatedSubObject(CapabilityRef);
        }
        RemoveReplicatedSubObject(CapabilitySet.MetaHead);
    }

    for (int m = MovedArray.Num() - 1; m >= 0; --m) {
        auto& CapabilitySet = MovedArray[m];
        for (int i = CapabilitySet.ComponentRefs.Num() - 1; i >= 0; --i) {
            if (CapabilitySet.ComponentRefs[i]) CapabilitySet.ComponentRefs[i]->DestroyComponent();
        }
    }

    for (int m = MovedArray.Num() - 1; m >= 0; --m) {
        MovedArray[m].MarkGC();
    }

    UpdateTickStatus();

    if (!bIsShuttingDown) {
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CapabilitySetListOnServer, this);
    }
}

FString UCapabilityComponent::GetString() const {
    if (bIsShuttingDown) return FString("CapabilityManager: Invalid (Will be Destroy)");
    FString Str = "CapabilityManager: " + GetOwner()->GetName() + "\n";
    const auto& Caps = GetSideCapabilityArray();
    for (auto& CapSet : Caps) {
        Str += CapSet.TargetSet.GetAssetName() + "\n";
        for (auto& Cap : CapSet.ObjectRefs) {
            Str += "    " + Cap->GetName() + "\n";
        }
    }
    return Str;
}

void UCapabilityComponent::GetCapabilityComponentStates(TArray<FCapabilitySetState>& OutStates) {
    OutStates.Reset();

    if (bIsShuttingDown) return;
    const auto& CapSetList = GetSideCapabilityArray();

    OutStates.Reserve(CapSetList.Num());

    for (const auto& CapSet : CapSetList) {
        FCapabilitySetState State{};
        State.SetName = CapSet.TargetSet.GetAssetName();
        for (const auto& Cap : CapSet.ObjectRefs) {
            FCapabilityState CapState{};
            CapState.CapName = Cap->GetName();
            CapState.CanEverTick = Cap->bCanEverTick;
            CapState.ExecSide = Cap->GetExecuteSide();
            CapState.ShouldRunOnThisSide = Cap->ShouldRunOnThisSide();
            CapState.IsActive = Cap->bIsCapabilityActive;
            State.States.Add(CapState);
        }
        OutStates.Add(State);
    }
}

void UCapabilityComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams SharedParams{};
    SharedParams.bIsPushBased = true;
    SharedParams.RepNotifyCondition = REPNOTIFY_Always;
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, CapabilitySetListOnServer, SharedParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, BlockInfo, SharedParams);
}

void UCapabilityComponent::NotifyShouldUpdateTickStatusNextFrame() {
    bShouldTickUpdateThisFrame = true;
    SetComponentTickEnabled(true);
}
