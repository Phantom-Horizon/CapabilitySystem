// Copyright ysion(LZY). All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CapabilityMetaHead.generated.h"

class UCapabilityBase;

UCLASS()
class CAPABILITYSYSTEM_API UCapabilityMetaHead : public UObject {
    GENERATED_BODY()
    
public:
    UPROPERTY(Replicated)
    TArray<TWeakObjectPtr<UCapabilityBase>> CapabilityList;

    bool bHasEndPlayCalled = false;

    void CallEndPlay();

    AActor* GetOwner() const;
    
    virtual void PreDestroyFromReplication() override;
    
    virtual bool IsSupportedForNetworking() const override { return true; }

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
