#include "TitanDefence/Robot/Damage/TDRobotPartDamageComponent.h"

#include "EngineGlobals.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/Damage/TDDamageComponentRegistry.h"
#include "TitanDefence/Robot/Damage/TDBodyDamageReceiver.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartDataAsset.h"

namespace
{
	FTDDamageResult MakePartDamageResult(float Damage, AController* InstigatorController, AActor* DamageCauser,
		TSubclassOf<UDamageType> DamageType, ETDDamageEventType EventType)
	{
		FTDDamageResult Result;
		Result.OriginalDamage = Damage;
		Result.InstigatorController = InstigatorController;
		Result.DamageCauser = DamageCauser;
		Result.DamageType = DamageType;
		Result.DamageEventType = EventType;
		return Result;
	}

	bool IsValidMultiplier(float Multiplier)
	{
		return FMath::IsFinite(Multiplier) && Multiplier >= 0.0f;
	}
}

UTDRobotPartDamageComponent::UTDRobotPartDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDRobotPartDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	const bool bMayBindOwnerDamageEvents = TDDamageComponentRegistry::Register(GetOwner(), this);
	const bool bPartsInitialized = InitializePartStates();

	if (bAutoBindOwnerDamageEvents && bMayBindOwnerDamageEvents)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakeAnyDamage.AddDynamic(this, &UTDRobotPartDamageComponent::HandleTakeAnyDamage);
			Owner->OnTakePointDamage.AddDynamic(this, &UTDRobotPartDamageComponent::HandleTakePointDamage);
			Owner->OnTakeRadialDamage.AddDynamic(this, &UTDRobotPartDamageComponent::HandleTakeRadialDamage);
			bOwnerEventsBound = true;
		}
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("PartDamage BeginPlay: owner='%s' authority=%s canBeDamaged=%s dataAsset='%s' partsInitialized=%s partCount=%d autoBind=%s registryAllowed=%s eventsBound=%s anyDelegateBound=%s pointDelegateBound=%s radialDelegateBound=%s receiver='%s'."),
		*GetNameSafe(GetOwner()),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->CanBeDamaged() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(PartDataAsset),
		bPartsInitialized ? TEXT("true") : TEXT("false"), PartRuntimeStates.Num(),
		bAutoBindOwnerDamageEvents ? TEXT("true") : TEXT("false"),
		bMayBindOwnerDamageEvents ? TEXT("true") : TEXT("false"),
		bOwnerEventsBound ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->OnTakeAnyDamage.IsBound() ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->OnTakePointDamage.IsBound() ? TEXT("true") : TEXT("false"),
		GetOwner() && GetOwner()->OnTakeRadialDamage.IsBound() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(BodyDamageReceiver.Get()));
}

void UTDRobotPartDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bOwnerEventsBound)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakeAnyDamage.RemoveDynamic(this, &UTDRobotPartDamageComponent::HandleTakeAnyDamage);
			Owner->OnTakePointDamage.RemoveDynamic(this, &UTDRobotPartDamageComponent::HandleTakePointDamage);
			Owner->OnTakeRadialDamage.RemoveDynamic(this, &UTDRobotPartDamageComponent::HandleTakeRadialDamage);
		}
		bOwnerEventsBound = false;
	}

	TDDamageComponentRegistry::Unregister(GetOwner(), this);
	Super::EndPlay(EndPlayReason);
}

