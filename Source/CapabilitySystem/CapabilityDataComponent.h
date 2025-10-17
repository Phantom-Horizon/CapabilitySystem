#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CapabilityDataComponent.generated.h"

class UCapabilityMetaHead;

UCLASS(Blueprintable, Abstract)
class UCapabilityDataComponent : public UActorComponent {
    GENERATED_BODY()
public:

    UPROPERTY(Replicated)
    TObjectPtr<UCapabilityMetaHead> TargetMetaHead = nullptr;

    UCapabilityDataComponent() { SetIsReplicatedByDefault(true); }
    
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    
    virtual void PreDestroyFromReplication() override;
};