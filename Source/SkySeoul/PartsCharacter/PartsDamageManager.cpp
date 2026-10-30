#include "PartsDamageManager.h"

#include "PartsCharacter/PartsCharacterBase.h"
#include "PartsCharacter/PartsComponent.h"
#include "Character/CharacterBase.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/HitResult.h"

UPartsDamageManager::UPartsDamageManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APartsCharacterBase* UPartsDamageManager::GetPartsOwner() const
{
	return Cast<APartsCharacterBase>(GetOwner());
}

UPartsComponent* UPartsDamageManager::GetPartsComponent() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (APartsCharacterBase* OwnerChar = Cast<APartsCharacterBase>(OwnerActor))
		{
			if (UPartsComponent* Parts = OwnerChar->GetPartsComponent())
			{
				return Parts;
			}
		}

		// BP may have cleared the TObjectPtr — still find the live component.
		return OwnerActor->FindComponentByClass<UPartsComponent>();
	}
	return nullptr;
}

FGameplayTag UPartsDamageManager::ResolveHitPartTag(const FHitResult& Hit) const
{
	UPartsComponent* Parts = GetPartsComponent();
	if (!Parts)
	{
		return FGameplayTag();
	}

    if (Hit.GetActor() != GetOwner()) return FGameplayTag();
    // Component identity takes priority for explicit/legacy component hits.
    const FGameplayTag ComponentTag = Parts->ResolvePartTagFromComponent(Hit.GetComponent());
    if (ComponentTag.IsValid()) return ComponentTag;
    const auto* OwnerChar = GetPartsOwner();
    if (!OwnerChar || Hit.GetComponent() != OwnerChar->GetMesh() || Hit.BoneName.IsNone()) return FGameplayTag();
    return Parts->ResolvePartTagForHit(Hit.BoneName, Hit.ImpactPoint);
}

float UPartsDamageManager::ApplyBodyDamage(
	float BodyDamage,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (!FMath::IsFinite(BodyDamage) || BodyDamage <= 0.0f)
	{
		return 0.f;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.f;
	}

    const auto* Character=Cast<ACharacterBase>(OwnerActor);
    const float Before=Character?Character->CurHP:0.f;
	const float Accepted=UGameplayStatics::ApplyDamage(
		OwnerActor,
		BodyDamage,
		InstigatorController,
		DamageCauser,
		UDamageType::StaticClass()
	);

    const float Actual = Character ? FMath::Max(0.f, Before - Character->CurHP) : FMath::Max(0.f, Accepted);
	if (ACharacterBase* Char = Cast<ACharacterBase>(OwnerActor); Char && Actual > 0.f)
	{
		AActor* Attacker = DamageCauser;
		if (!Attacker && InstigatorController)
		{
			Attacker = InstigatorController->GetPawn();
		}
		Char->OnHitEvent(Attacker, Actual);
	}
    return Actual;
}

FPartsHitDamageResult UPartsDamageManager::ApplyHitDamage(
	const FHitResult& Hit,
	float Damage,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	FPartsHitDamageResult Result;
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f || !GetOwner())
	{
        Result.ZeroDamageReason = TEXT("InvalidOwnerOrDamage");
		return Result;
	}
    if (Hit.GetActor() != GetOwner() || !Hit.GetComponent() || Hit.GetComponent()->GetOwner() != GetOwner())
    {
        Result.ZeroDamageReason = TEXT("ForeignHitOrDebris");
        return Result;
    }
    if (const auto* Character = GetPartsOwner(); Character && (Character->IsDead || Character->CurHP <= 0.f))
    {
        Result.ZeroDamageReason = TEXT("DeadTarget");
        return Result;
    }

    TGuardValue<const FHitResult*> HitGuard(RoutedHit,&Hit);
	const FGameplayTag PartTag = ResolveHitPartTag(Hit);
	if (PartTag.IsValid() && GetPartsComponent() && GetPartsComponent()->HasPart(PartTag))
	{
		return ApplyDamageToPart(PartTag, Damage, InstigatorController, DamageCauser);
	}

	Result.bHitPart = false;
    TGuardValue<bool> RoutingGuard(bRoutingDamage,true);
	Result.BodyDamageApplied = ApplyBodyDamage(Damage * BodyDamageOnNoPartScale, InstigatorController, DamageCauser);
    Result.bApplied=Result.BodyDamageApplied>0;
    if (!Result.bApplied) Result.ZeroDamageReason = BodyDamageOnNoPartScale <= 0.f ? TEXT("NoPartBodyScaleZero") : TEXT("BodyDamageRejected");
    OnAppliedDamage.Broadcast(Result,InstigatorController,DamageCauser,Hit.ImpactPoint,Hit.BoneName);
	return Result;
}

FPartsHitDamageResult UPartsDamageManager::ApplyDamageToPart(
	FGameplayTag PartTag,
	float Damage,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	FPartsHitDamageResult Result;
	if (!PartTag.IsValid() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
        Result.ZeroDamageReason = TEXT("InvalidPartOrDamage");
		return Result;
	}

	UPartsComponent* Parts = GetPartsComponent();
	if (!Parts || !Parts->HasPart(PartTag))
	{
        Result.ZeroDamageReason = TEXT("MissingPart");
		return Result;
	}

	Result.bHitPart = true;
	Result.HitPartTag = PartTag;
    if (const auto* Character = GetPartsOwner(); Character && (Character->IsDead || Character->CurHP <= 0.f))
    {
        Result.ZeroDamageReason = TEXT("DeadTarget");
        return Result;
    }
    const bool bDestroyed = Parts->IsPartDestroyed(PartTag);
    const auto* Character = GetPartsOwner();
    // Only this owner's retained PhAT can bypass destroyed part HP.
    // Direct tag damage and detached debris gain no new body-damage route.
    if (bDestroyed && !(RoutedHit && Character && RoutedHit->GetActor() == GetOwner()
        && RoutedHit->GetComponent() == Character->GetMesh()))
    {
        Result.ZeroDamageReason = TEXT("DestroyedPartWithoutOwnerMeshHit");
        return Result;
    }
    TGuardValue<bool> RoutingGuard(bRoutingDamage,true);
    const float Before=Parts->GetPartHP(PartTag);
	const bool bPartOk = !bDestroyed && Parts->ApplyPartDamage(PartTag, Damage * PartDamageScale);
    Result.PartDamageApplied=bPartOk?FMath::Clamp(Before-Parts->GetPartHP(PartTag),0.f,FMath::Max(0.f,Damage*PartDamageScale)):0.f;
	Result.BodyDamageApplied = ApplyBodyDamage(Damage * BodyDamageOnPartHitScale, InstigatorController, DamageCauser);

	Result.bApplied = Result.PartDamageApplied > 0.0f || Result.BodyDamageApplied > 0.0f;
    if (!Result.bApplied) Result.ZeroDamageReason = BodyDamageOnPartHitScale <= 0.f
        ? (bDestroyed ? TEXT("DestroyedPartBodyScaleZero") : TEXT("PartDamageRejectedAndBodyScaleZero"))
        : TEXT("DamageRejected");
    const FVector HitLocation=RoutedHit?FVector(RoutedHit->ImpactPoint):GetOwner()->GetActorLocation();
    OnAppliedDamage.Broadcast(Result,InstigatorController,DamageCauser,HitLocation,PartTag.GetTagName());
	return Result;
}