bool UTDRobotPartDamageComponent::SetBodyDamageReceiver(UObject* InBodyDamageReceiver)
{
	if (!IsValid(InBodyDamageReceiver)
		|| !InBodyDamageReceiver->GetClass()->ImplementsInterface(UTDBodyDamageReceiver::StaticClass()))
	{
		FTDDamageResult RejectedResult;
		RejectDamage(RejectedResult, IsValid(InBodyDamageReceiver)
			? ETDDamageRejectionReason::BodyDamageReceiverInvalid
			: ETDDamageRejectionReason::BodyDamageReceiverNotSet);
		return false;
	}

	UObject* PreviousReceiver = BodyDamageReceiver.Get();
	if (PreviousReceiver == InBodyDamageReceiver)
	{
		return true;
	}

	BodyDamageReceiver = InBodyDamageReceiver;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("PartDamage receiver changed: owner='%s' previous='%s' new='%s'."),
		*GetNameSafe(GetOwner()), *GetNameSafe(PreviousReceiver), *GetNameSafe(InBodyDamageReceiver));
	OnBodyDamageReceiverChanged.Broadcast(PreviousReceiver, InBodyDamageReceiver);
	return true;
}

void UTDRobotPartDamageComponent::ClearBodyDamageReceiver()
{
	UObject* PreviousReceiver = BodyDamageReceiver.Get();
	BodyDamageReceiver.Reset();
	if (PreviousReceiver)
	{
		OnBodyDamageReceiverChanged.Broadcast(PreviousReceiver, nullptr);
	}
}

bool UTDRobotPartDamageComponent::InitializePartStates()
{
	PartRuntimeStates.Reset();
	bPartStatesInitialized = false;

	if (!IsValid(PartDataAsset))
	{
		UE_LOG(LogTDRobot, Warning, TEXT("Robot part damage component '%s' has no PartDataAsset."),
			*GetNameSafe(this));
		return false;
	}

	TSet<FGameplayTag> AddedTags;
	bool bConfigurationValid = true;
	for (const FTDRobotPartDefinition& Definition : PartDataAsset->Parts)
	{
		if (!Definition.PartTag.IsValid() || AddedTags.Contains(Definition.PartTag)
			|| !FMath::IsFinite(Definition.MaxHealth) || Definition.MaxHealth <= 0.0f)
		{
			UE_LOG(LogTDRobot, Warning,
				TEXT("Robot part damage component '%s' skipped invalid or duplicated part '%s'."),
				*GetNameSafe(this), *Definition.PartTag.ToString());
			bConfigurationValid = false;
			continue;
		}

		FTDRobotPartRuntimeState& RuntimeState = PartRuntimeStates.AddDefaulted_GetRef();
		RuntimeState.PartTag = Definition.PartTag;
		RuntimeState.CurrentHealth = Definition.MaxHealth;
		RuntimeState.bHealthDepleted = false;
		AddedTags.Add(Definition.PartTag);
	}

	bPartStatesInitialized = bConfigurationValid && !PartRuntimeStates.IsEmpty();
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Part states initialized: owner='%s' result=%s validParts=%d sourceDefinitions=%d."),
		*GetNameSafe(GetOwner()), bPartStatesInitialized ? TEXT("success") : TEXT("failed"),
		PartRuntimeStates.Num(), PartDataAsset->Parts.Num());
	return bPartStatesInitialized;
}

bool UTDRobotPartDamageComponent::ResetPartStates()
{
	return InitializePartStates();
}

bool UTDRobotPartDamageComponent::ProcessGenericDamage(float Damage, AController* InstigatorController,
	AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakePartDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Generic);
	return ResolveDamage(Result);
}

bool UTDRobotPartDamageComponent::ProcessPointDamage(float Damage, const FHitResult& HitResult,
	AController* InstigatorController, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakePartDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Point);
	Result.HitLocation = HitResult.ImpactPoint;
	Result.HitBoneName = HitResult.BoneName;
	return ResolveDamage(Result);
}

bool UTDRobotPartDamageComponent::ProcessRadialDamage(float Damage, FVector DamageOrigin,
	const FHitResult& HitInfo, AController* InstigatorController, AActor* DamageCauser,
	TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakePartDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Radial);
	Result.HitLocation = HitInfo.bBlockingHit ? FVector(HitInfo.ImpactPoint) : DamageOrigin;
	Result.HitBoneName = HitInfo.BoneName;
	return ResolveDamage(Result);
}

