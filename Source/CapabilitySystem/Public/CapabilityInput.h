// Copyright ysion(LZY). All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Capability.h"
#include "EnhancedInputComponent.h"
#include "CapabilityInput.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class AArzCharacter;
class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;

UCLASS(Abstract, Blueprintable, ClassGroup=(CapabilitySystem))
class CAPABILITYSYSTEM_API UCapabilityInput : public UCapability {
    GENERATED_BODY()

public:
    UCapabilityInput(const FObjectInitializer& ObjectInitializer);

    void OnGetControllerAndInputComponent(APlayerController* TargetController,
                                          UEnhancedInputComponent* InputComponent);

    void OnMissingController();

    void TryUpdateMappingContext();

protected:
    bool ShouldUseMappingContext = true;

    virtual void EndPlay() override;

    UFUNCTION(BlueprintNativeEvent)
    void OnControllerAttach();

    UFUNCTION(BlueprintNativeEvent)
    void OnControllerDeattach();

    UFUNCTION(BlueprintNativeEvent)
    void OnBindActions();

    UFUNCTION(BlueprintNativeEvent)
    void OnBindInputMappingContext();

    UFUNCTION(BlueprintCallable)
    bool BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UObject* Target, FName FunctionName);

    UFUNCTION(BlueprintCallable)
    bool BindInputMappingContext(const UInputMappingContext* Context, int32 IMC_Priority = 0);
    
    UFUNCTION(BlueprintCallable)
    void UseInput();

    UFUNCTION(BlueprintCallable)
    void StopUseInput();

    void UseInputMappingContext();

    void RemoveInputMappingContext();

    void BindInputActions();

    void UnBindInputActions();

    UEnhancedInputLocalPlayerSubsystem* TryGetEnhancedInputSubsystem() const;

private:
    UPROPERTY()
    TArray<int32> ActionRecords;

    TArray<TPair<const UInputMappingContext*, int32>> InputMappingContexts;

    UPROPERTY()
    TObjectPtr<APlayerController> CachedController;

    UPROPERTY()
    TObjectPtr<UEnhancedInputComponent> CachedInputComponent;

    UPROPERTY()
    bool bIsUsingInputMappingContext = false;

    UPROPERTY()
    bool bIsBoundActions = false;

    friend class AArzPlayerController;

    friend class AArzCharacter;
};