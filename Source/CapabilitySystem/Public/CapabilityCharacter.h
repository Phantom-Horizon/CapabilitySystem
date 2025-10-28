// Copyright ysion(LZY). All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CapabilityCharacter.generated.h"

UCLASS()
class CAPABILITYSYSTEM_API ACapabilityCharacter : public ACharacter {
	GENERATED_BODY()

public:
	ACapabilityCharacter() = default;

protected:
	virtual void UnPossessed() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
};