float UTDRobotPartDamageComponent::GetPartCurrentHealth(FGameplayTag PartTag) const
{
	const FTDRobotPartRuntimeState* RuntimeState = FindRuntimeState(PartTag);
	return RuntimeState ? RuntimeState->CurrentHealth : 0.0f;
}

float UTDRobotPartDamageComponent::GetPartMaxHealth(FGameplayTag PartTag) const
{
	if (!IsValid(PartDataAsset))
	{
		return 0.0f;
	}

	const FTDRobotPartDefinition* Definition = PartDataAsset->Parts.FindByPredicate(
		[PartTag](const FTDRobotPartDefinition& Candidate)
		{
			return Candidate.PartTag == PartTag;
		});
	return Definition ? Definition->MaxHealth : 0.0f;
}

float UTDRobotPartDamageComponent::GetPartHealthRatio(FGameplayTag PartTag) const
{
	const float MaxHealth = GetPartMaxHealth(PartTag);
	return MaxHealth > 0.0f ? FMath::Clamp(GetPartCurrentHealth(PartTag) / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

bool UTDRobotPartDamageComponent::IsPartHealthDepleted(FGameplayTag PartTag) const
{
	const FTDRobotPartRuntimeState* RuntimeState = FindRuntimeState(PartTag);
	return RuntimeState && RuntimeState->bHealthDepleted;
}

bool UTDRobotPartDamageComponent::ShouldApplyUnresolvedDamageToBody_Implementation(
	ETDDamageEventType DamageEventType, FName HitBoneName) const
{
	return true;
}

void UTDRobotPartDamageComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (ConsumeMatchingAnyDamage(Damage, InstigatedBy, DamageCauser, DamageType))
	{
		UE_LOG(LogTDRobotDamage, VeryVerbose,
			TEXT("Suppressed duplicate AnyDamage after specialized damage: actor='%s' damage=%.2f."),
			*GetNameSafe(DamagedActor), Damage);
		return;
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("AnyDamage received: actor='%s' damage=%.2f instigator='%s' causer='%s' type='%s'."),
		*GetNameSafe(DamagedActor), Damage, *GetNameSafe(InstigatedBy),
		*GetNameSafe(DamageCauser), *GetNameSafe(DamageType));

	ProcessGenericDamage(Damage, InstigatedBy, DamageCauser, DamageType ? DamageType->GetClass() : nullptr);
}

void UTDRobotPartDamageComponent::HandleTakePointDamage(AActor* DamagedActor, float Damage,
	AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
	FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("PointDamage received: actor='%s' damage=%.2f bone='%s' hitComponent='%s' hitLocation=%s instigator='%s' causer='%s' type='%s'."),
		*GetNameSafe(DamagedActor), Damage, *BoneName.ToString(), *GetNameSafe(HitComponent),
		*HitLocation.ToCompactString(), *GetNameSafe(InstigatedBy),
		*GetNameSafe(DamageCauser), *GetNameSafe(DamageType));
	MarkSpecializedDamageForAnySuppression(Damage, InstigatedBy, DamageCauser, DamageType);
	FHitResult HitResult;
	HitResult.ImpactPoint = HitLocation;
	HitResult.Location = HitLocation;
	HitResult.BoneName = BoneName;
	HitResult.Component = HitComponent;
	ProcessPointDamage(Damage, HitResult, InstigatedBy, DamageCauser,
		DamageType ? DamageType->GetClass() : nullptr);
}

void UTDRobotPartDamageComponent::HandleTakeRadialDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy,
	AActor* DamageCauser)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("RadialDamage received: actor='%s' damage=%.2f bone='%s' origin=%s instigator='%s' causer='%s' type='%s'."),
		*GetNameSafe(DamagedActor), Damage, *HitInfo.BoneName.ToString(), *Origin.ToCompactString(),
		*GetNameSafe(InstigatedBy), *GetNameSafe(DamageCauser), *GetNameSafe(DamageType));
	MarkSpecializedDamageForAnySuppression(Damage, InstigatedBy, DamageCauser, DamageType);
	ProcessRadialDamage(Damage, Origin, HitInfo, InstigatedBy, DamageCauser,
		DamageType ? DamageType->GetClass() : nullptr);
}

