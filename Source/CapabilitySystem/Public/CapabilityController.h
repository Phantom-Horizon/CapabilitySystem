#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CapabilityController.generated.h"

UCLASS()
class CAPABILITYSYSTEM_API ACapabilityController : public APlayerController {
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
};
