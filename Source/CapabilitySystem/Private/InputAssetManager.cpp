#include "CapabilitySystem/Public/InputAssetManager.h"
#include "CapabilitySystem/Public/CapabilityCommon.h"
#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "EnhancedInput/Public/InputMappingContext.h"

void UInputAssetManager::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    ScanAndLoadInputAssets();
    Instance = this;
}

void UInputAssetManager::Deinitialize() {
    InputActionMap.Empty();
    InputMappingContextMap.Empty();
    Instance = nullptr;
    Super::Deinitialize();
}

void UInputAssetManager::ScanAndLoadInputAssets() {
    const UInputAssetManagerSetting* Settings = GetDefault<UInputAssetManagerSetting>();
    if (!Settings) return;
    
    for (auto IA : Settings->InputActions) {
        if (auto P = IA.LoadSynchronous()) {
            InputActionMap.Add(P->GetFName(), P);
        }
    }
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Load Action Complete, Count %d."), InputActionMap.Num());
    
    for (auto IMC : Settings->InputMappingContexts) {
        if (auto P = IMC.LoadSynchronous()) {
            InputMappingContextMap.Add(P->GetFName(), P);
        }
    }
    
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Load IMC Complete, Count %d."), InputMappingContextMap.Num());
}

UInputAction* UInputAssetManager::FindAction(const FName& ActionName) const {
    if (const TObjectPtr<UInputAction>* FoundAction = InputActionMap.Find(ActionName)) {
        return *FoundAction;
    }
    UE_LOG(CapabilitySystemLog, Error, TEXT("Could not find Input Action: %s"), *ActionName.ToString());
    return nullptr;
}

UInputMappingContext* UInputAssetManager::FindIMC(const FName& IMCName) const {
    if (const TObjectPtr<UInputMappingContext>* FoundIMC = InputMappingContextMap.Find(IMCName)) {
        return *FoundIMC;
    }
    UE_LOG(CapabilitySystemLog, Error, TEXT("Could not find Input Mapping Context: %s"), *IMCName.ToString());
    return nullptr;
}

FString UInputAssetManager::ToStringAllIMC() const {
    FString Result;
    for (const auto& Pair : InputMappingContextMap) {
        Result += Pair.Key.ToString() + "\n";
    }
    return Result;
}

FString UInputAssetManager::ToStringAllAction() const {
    FString Result;
    for (const auto& Pair : InputActionMap) {
        Result += Pair.Key.ToString() + "\n";
    }
    return Result;
}

UInputAction* UInputAssetManager::Action(const FName& ActionName) {
    if (Instance)
        return Instance->FindAction(ActionName);
    return nullptr;
}

UInputMappingContext* UInputAssetManager::IMC(const FName& IMCName) {
    if (Instance)
        return Instance->FindIMC(IMCName);
    return nullptr;
}

FString UInputAssetManager::GetStringAllAction() {
    if (Instance)
        return Instance->ToStringAllAction();
    return "";
}

FString UInputAssetManager::GetStringAllIMC() {
    if (Instance)
        return Instance->ToStringAllIMC();
    return "";
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_ArzInputAssetManagerHelpers(FAngelscriptBinds::EOrder::Late, [] {
    {
        FAngelscriptBinds::FNamespace iam("IAManager");

        FAngelscriptBinds::BindGlobalFunction("UInputAction Action(const FName& InputActionName)",
                                              &UInputAssetManager::Action);
        SCRIPT_BIND_DOCUMENTATION("尝试获取一个UInputAction");

        FAngelscriptBinds::BindGlobalFunction("UInputMappingContext IMC(const FName& IMCName)",
                                              &UInputAssetManager::IMC);
        SCRIPT_BIND_DOCUMENTATION("尝试获取一个UInputMappingContext");

        FAngelscriptBinds::BindGlobalFunction("FString GetStringAllAction()",
                                              &UInputAssetManager::GetStringAllAction);
        SCRIPT_BIND_DOCUMENTATION("获取所有Action的信息字符串");

        FAngelscriptBinds::BindGlobalFunction("FString GetStringAllIMC()",
                                              &UInputAssetManager::GetStringAllIMC);
        SCRIPT_BIND_DOCUMENTATION("获取所有InputMappingContext的信息字符串");
    }
});