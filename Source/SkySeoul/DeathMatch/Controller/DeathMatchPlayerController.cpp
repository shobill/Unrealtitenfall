#include "DeathMatchPlayerController.h"

#include "DeathMatch/Character/DeathMatchPlayerCharacter.h"
#include "DeathMatch/Debug/DMDebugObserverPawn.h"
#include "DeathMatch/Weapons/DMWeaponLoadoutComponent.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"

ADeathMatchPlayerController::ADeathMatchPlayerController()
{
	WeaponCycleNextInputTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Mouse.WheelUp"), false);
	WeaponCyclePrevInputTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Mouse.WheelDown"), false);
	AimZoomInputTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Mouse.RightMouseClick"), false);
}

void ADeathMatchPlayerController::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (InputTag.IsValid())
	{
		// The observer owns wheel zoom only while actively following. Normal player pawns
		// and a free-flight observer keep the existing input path unchanged.
		if (const ADMDebugObserverPawn* Observer = Cast<ADMDebugObserverPawn>(GetPawn());
			Observer && Observer->IsFollowingTarget()
			&& ((WeaponCycleNextInputTag.IsValid() && InputTag.MatchesTagExact(WeaponCycleNextInputTag))
				|| (WeaponCyclePrevInputTag.IsValid() && InputTag.MatchesTagExact(WeaponCyclePrevInputTag))))
		{
			return;
		}

		ADeathMatchPlayerCharacter* DMChar = Cast<ADeathMatchPlayerCharacter>(GetPawn());
		if (!DMChar)
		{
			DMChar = Cast<ADeathMatchPlayerCharacter>(PlayerCharacter);
		}

		if (DMChar)
		{
			if (UDMWeaponLoadoutComponent* Loadout = DMChar->GetWeaponLoadout())
			{
				if (WeaponCycleNextInputTag.IsValid() && InputTag.MatchesTagExact(WeaponCycleNextInputTag))
				{
					Loadout->CycleWeaponNext();
					return;
				}
				if (WeaponCyclePrevInputTag.IsValid() && InputTag.MatchesTagExact(WeaponCyclePrevInputTag))
				{
					Loadout->CycleWeaponPrev();
					return;
				}
			}

			if (AimZoomInputTag.IsValid() && InputTag.MatchesTagExact(AimZoomInputTag))
			{
				if (UDMAimZoomComponent* AimZoom = DMChar->GetAimZoom())
				{
					AimZoom->SetAiming(true);
				}
				return;
			}
		}
	}

	Super::Input_AbilityInputTagPressed(InputTag);
}

void ADeathMatchPlayerController::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (InputTag.IsValid() && AimZoomInputTag.IsValid() && InputTag.MatchesTagExact(AimZoomInputTag))
	{
		ADeathMatchPlayerCharacter* DMChar = Cast<ADeathMatchPlayerCharacter>(GetPawn());
		if (!DMChar)
		{
			DMChar = Cast<ADeathMatchPlayerCharacter>(PlayerCharacter);
		}

		if (UDMAimZoomComponent* AimZoom = DMChar ? DMChar->GetAimZoom() : nullptr)
		{
			AimZoom->SetAiming(false);
		}
		return;
	}

	Super::Input_AbilityInputTagReleased(InputTag);
}