bool UTDRobotPartDamageComponent::ResolveDamage(FTDDamageResult& Result)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Resolving damage: owner='%s' event=%s original=%.2f bone='%s' receiver='%s'."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Result.DamageEventType),
		Result.OriginalDamage, *Result.HitBoneName.ToString(), *GetNameSafe(BodyDamageReceiver.Get()));

	if (!ValidateCommonRequest(Result))
	{
		return false;
	}

	if (!bPartStatesInitialized)
	{
		RejectDamage(Result, IsValid(PartDataAsset)
			? ETDDamageRejectionReason::PartStatesNotInitialized
			: ETDDamageRejectionReason::PartDataAssetNotSet);
		return false;
	}

	Result.PartTag = ResolvePartTag(Result.HitBoneName);
	if (!Result.PartTag.IsValid())
	{
		UE_LOG(LogTDRobotDamage, Warning,
			TEXT("Part resolution failed: owner='%s' bone='%s' event=%s. Evaluating body-only fallback."),
			*GetNameSafe(GetOwner()), *Result.HitBoneName.ToString(),
			*UEnum::GetValueAsString(Result.DamageEventType));
		BroadcastPartResolutionFailed(Result);
		if (!ShouldApplyUnresolvedDamageToBody(Result.DamageEventType, Result.HitBoneName))
		{
			RejectDamage(Result, ETDDamageRejectionReason::PartNotResolved);
			return false;
		}
		return ResolveBodyOnlyDamage(Result);
	}

	FTDRobotPartRuntimeState* RuntimeState = FindRuntimeState(Result.PartTag);
	if (!RuntimeState)
	{
		RejectDamage(Result, ETDDamageRejectionReason::PartStatesNotInitialized);
		return false;
	}

	const FTDRobotPartDamageRule* Rule = FindDamageRule(Result.PartTag);
	const float PartMultiplier = Rule ? Rule->PartDamageMultiplier : DefaultPartDamageMultiplier;
	const float BodyMultiplier = Rule ? Rule->BodyDamageMultiplier : DefaultBodyDamageMultiplier;
	if (!IsValidMultiplier(PartMultiplier) || !IsValidMultiplier(BodyMultiplier))
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidConfiguration);
		return false;
	}

	const float PartDamage = Result.OriginalDamage * PartMultiplier;
	Result.FinalBodyDamage = Result.OriginalDamage * BodyMultiplier;
	if (!FMath::IsFinite(PartDamage) || !FMath::IsFinite(Result.FinalBodyDamage))
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidDamage);
		return false;
	}

	const float PreviousHealth = RuntimeState->CurrentHealth;
	const bool bWasDepleted = RuntimeState->bHealthDepleted;
	RuntimeState->CurrentHealth = FMath::Max(0.0f, RuntimeState->CurrentHealth - PartDamage);
	RuntimeState->bHealthDepleted = RuntimeState->CurrentHealth <= 0.0f;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Part resolved: owner='%s' part='%s' bone='%s' partMultiplier=%.3f bodyMultiplier=%.3f partDamage=%.2f bodyDamage=%.2f partHealth=%.2f->%.2f."),
		*GetNameSafe(GetOwner()), *Result.PartTag.ToString(), *Result.HitBoneName.ToString(),
		PartMultiplier, BodyMultiplier, PartDamage, Result.FinalBodyDamage,
		PreviousHealth, RuntimeState->CurrentHealth);

	if (!ApplyBodyDamageToReceiver(Result))
	{
		RuntimeState->CurrentHealth = PreviousHealth;
		RuntimeState->bHealthDepleted = bWasDepleted;
		UE_LOG(LogTDRobotDamage, Warning,
			TEXT("Body damage rejected; rolled part health back: owner='%s' part='%s' restoredHealth=%.2f."),
			*GetNameSafe(GetOwner()), *Result.PartTag.ToString(), PreviousHealth);
		return false;
	}

	const float AppliedPartDamage = PreviousHealth - RuntimeState->CurrentHealth;
	OnPartDamaged.Broadcast(Result, AppliedPartDamage);
	if (!FMath::IsNearlyEqual(PreviousHealth, RuntimeState->CurrentHealth))
	{
		OnPartHealthChanged.Broadcast(Result.PartTag, PreviousHealth, RuntimeState->CurrentHealth,
			GetPartMaxHealth(Result.PartTag));
	}
	if (!bWasDepleted && RuntimeState->bHealthDepleted)
	{
		UE_LOG(LogTDRobotDamage, Log,
			TEXT("Part depleted: owner='%s' part='%s'. Broadcasting separation event."),
			*GetNameSafe(GetOwner()), *Result.PartTag.ToString());
		OnPartHealthDepleted.Broadcast(Result.PartTag, Result);
	}
	OnBodyDamageResolved.Broadcast(Result);
	return true;
}

