#include "InputAssetManager.h"

#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "CapabilityCommon.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "AssetRegistry/AssetRegistryModule.h"

UInputAssetManagerSetting::UInputAssetManagerSetting() {
    ScanPath = TArray<FString>();
}

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
    
    // 加载资产注册表模块
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<
        FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetData;
    // --- 1. 查找并加载所有的 UInputAction ---
    // 设置过滤器，只查找 UInputAction 类的资源
    FARFilter ActionFilter;
    ActionFilter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
    ActionFilter.bRecursiveClasses = true;
    
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Prepare To Scan Path: "));
    for (const FString& Path : Settings->ScanPath) {
        ActionFilter.PackagePaths.Add(*Path);
        UE_LOG(CapabilitySystemLog, Log, TEXT("\t\t %s"), *Path);
    }
    
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Scan Action: "));
    int Count = 0;
    ActionFilter.bRecursivePaths = true;
    AssetRegistryModule.Get().GetAssets(ActionFilter, AssetData);
    for (const FAssetData& Data : AssetData) {
        if (UInputAction* LoadedAction = Cast<UInputAction>(Data.GetAsset())) {
            // 使用资源本身的名称作为Key
            InputActionMap.Add(LoadedAction->GetFName(), LoadedAction);
            Count++;
            UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Loaded Input Action: %s"), *LoadedAction->GetName());
        }
    }
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Scan Action Complete, Count %d."), Count);
    
    // --- 2. 查找并加载所有的 UInputMappingContext ---
    AssetData.Empty(); // 清空数组以便复用
    FARFilter IMCFilter;
    IMCFilter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
    IMCFilter.bRecursiveClasses = true;
    
    for (const FString& Path : Settings->ScanPath) {
        ActionFilter.PackagePaths.Add(*Path);
    }
    
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Scan IMC: "));
    Count = 0;
    
    IMCFilter.bRecursivePaths = true;
    AssetRegistryModule.Get().GetAssets(IMCFilter, AssetData);
    for (const FAssetData& Data : AssetData) {
        if (UInputMappingContext* LoadedIMC = Cast<UInputMappingContext>(Data.GetAsset())) {
            InputMappingContextMap.Add(LoadedIMC->GetFName(), LoadedIMC);
            Count++;
            UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Loaded Input Mapping Context: %s"), *LoadedIMC->GetName());
        }
    }
    UE_LOG(CapabilitySystemLog, Log, TEXT("UInputAssetManager: Scan IMC Complete, Count %d."), Count);
    
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