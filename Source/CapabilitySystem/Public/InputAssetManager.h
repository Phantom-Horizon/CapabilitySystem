#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "EnhancedInputSubsystems.h"

#include "InputAssetManager.generated.h"


UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Input Asset Manager"))
class CAPABILITYSYSTEM_API UInputAssetManagerSetting : public UDeveloperSettings {
    GENERATED_BODY()
public:

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<TSoftObjectPtr<UInputAction>> InputActions {};

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<TSoftObjectPtr<UInputMappingContext>> InputMappingContexts {};
    
    UInputAssetManagerSetting() = default;
};

UCLASS()
class CAPABILITYSYSTEM_API UInputAssetManager : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:

    inline static UInputAssetManager* Instance = nullptr;
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    
    virtual void Deinitialize() override;
    
    UInputAction* FindAction(const FName& ActionName) const;
    
    UInputMappingContext* FindIMC(const FName& IMCName) const;
    
    FString ToStringAllAction() const;

    FString ToStringAllIMC() const;
    
    static UInputAction* Action(const FName& ActionName);
    
    static UInputMappingContext* IMC(const FName& IMCName);

    static FString GetStringAllAction();
    
    static FString GetStringAllIMC();

private:
    
    UPROPERTY()
    TMap<FName, TObjectPtr<UInputAction>> InputActionMap;
    
    UPROPERTY()
    TMap<FName, TObjectPtr<UInputMappingContext>> InputMappingContextMap;
    
    void ScanAndLoadInputAssets();
};
