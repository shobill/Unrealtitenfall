#include "DeathMatchPlayerCharacter.h"

#include "DeathMatch/Weapons/DMWeaponLoadoutComponent.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"
#include "DeathMatch/AIModules/DMPerceptionStimuliSourceComponent.h"
#include "GameFramework/SpringArmComponent.h"

ADeathMatchPlayerCharacter::ADeathMatchPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponLoadout = CreateDefaultSubobject<UDMWeaponLoadoutComponent>(TEXT("WeaponLoadout"));
	AimZoom = CreateDefaultSubobject<UDMAimZoomComponent>(TEXT("AimZoom"));
	PerceptionStimuliSource = CreateDefaultSubobject<UDMPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));

	AimTarget = CreateDefaultSubobject<UDMAimTargetComponent>(TEXT("AimTarget"));
	// Capsule/actor space — mesh is usually yaw -90, so mesh-local sensors skew left/into floor.
	AimTarget->SetupAttachment(RootComponent);
}

void ADeathMatchPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyThirdPersonCameraRig();
}

void ADeathMatchPlayerCharacter::ApplyThirdPersonCameraRig()
{
	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = ThirdPersonCameraArmLength;
		Boom->SocketOffset = ThirdPersonCameraSocketOffset;
		Boom->bDoCollisionTest = true;
	}
}

void ADeathMatchPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (WeaponLoadout)
	{
		WeaponLoadout->RefreshActiveWeaponAbilities();
	}
}
