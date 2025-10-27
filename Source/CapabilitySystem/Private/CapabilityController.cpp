#include "CapabilityController.h"

#include "CapabilityComponent.h"
#include "EnhancedInputComponent.h"

void ACapabilityController::SetupInputComponent() {
	Super::SetupInputComponent();

	TArray<UCapabilityComponent*> Comps{};
	GetComponents<UCapabilityComponent>(Comps);

	for (auto Comp : Comps) {
		Comp->OnControllerChanged(this, Cast<UEnhancedInputComponent>(InputComponent));
	}
}
