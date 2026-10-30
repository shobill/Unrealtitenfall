#include "DMWeaponActor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"

namespace
{
	float RangeMultiplier(float Distance, float Effective, float Maximum, float Minimum)
	{
		if (!FMath::IsFinite(Distance) || Distance > Maximum) return 0.0f;
		if (Distance <= Effective) return 1.0f;
		if (Distance == Maximum) return FMath::Clamp(Minimum, 0.0f, 1.0f);
		const float Alpha = (Distance - Effective) / (Maximum - Effective);
		return FMath::Lerp(1.0f, FMath::Clamp(Minimum, 0.0f, 1.0f), Alpha);
	}
}

float ADMWeaponActor::GetFireProbabilityAtDistance(float Distance) const
{
	return RangeMultiplier(Distance, GetEffectiveRange(), GetMaxFireRange(), MinFireProbabilityAtMaxRange);
}

float ADMWeaponActor::GetDamageMultiplierAtDistance(float Distance) const
{
	return RangeMultiplier(Distance, GetEffectiveRange(), GetMaxFireRange(), MinDamageMultiplierAtMaxRange);
}

ADMWeaponActor::ADMWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Root);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	WeaponStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponStaticMesh"));
	WeaponStaticMesh->SetupAttachment(Root);
	WeaponStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponStaticMesh->SetGenerateOverlapEvents(false);
	WeaponStaticMesh->SetVisibility(false);

	Muzzle = CreateDefaultSubobject<UArrowComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Root);
	Muzzle->ArrowSize = 0.35f;
	Muzzle->SetRelativeLocation(FVector(40.0f, 0.0f, 0.0f));
}

FTransform ADMWeaponActor::GetMuzzleTransform() const
{
	return Muzzle ? Muzzle->GetComponentTransform() : GetActorTransform();
}

FVector ADMWeaponActor::GetMuzzleLocation() const
{
	return GetMuzzleTransform().GetLocation();
}

FVector ADMWeaponActor::GetMuzzleForward() const
{
	return GetMuzzleTransform().GetRotation().GetForwardVector();
}

void ADMWeaponActor::RecoverSpreadToTime(float WorldTimeSeconds)
{
	if (!bSpreadInitialized)
	{
		CurrentSpreadDegrees = BaseSpreadDegrees;
		bSpreadInitialized = true;
		LastShotTimeSeconds = WorldTimeSeconds;
		return;
	}

	const float Idle = WorldTimeSeconds - LastShotTimeSeconds;
	if (Idle > BurstResetSeconds)
	{
		ConsecutiveShots = 0;
	}

	if (Idle > 0.0f && SpreadRecoveryPerSecond > 0.0f)
	{
		CurrentSpreadDegrees = FMath::Max(
			BaseSpreadDegrees,
			CurrentSpreadDegrees - Idle * SpreadRecoveryPerSecond);
	}
}

float ADMWeaponActor::GetSpreadDegreesForShot(float WorldTimeSeconds, bool bIsAds)
{
	RecoverSpreadToTime(WorldTimeSeconds);

	float Spread = FMath::Clamp(CurrentSpreadDegrees, BaseSpreadDegrees, MaxSpreadDegrees);
	if (bIsAds)
	{
		Spread *= FMath::Clamp(AdsSpreadScale, 0.0f, 1.0f);
	}
	return Spread;
}

void ADMWeaponActor::NotifyShotFired(float WorldTimeSeconds)
{
	RecoverSpreadToTime(WorldTimeSeconds);

	++ConsecutiveShots;
	if (ConsecutiveShots > ShotsBeforeBloom)
	{
		CurrentSpreadDegrees = FMath::Min(
			MaxSpreadDegrees,
			CurrentSpreadDegrees + SpreadPerShotDegrees);
	}
	else
	{
		CurrentSpreadDegrees = FMath::Max(CurrentSpreadDegrees, BaseSpreadDegrees);
	}

	LastShotTimeSeconds = WorldTimeSeconds;
}

void ADMWeaponActor::ResetSpread()
{
	CurrentSpreadDegrees = BaseSpreadDegrees;
	ConsecutiveShots = 0;
	LastShotTimeSeconds = -1000.0f;
	bSpreadInitialized = true;
}
