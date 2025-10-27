#include "CapabilityCharacter.h"
#include "CapabilityComponent.h"
#include "EnhancedInputComponent.h"

void ACapabilityCharacter::UnPossessed() {
	TArray<UCapabilityComponent*> Comps{};
	GetComponents<UCapabilityComponent>(Comps);

	for (auto Comp : Comps) {
		Comp->OnControllerRemoved();
	}
	Super::UnPossessed();
}

void ACapabilityCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	TArray<UCapabilityComponent*> Comps{};
	GetComponents<UCapabilityComponent>(Comps);

	for (auto Comp : Comps) {
		Comp->OnControllerChanged(GetController<APlayerController>(), Cast<UEnhancedInputComponent>(InputComponent));
	}
}
