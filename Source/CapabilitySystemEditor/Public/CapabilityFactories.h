#pragma once

#include "CoreMinimal.h"
#include "CapabilitySystem/Public/CapabilityComponent.h"
#include "AssetToolsModule.h"

#include "CapabilityFactories.generated.h"

UCLASS()
class CAPABILITYSYSTEMEDITOR_API UCapabilitySetCollectionFactory : public UFactory {
    GENERATED_BODY()

public:
    UCapabilitySetCollectionFactory() {
        bCreateNew = true;
        bEditAfterNew = true;
        SupportedClass = UCapabilitySetCollection::StaticClass();
    }

    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
                                      EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override {
        return NewObject<UCapabilitySetCollection>(InParent, Class, Name, Flags);
    }

    virtual FText GetDisplayName() const override {
        return NSLOCTEXT("CapabilitySetCollection", "CapabilitySetCollectionFactoryName", "Capability Set Collection");
    }

    virtual uint32 GetMenuCategories() const override {
        return EAssetTypeCategories::Misc;
    }

    virtual bool ShouldShowInNewMenu() const override { return true; }
};

UCLASS()
class CAPABILITYSYSTEMEDITOR_API UCapabilitySetFactory : public UFactory {
    GENERATED_BODY() 

public:
    UCapabilitySetFactory() {
        bCreateNew = true;
        bEditAfterNew = true;
        SupportedClass = UCapabilitySet::StaticClass();
    }

    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
                                      EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override {
        return NewObject<UCapabilitySet>(InParent, Class, Name, Flags);
    }

    virtual FText GetDisplayName() const override {
        return NSLOCTEXT("UCapabilitySet", "UCapabilitySetFactoryName", "Capability Set");
    }

    virtual uint32 GetMenuCategories() const override {
        return EAssetTypeCategories::Misc;
    }

    virtual bool ShouldShowInNewMenu() const override { return true; }
};