bool UTDRobotPartDamageComponent::ResolveBodyOnlyDamage(FTDDamageResult& Result)
{
	if (!IsValidMultiplier(DefaultBodyDamageMultiplier))
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidConfiguration);
		return false;
	}

	Result.FinalBodyDamage = Result.OriginalDamage * DefaultBodyDamageMultiplier;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Body-only damage fallback: owner='%s' original=%.2f multiplier=%.3f finalBodyDamage=%.2f bone='%s'."),
		*GetNameSafe(GetOwner()), Result.OriginalDamage, DefaultBodyDamageMultiplier,
		Result.FinalBodyDamage, *Result.HitBoneName.ToString());
	if (!FMath::IsFinite(Result.FinalBodyDamage))
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidDamage);
		return false;
	}

	if (!ApplyBodyDamageToReceiver(Result))
	{
		return false;
	}

	OnBodyDamageResolved.Broadcast(Result);
	return true;
}

bool UTDRobotPartDamageComponent::ApplyBodyDamageToReceiver(FTDDamageResult& Result)
{
	UObject* ResolvedReceiver = BodyDamageReceiver.Get();
	if (!ResolvedReceiver)
	{
		RejectDamage(Result, ETDDamageRejectionReason::BodyDamageReceiverNotSet);
		return false;
	}

	if (!ResolvedReceiver->GetClass()->ImplementsInterface(UTDBodyDamageReceiver::StaticClass()))
	{
		RejectDamage(Result, ETDDamageRejectionReason::BodyDamageReceiverInvalid);
		return false;
	}

	Result.bSucceeded = true;
	Result.RejectionReason = ETDDamageRejectionReason::None;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Forwarding body damage: owner='%s' receiver='%s' damage=%.2f part='%s'."),
		*GetNameSafe(GetOwner()), *GetNameSafe(ResolvedReceiver), Result.FinalBodyDamage,
		*Result.PartTag.ToString());
	ITDBodyDamageReceiver* Receiver = Cast<ITDBodyDamageReceiver>(ResolvedReceiver);
	if (!Receiver || !Receiver->ApplyResolvedBodyDamage(Result.FinalBodyDamage, Result))
	{
		RejectDamage(Result, ETDDamageRejectionReason::BodyDamageReceiverRejected);
		return false;
	}
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Body damage accepted: owner='%s' receiver='%s' damage=%.2f."),
		*GetNameSafe(GetOwner()), *GetNameSafe(ResolvedReceiver), Result.FinalBodyDamage);
	return true;
}

