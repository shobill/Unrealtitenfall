#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TitanDefence/Robot/Character/TDBaseRobotCharacter.h"
#include "TitanDefence/Robot/Damage/TDDamageTypes.h"
#include "TDRobotStateTestTypes.generated.h"

class UTDRobotPartDamageComponent;

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UTDRobotInitialAttributesTestEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTDRobotInitialAttributesTestEffect();
};

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UTDRobotBodyDamageTestEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTDRobotBodyDamageTestEffect();
};

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UTDRobotExternalHealthDamageTestEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTDRobotExternalHealthDamageTestEffect();
};

/** Test robot with native transient effects standing in for Blueprint GE assets. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class ATDRobotStateTestCharacter : public ATDBaseRobotCharacter
{
	GENERATED_BODY()

public:
	ATDRobotStateTestCharacter();

	void ConfigureAutomaticMoveForTest(AActor* Target)
	{
		InitialMoveTargetActor = Target;
		bAutoMoveOnBeginPlay = true;
	}
};

/** Has valid initial attributes but deliberately omits the body-damage GE. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class ATDRobotMissingDamageEffectTestCharacter : public ATDBaseRobotCharacter
{
	GENERATED_BODY()

public:
	ATDRobotMissingDamageEffectTestCharacter();
};

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UTDRobotStateTestObserver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void ObserveDeath(const FTDDamageResult& DamageResult) { ++DeathCount; }

	UFUNCTION()
	void ObserveExternalDamage(const FTDRobotExternalGameplayEffectDamage& DamageRequest)
	{
		++ExternalDamageCount;
		LastExternalDamage = DamageRequest;
	}

	UFUNCTION()
	void ForwardExternalDamage(const FTDRobotExternalGameplayEffectDamage& DamageRequest);

	int32 DeathCount = 0;
	int32 ExternalDamageCount = 0;
	FTDRobotExternalGameplayEffectDamage LastExternalDamage;

	UPROPERTY(Transient)
	TObjectPtr<UTDRobotPartDamageComponent> PartDamageComponent = nullptr;
};

