#pragma once

#include "CoreMinimal.h"
#include "CapabilityBase.h"
#include "CapabilityAsset.generated.h"

class UCapabilityDataComponent;

UCLASS(Blueprintable, BlueprintType)
class CAPABILITYSYSTEM_API UCapabilitySet : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    
    UPROPERTY(EditAnywhere)
    TArray<TSubclassOf<UCapabilityBase>> ClassOfCapability;
    
    UPROPERTY(EditAnywhere)
    TArray<TSubclassOf<UCapabilityDataComponent>> ClassOfComponent;
};

UCLASS(Blueprintable, BlueprintType)
class UCapabilitySetCollection : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    TArray<TSoftObjectPtr<UCapabilitySet>> CapabilitySets;
};

USTRUCT(Blueprintable, BlueprintType)
struct FCapabilityState {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString CapName = "";

    UPROPERTY(BlueprintReadOnly)
    bool CanEverTick = false;

    UPROPERTY(BlueprintReadOnly)
    bool IsActive = false;

    UPROPERTY(BlueprintReadOnly)
    ECapabilityExecuteSide ExecSide = ECapabilityExecuteSide::LocalControlledOnly;

    UPROPERTY(BlueprintReadOnly)
    bool ShouldRunOnThisSide = false;
};

USTRUCT(Blueprintable, BlueprintType)
struct FCapabilitySetState {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SetName = "";

    UPROPERTY(BlueprintReadOnly)
    TArray<FCapabilityState> States = {};
};

USTRUCT()
struct FCapabilityObjectRefSet {
    GENERATED_BODY()

    UPROPERTY()
    TSoftObjectPtr<UCapabilitySet> TargetSet;

    UPROPERTY()
    uint32 InstanceID = 0;

    UPROPERTY()
    TObjectPtr<UCapabilityMetaHead> MetaHead;

    UPROPERTY()
    TArray<TObjectPtr<UCapabilityBase>> ObjectRefs;

    UPROPERTY(NotReplicated)
    TArray<TObjectPtr<UCapabilityDataComponent>> ComponentRefs;

    UPROPERTY()
    TArray<TSubclassOf<UCapabilityDataComponent>> ClassOfComponents;

    void CallBeginPlay();

    void CallPreEndPlay();

    void CallEndPlay();

    void MarkGC();

    bool operator==(const FCapabilityObjectRefSet& Other) const {
        return InstanceID == Other.InstanceID;
    }

    friend uint32 GetTypeHash(const FCapabilityObjectRefSet& Set) {
        return Set.InstanceID;
    }
};