void UTDRobotPartDamageComponent::RejectDamage(FTDDamageResult& Result, ETDDamageRejectionReason Reason)
{
	Result.bSucceeded = false;
	Result.RejectionReason = Reason;
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Damage rejected: owner='%s' reason=%s event=%s original=%.2f body=%.2f bone='%s' part='%s' receiver='%s'."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Reason),
		*UEnum::GetValueAsString(Result.DamageEventType), Result.OriginalDamage,
		Result.FinalBodyDamage, *Result.HitBoneName.ToString(), *Result.PartTag.ToString(),
		*GetNameSafe(BodyDamageReceiver.Get()));
	OnDamageRejected.Broadcast(Result);
}

void UTDRobotPartDamageComponent::BroadcastPartResolutionFailed(const FTDDamageResult& Result)
{
	FTDDamageResult FailureResult = Result;
	FailureResult.bSucceeded = false;
	FailureResult.RejectionReason = ETDDamageRejectionReason::PartNotResolved;
	OnPartResolutionFailed.Broadcast(FailureResult);
}

FTDRobotPartRuntimeState* UTDRobotPartDamageComponent::FindRuntimeState(FGameplayTag PartTag)
{
	return PartRuntimeStates.FindByPredicate([PartTag](const FTDRobotPartRuntimeState& State)
	{
		return State.PartTag == PartTag;
	});
}

const FTDRobotPartRuntimeState* UTDRobotPartDamageComponent::FindRuntimeState(FGameplayTag PartTag) const
{
	return PartRuntimeStates.FindByPredicate([PartTag](const FTDRobotPartRuntimeState& State)
	{
		return State.PartTag == PartTag;
	});
}

FGameplayTag UTDRobotPartDamageComponent::ResolvePartTag(FName HitBoneName) const
{
	if (HitBoneName.IsNone() || !IsValid(PartDataAsset))
	{
		return FGameplayTag();
	}

	const FTDRobotPartDefinition* Definition = PartDataAsset->Parts.FindByPredicate(
		[HitBoneName](const FTDRobotPartDefinition& Candidate)
		{
			return Candidate.BoneName == HitBoneName;
		});
	return Definition ? Definition->PartTag : FGameplayTag();
}

const FTDRobotPartDamageRule* UTDRobotPartDamageComponent::FindDamageRule(FGameplayTag PartTag) const
{
	return PerPartDamageRules.FindByPredicate([PartTag](const FTDRobotPartDamageRule& Rule)
	{
		return Rule.PartTag == PartTag;
	});
}

bool UTDRobotPartDamageComponent::ValidateCommonRequest(FTDDamageResult& Result)
{
	if (!bDamageEnabled)
	{
		RejectDamage(Result, ETDDamageRejectionReason::DamageDisabled);
		return false;
	}
	if (!FMath::IsFinite(Result.OriginalDamage) || Result.OriginalDamage <= 0.0f)
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidDamage);
		return false;
	}
	if (!BodyDamageReceiver.IsValid())
	{
		RejectDamage(Result, ETDDamageRejectionReason::BodyDamageReceiverNotSet);
		return false;
	}
	return true;
}

void UTDRobotPartDamageComponent::MarkSpecializedDamageForAnySuppression(float Damage,
	AController* InstigatorController, AActor* DamageCauser, const UDamageType* DamageType)
{
	bHasPendingAnySuppression = true;
	PendingSuppressionFrame = GFrameCounter;
	PendingSuppressionDamage = Damage;
	PendingSuppressionInstigator = InstigatorController;
	PendingSuppressionCauser = DamageCauser;
	PendingSuppressionDamageType = DamageType;
}

bool UTDRobotPartDamageComponent::ConsumeMatchingAnyDamage(float Damage, AController* InstigatorController,
	AActor* DamageCauser, const UDamageType* DamageType)
{
	const bool bMatches = bHasPendingAnySuppression
		&& PendingSuppressionFrame == GFrameCounter
		&& FMath::IsNearlyEqual(PendingSuppressionDamage, Damage)
		&& PendingSuppressionInstigator.Get() == InstigatorController
		&& PendingSuppressionCauser.Get() == DamageCauser
		&& PendingSuppressionDamageType.Get() == DamageType;

	bHasPendingAnySuppression = false;
	return bMatches;
}

