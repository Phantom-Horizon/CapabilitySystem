#pragma once

#include "CoreMinimal.h"
#include "CapabilityCommon.h"
#include "Components/ActorComponent.h"

#include "CapabilityComponent.generated.h"

UENUM(BlueprintType)
enum class ECapabilityComponentMode : uint8 {
    Authority UMETA(DisplayName = "Authority (网络同步)"),
    Local UMETA(DisplayName = "Local (仅本地)")
};

USTRUCT()
struct FPendingDestroyComps {
    GENERATED_BODY()

    UPROPERTY()
    TArray<TWeakObjectPtr<UCapabilityDataComponent>> Comps;
};

USTRUCT()
struct FCapabilityBlockInfo {
    GENERATED_BODY()
    
    UPROPERTY()
    FName BlockTargetTag;

    UPROPERTY()
    TArray<TWeakObjectPtr<UObject>> From;

    bool operator==(const FName& Tag) const {
        return BlockTargetTag == Tag;
    }
};

UCLASS(BlueprintType, ClassGroup=(CapabilitySystem), meta=(BlueprintSpawnableComponent))
class CAPABILITYSYSTEM_API UCapabilityComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "A Capability Set Config")
    ECapabilityComponentMode ComponentMode = ECapabilityComponentMode::Authority;

    UPROPERTY(EditDefaultsOnly, Category = "A Capability Set Config")
    TArray<TSoftObjectPtr<UCapabilitySetCollection>> CapabilitySetCollection;
    
    UPROPERTY(EditDefaultsOnly, Category = "A Capability Set Config")
    TArray<TSoftObjectPtr<UCapabilitySet>> CapabilitySetPresets;
    
    UCapabilityComponent();

    UFUNCTION(BlueprintCallable)
    void AddCapabilitySetCollection(TSoftObjectPtr<UCapabilitySetCollection> TargetCollection);
    
    UFUNCTION(BlueprintCallable)
    void RemoveCapabilitySetCollection(TSoftObjectPtr<UCapabilitySetCollection> TargetCollection);
    
    UFUNCTION(BlueprintCallable)
    void AddCapabilitySet(TSoftObjectPtr<UCapabilitySet> TargetSet);

    UFUNCTION(BlueprintCallable)
    void RemoveCapabilitySet(TSoftObjectPtr<UCapabilitySet> TargetSet);

    UFUNCTION(BlueprintCallable)
    bool IsCapabilitySetExist(TSoftObjectPtr<UCapabilitySet> TargetSet);
    
    UFUNCTION(BlueprintCallable)
    void RemoveAllCapabilitySet();

    UFUNCTION(BlueprintCallable)
    FString GetString() const;

    UFUNCTION(BlueprintCallable)
    void GetCapabilityComponentStates(TArray<FCapabilitySetState>& OutStates);

    virtual void OnControllerChanged(APlayerController* NewController, UEnhancedInputComponent* InputComponent);

    virtual void OnControllerRemoved();
    
protected:
    friend class UCapabilityBase;
    
    UPROPERTY()
    TArray<TObjectPtr<UCapabilityBase>> TickList{};
    
    uint32 InstanceGen = 0;
    
    bool bShouldTickUpdateThisFrame = false;

    UPROPERTY(ReplicatedUsing=OnRep_CapabilitySetListOnServer)
    TArray<FCapabilityObjectRefSet> CapabilitySetListOnServer{}; //服务端

    UPROPERTY(Replicated)
    TArray<FCapabilityBlockInfo> BlockInfo;

    UPROPERTY()
    TSet<FCapabilityObjectRefSet> LastClientCapabilities; //差异记录

    UPROPERTY()
    TArray<FCapabilityObjectRefSet> CapabilitiesOnClient; //客户端

    UPROPERTY()
    TSet<FCapabilityObjectRefSet> ToRemoveCollect;

    UPROPERTY()
    TSet<FCapabilityObjectRefSet> ToAddCollect;

    UPROPERTY()
    TObjectPtr<APlayerController> CachedController;

    UPROPERTY()
    TObjectPtr<UEnhancedInputComponent> CachedInputComponent;
    
    UPROPERTY()
    TArray<FCapabilityObjectRefSet> LocalCapabilities;
    
    bool bNeedSyncClientCaps = false;

    bool bIsShuttingDown = false;

    virtual void BeginPlay() override;
    
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    
    virtual void UpdateTickStatus();

    virtual void NotifyShouldUpdateTickStatusNextFrame();

    virtual void BlockCapability(const FName& Tag, UObject* From);
    
    virtual void UnBlockCapability(const FName& Tag, UObject* From);

    UFUNCTION(Server, Reliable)
    void ServerBlockCapability(const FName& Tag, UObject* From);

    UFUNCTION(Server, Reliable)
    void ServerUnBlockCapability(const FName& Tag, UObject* From);
    
    void UpdateInputCapabilities();
    
    void SyncCapabilityClient();

    TArray<FCapabilityObjectRefSet>& GetSideCapabilityArray() {
        if (ComponentMode == ECapabilityComponentMode::Local) return LocalCapabilities;
        const bool bNowAuthority = GetOwner() ? GetOwner()->HasAuthority() : false;
        return bNowAuthority ? CapabilitySetListOnServer : CapabilitiesOnClient;
    }

    const TArray<FCapabilityObjectRefSet>& GetSideCapabilityArray() const {
        if (ComponentMode == ECapabilityComponentMode::Local) return LocalCapabilities;
        const bool bNowAuthority = GetOwner() ? GetOwner()->HasAuthority() : false;
        return bNowAuthority ? CapabilitySetListOnServer : CapabilitiesOnClient;
    }

    UFUNCTION()
    void OnRep_CapabilitySetListOnServer();

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
