// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/WeaponGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "Components/ArrowComponent.h"


#include "Character/CharacterBase.h"
#include "Inventory/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/GunWeaponObject.h"
#include "Character/SkySeoulCharacter.h"
#include "Character/Abilities/Attributes/WeaponAttributeSet.h"
#include "Inventory/EquipmentItemActor.h"

FTransform UWeaponGameplayAbility::GetEndLocation()
{
	ASkySeoulCharacter* Character = Cast<ASkySeoulCharacter>(GetCharacterFromActorInfo());
	UEquipmentComponent* EquipComponent = Character->FindComponentByClass<UEquipmentComponent>();

	AActor* EquipActor = EquipComponent->GetEquipActor();

	AEquipmentItemActor* EquipmentItemActor = Cast<AEquipmentItemActor>(EquipActor);


	const FVector ActorLoc = EquipmentItemActor->StartPoint->GetComponentLocation();
	const FVector EndPointLoc = EquipmentItemActor->EndPoint->GetComponentLocation();
	FQuat AimQuat = EquipmentItemActor->StartPoint->GetComponentQuat();
	FVector SourceLoc = ActorLoc;
	FVector CamLoc;
	FRotator CamRot;
	Character->GetController()->GetPlayerViewPoint(CamLoc, CamRot);
	double FocalDistance = 1024.0f;
	FVector FocalLoc;
	FVector AimDir = CamRot.Vector().GetSafeNormal();
	FocalLoc = CamLoc + (AimDir * FocalDistance);
	CamLoc = FocalLoc + (((ActorLoc - FocalLoc) | AimDir) * AimDir);
	FocalLoc = CamLoc + (AimDir * FocalDistance);

	if (Character->PlayerActionState == EPlayerActionState::E_ZoomAction)
	{
		return FTransform(CamRot, CamLoc);
		//return FTransform((FocalLoc - SourceLoc).Rotation(), SourceLoc);
	}

	return FTransform((EndPointLoc - ActorLoc).Rotation(), SourceLoc);
}

FHitResult UWeaponGameplayAbility::WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, TArray<FHitResult>& OutHitResults) const
{
	TArray<FHitResult> HitResults;

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), /*bTraceComplex=*/ true, /*IgnoreActor=*/ GetAvatarActorFromActorInfo());
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		TArray<AActor*> AttachedActors;
		Avatar->GetAttachedActors(/*out*/ AttachedActors);
		TraceParams.AddIgnoredActors(AttachedActors);
	}
	const ECollisionChannel TraceChannel = ECollisionChannel::ECC_GameTraceChannel1;
	if (SweepRadius > 0.0f)
	{
		GetWorld()->SweepMultiByChannel(HitResults, StartTrace, EndTrace, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(SweepRadius), TraceParams);
	}
	else
	{
		GetWorld()->LineTraceMultiByChannel(HitResults, StartTrace, EndTrace, TraceChannel, TraceParams);
	}

	FHitResult Hit(ForceInit);
	if (HitResults.Num() > 0)
	{
		for (FHitResult& CurHitResult : HitResults)
		{
			auto Pred = [&CurHitResult](const FHitResult& Other)
				{
					return Other.HitObjectHandle == CurHitResult.HitObjectHandle;
				};

			if (!OutHitResults.ContainsByPredicate(Pred))
			{
				OutHitResults.Add(CurHitResult);
			}
		}
		Hit = OutHitResults.Last();
	}
	else
	{
		Hit.TraceStart = StartTrace;
		Hit.TraceEnd = EndTrace;
	}
	return Hit;
}

void UWeaponGameplayAbility::OnFire()
{
	FTransform TargetTransform = GetEndLocation();

	FVector StartLocation = TargetTransform.GetTranslation();
	FVector EndLocation = StartLocation + (TargetTransform.GetUnitAxis(EAxis::X) * FireDistance);
	//FVector EndLocation = StartLocation + (Character->GetActorForwardVector() * 100.f);

	TArray<FHitResult> OutResults;

	FGameplayAbilityTargetDataHandle TargetData;
	FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit();
	HitData->HitResult = WeaponTrace(StartLocation, EndLocation, 0.f, true, OutResults);
	//DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red,false,3.f);


	TargetData.Add(HitData);
	TargetHit(TargetData);
}

void UWeaponGameplayAbility::SetWeaponObject(const FGameplayAbilityActorInfo* ActorInfo)
{
	ASkySeoulCharacter* Character = Cast<ASkySeoulCharacter>(ActorInfo->AvatarActor);
	WeaponObject = Cast<UGunWeaponObject>(Character->EquipComponent->GetSelectedItem());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	WeaponAttribute = Cast<UWeaponAttributeSet>(ASC->GetAttributeSet(UWeaponAttributeSet::StaticClass()));

	if (WeaponAttribute)
	{
		FireDelayTimeSecs = WeaponAttribute->GetFireTime();
		FireDistance = WeaponAttribute->GetDistance();
		FireLevel = WeaponAttribute->GetWeaponPowerLevel();
	}
}

bool UWeaponGameplayAbility::CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitCheck(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}
	if (WeaponObject == nullptr)
		SetWeaponObject(ActorInfo);
	if (WeaponObject != nullptr)
	{
		if (WeaponAttribute->GetCurrentMagazine() <= 0)
			return false;
	}
	return true;
}

bool UWeaponGameplayAbility::CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags)
{
	Super::CommitAbilityCost(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);

	check(ActorInfo);
	if (WeaponObject != nullptr)
	{

	}
	return true;
}