#include "DMHealthResolver.h"

#include "Character/CharacterBase.h"
#include "PartsCharacter/PartsCharacterBase.h"
#include "DeathMatch/Character/DeathMatchPlayerCharacter.h"

namespace
{
	bool IsValidHealthPair(float CurrentHealth, float MaxHealth)
	{
		return FMath::IsFinite(CurrentHealth) && FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f;
	}
}

FDMHealthSnapshot DMHealth::MakeSnapshot(float CurrentHealth, float MaxHealth, bool bExistingAliveState, EDMHealthSource Source)
{
	FDMHealthSnapshot Result;
	if (Source == EDMHealthSource::Invalid || !IsValidHealthPair(CurrentHealth, MaxHealth))
	{
		return Result;
	}

	Result.MaxHealth = MaxHealth;
	Result.CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	Result.HealthRatio = FMath::Clamp(Result.CurrentHealth / Result.MaxHealth, 0.0f, 1.0f);
	Result.bAlive = bExistingAliveState && Result.CurrentHealth > 0.0f;
	Result.bValid = true;
	Result.HealthSource = Source;
	return Result;
}

FDMHealthSnapshot DMHealth::Resolve(const AActor* Actor)
{
	const ACharacterBase* Character = Cast<ACharacterBase>(Actor);
	if (!IsValid(Character))
	{
		return FDMHealthSnapshot();
	}

	const bool bDeathMatchBodyHealth = Character->IsA<APartsCharacterBase>() || Character->IsA<ADeathMatchPlayerCharacter>();
	const bool bExistingAliveState = !Character->IsDead;

	if (bDeathMatchBodyHealth && IsValidHealthPair(Character->CurHP, Character->MaxHP))
	{
		return MakeSnapshot(Character->CurHP, Character->MaxHP, bExistingAliveState, EDMHealthSource::BodyHP);
	}

	const float GasCurrentHealth = Character->GetHealth();
	const float GasMaxHealth = Character->GetMaxHealth();
	if (IsValidHealthPair(GasCurrentHealth, GasMaxHealth))
	{
		return MakeSnapshot(GasCurrentHealth, GasMaxHealth, bExistingAliveState, EDMHealthSource::GAS);
	}

	if (IsValidHealthPair(Character->CurHP, Character->MaxHP))
	{
		return MakeSnapshot(Character->CurHP, Character->MaxHP, bExistingAliveState, EDMHealthSource::BodyHP);
	}

	return FDMHealthSnapshot();
}

FDMHealthSnapshot UDMHealthFunctionLibrary::GetHealthSnapshot(const AActor* Actor)
{
	return DMHealth::Resolve(Actor);
}

