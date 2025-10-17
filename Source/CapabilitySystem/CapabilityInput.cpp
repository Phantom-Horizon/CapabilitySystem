#include "CapabilityInput.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

UCapabilityInput::UCapabilityInput(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
    bCanEverTick = false;
    SetExecuteSide(ECapabilityExecuteSide::LocalControlledOnly);
}

void UCapabilityInput::OnGetControllerAndInputComponent(
    APlayerController* TargetController, UEnhancedInputComponent* InputComponent) {
    
    if (!IsValid(TargetController) || !IsValid(InputComponent)) return; //无效参数跳过
    if (CachedController == TargetController && CachedInputComponent == InputComponent) return; //重复跳过
    
    UE_LOGFMT(CapabilitySystemLog, Log, "UCapabilityInput::OnGetControllerAndInputComponent: {0} - {1}.",
              GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"), GetName());
    
    if (CachedController) OnMissingController(); //如果之前被附身了, 那么移除, 一段本身不应该被执行到;
    
    CachedController = TargetController;
    CachedInputComponent = InputComponent;

    BindInputActions();
    TryUpdateMappingContext();

    OnControllerAttach();
}

void UCapabilityInput::OnMissingController() {
    UE_LOGFMT(CapabilitySystemLog, Log, "UCapabilityInput::OnMissingController: {0} - {1}.",
              GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"), GetName());
    
    RemoveInputMappingContext();
    UnBindInputActions();
    
    OnControllerDeattach();
    
    CachedInputComponent = nullptr;
    CachedController = nullptr;
}

void UCapabilityInput::TryUpdateMappingContext() {
    if (!CachedInputComponent || !CachedController) {
        RemoveInputMappingContext();
        return;
    }
    if (ShouldUseMappingContext) {
        UseInputMappingContext();
    } else {
        RemoveInputMappingContext();
    }
}

void UCapabilityInput::EndPlay() {
    Super::EndPlay();
    RemoveInputMappingContext();
    UnBindInputActions();
}

void UCapabilityInput::OnControllerDeattach_Implementation() {}

void UCapabilityInput::OnControllerAttach_Implementation() {}

bool UCapabilityInput::BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UObject* Target,
                                  FName FunctionName) {
    if (!IsValid(Target)) {
        UE_LOGFMT(CapabilitySystemLog, Error, "UCapabilityInput::BindAction Target UObject is null {0} - {1}",
                  GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), *GetName());
        return false;
    }

    if (!IsValid(Action)) {
        UE_LOGFMT(CapabilitySystemLog, Error,
                  "UCapabilityInput::BindAction - TObjectPtr<UInputAction> Action is null {0} - {1}",
                  GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), *GetName());
        return false;
    }

    if (!CachedInputComponent) {
        UE_LOGFMT(CapabilitySystemLog, Error, "UCapabilityInput::BindAction - TryGetEnhanceInputComponent Failed {0} - {1}",
                  GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), *GetName());
        return false;
    }

    if (!Target->FindFunction(FunctionName)) {
        UE_LOGFMT(CapabilitySystemLog, Error, "UCapabilityInput::BindAction - Function {0} not Find in Target {1} - {2}",
                  FunctionName,
                  GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), GetName());
        return false;
    }

    auto& Handle = CachedInputComponent->BindAction(Action, TriggerEvent, Target, FunctionName);
    ActionRecords.Add(Handle.GetHandle());
    return true;
}

bool UCapabilityInput::BindInputMappingContext(const UInputMappingContext* Context, int32 IMC_Priority) {
    if (Context == nullptr) {
        UE_LOGFMT(CapabilitySystemLog, Error,
                  "UCapabilityInput::BindAction - TObjectPtr<UInputAction> Action is null {0} - {1}",
                  GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), *GetName());
        return false;
    }

    InputMappingContexts.AddUnique(TPair<const UInputMappingContext*, int32>(Context, IMC_Priority));
    return true;
}

void UCapabilityInput::UseInput() {
    if (ShouldUseMappingContext) return;
    ShouldUseMappingContext = true;
    TryUpdateMappingContext();
}

void UCapabilityInput::StopUseInput() {
    if (!ShouldUseMappingContext) return;
    ShouldUseMappingContext = false;
    TryUpdateMappingContext();
}

UEnhancedInputLocalPlayerSubsystem* UCapabilityInput::TryGetEnhancedInputSubsystem() const {
    if (!CachedController) return nullptr;

    if (CachedController && CachedController->IsLocalController()) {
        if (ULocalPlayer* LocalPlayer = CachedController->GetLocalPlayer()) {
            if (auto IMS = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) {
                return IMS;
            }
        }
    }
    return nullptr;
}

void UCapabilityInput::UseInputMappingContext() {
    if (bIsUsingInputMappingContext) return;

    if (auto IMS = TryGetEnhancedInputSubsystem(); IMS) {
        OnBindInputMappingContext();
        for (auto& Context : InputMappingContexts) {
            if (Context.Key) {
                IMS->AddMappingContext(Context.Key, Context.Value);
            }
        }
        bIsUsingInputMappingContext = true;
    } else {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityInput::BindInputMappingContext - Failed to bind input mapping context for {0} - {1} - IMS: {2}",
                  GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"),
                  GetName(),
                  TryGetEnhancedInputSubsystem() ? TEXT("Valid") : TEXT("Null"));
    }
}

void UCapabilityInput::RemoveInputMappingContext() {
    if (!bIsUsingInputMappingContext) return;
    if (auto IMS = TryGetEnhancedInputSubsystem()) {
        for (auto& Context : InputMappingContexts) {
            if (Context.Key) {
                IMS->RemoveMappingContext(Context.Key);
            }
        }
        bIsUsingInputMappingContext = false;
        InputMappingContexts.Reset();
    }
}

void UCapabilityInput::BindInputActions() {
    if (bIsBoundActions) return;

    if (!CachedController) {
        UE_LOGFMT(CapabilitySystemLog, Warning,
                  "UCapabilityInput::BindInputMappingContext - Failed : no CachedController for {0}",
                  GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
        return;
    }

    OnBindActions();
    bIsBoundActions = true;

    UE_LOGFMT(CapabilitySystemLog, Log, "Input actions bound for {0}", GetOwner() ? GetOwner()->GetName() : TEXT("Unknown"));
}

void UCapabilityInput::UnBindInputActions() {
    if (!bIsBoundActions) return;

    if (CachedInputComponent) {
        for (auto& Handle : ActionRecords) {
            CachedInputComponent->RemoveActionBinding(Handle);
        }
    }

    ActionRecords.Reset();
    bIsBoundActions = false;
}
