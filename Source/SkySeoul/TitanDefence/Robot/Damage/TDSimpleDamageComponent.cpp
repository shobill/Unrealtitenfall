#include "TitanDefence/Robot/Damage/TDSimpleDamageComponent.h"

#include "Engine/DamageEvents.h"
#include "Engine/EngineTypes.h"
#include "EngineGlobals.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/Damage/TDDamageComponentRegistry.h"
#include "TitanDefence/Robot/Damage/TDBodyDamageReceiver.h"

namespace
{
	FTDDamageResult MakeSimpleDamageResult(float Damage, AController* InstigatorController, AActor* DamageCauser,
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
}

UTDSimpleDamageComponent::UTDSimpleDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDSimpleDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	const bool bMayBindOwnerDamageEvents = TDDamageComponentRegistry::Register(GetOwner(), this);
	if (bAutoBindOwnerDamageEvents && bMayBindOwnerDamageEvents)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakeAnyDamage.AddDynamic(this, &UTDSimpleDamageComponent::HandleTakeAnyDamage);
			Owner->OnTakePointDamage.AddDynamic(this, &UTDSimpleDamageComponent::HandleTakePointDamage);
			Owner->OnTakeRadialDamage.AddDynamic(this, &UTDSimpleDamageComponent::HandleTakeRadialDamage);
			bOwnerEventsBound = true;
		}
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("SimpleDamage BeginPlay: owner='%s' autoBind=%s registryAllowed=%s eventsBound=%s receiver='%s' multiplier=%.3f."),
		*GetNameSafe(GetOwner()), bAutoBindOwnerDamageEvents ? TEXT("true") : TEXT("false"),
		bMayBindOwnerDamageEvents ? TEXT("true") : TEXT("false"),
		bOwnerEventsBound ? TEXT("true") : TEXT("false"),
		*GetNameSafe(BodyDamageReceiver.Get()), IncomingDamageMultiplier);
}

void UTDSimpleDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bOwnerEventsBound)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->OnTakeAnyDamage.RemoveDynamic(this, &UTDSimpleDamageComponent::HandleTakeAnyDamage);
			Owner->OnTakePointDamage.RemoveDynamic(this, &UTDSimpleDamageComponent::HandleTakePointDamage);
			Owner->OnTakeRadialDamage.RemoveDynamic(this, &UTDSimpleDamageComponent::HandleTakeRadialDamage);
		}
		bOwnerEventsBound = false;
	}

	TDDamageComponentRegistry::Unregister(GetOwner(), this);
	Super::EndPlay(EndPlayReason);
}

bool UTDSimpleDamageComponent::SetBodyDamageReceiver(UObject* InBodyDamageReceiver)
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
		TEXT("SimpleDamage receiver changed: owner='%s' previous='%s' new='%s'."),
		*GetNameSafe(GetOwner()), *GetNameSafe(PreviousReceiver), *GetNameSafe(InBodyDamageReceiver));
	OnBodyDamageReceiverChanged.Broadcast(PreviousReceiver, InBodyDamageReceiver);
	return true;
}

void UTDSimpleDamageComponent::ClearBodyDamageReceiver()
{
	UObject* PreviousReceiver = BodyDamageReceiver.Get();
	BodyDamageReceiver.Reset();
	if (PreviousReceiver)
	{
		OnBodyDamageReceiverChanged.Broadcast(PreviousReceiver, nullptr);
	}
}

bool UTDSimpleDamageComponent::ProcessGenericDamage(float Damage, AController* InstigatorController,
	AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakeSimpleDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Generic);
	return ResolveDamage(Result);
}

bool UTDSimpleDamageComponent::ProcessPointDamage(float Damage, const FHitResult& HitResult,
	AController* InstigatorController, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakeSimpleDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Point);
	Result.HitLocation = HitResult.ImpactPoint;
	Result.HitBoneName = HitResult.BoneName;
	return ResolveDamage(Result);
}

bool UTDSimpleDamageComponent::ProcessRadialDamage(float Damage, FVector DamageOrigin,
	const FHitResult& HitInfo, AController* InstigatorController, AActor* DamageCauser,
	TSubclassOf<UDamageType> DamageType)
{
	FTDDamageResult Result = MakeSimpleDamageResult(Damage, InstigatorController, DamageCauser,
		DamageType, ETDDamageEventType::Radial);
	Result.HitLocation = HitInfo.bBlockingHit ? FVector(HitInfo.ImpactPoint) : DamageOrigin;
	Result.HitBoneName = HitInfo.BoneName;
	return ResolveDamage(Result);
}

void UTDSimpleDamageComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (ConsumeMatchingAnyDamage(Damage, InstigatedBy, DamageCauser, DamageType))
	{
		return;
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Simple AnyDamage received: actor='%s' damage=%.2f instigator='%s' causer='%s' type='%s'."),
		*GetNameSafe(DamagedActor), Damage, *GetNameSafe(InstigatedBy),
		*GetNameSafe(DamageCauser), *GetNameSafe(DamageType));

	ProcessGenericDamage(Damage, InstigatedBy, DamageCauser, DamageType ? DamageType->GetClass() : nullptr);
}

void UTDSimpleDamageComponent::HandleTakePointDamage(AActor* DamagedActor, float Damage,
	AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName,
	FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Simple PointDamage received: actor='%s' damage=%.2f bone='%s' location=%s instigator='%s' causer='%s'."),
		*GetNameSafe(DamagedActor), Damage, *BoneName.ToString(), *HitLocation.ToCompactString(),
		*GetNameSafe(InstigatedBy), *GetNameSafe(DamageCauser));
	MarkSpecializedDamageForAnySuppression(Damage, InstigatedBy, DamageCauser, DamageType);
	FHitResult HitResult;
	HitResult.ImpactPoint = HitLocation;
	HitResult.Location = HitLocation;
	HitResult.BoneName = BoneName;
	HitResult.Component = HitComponent;
	ProcessPointDamage(Damage, HitResult, InstigatedBy, DamageCauser,
		DamageType ? DamageType->GetClass() : nullptr);
}

void UTDSimpleDamageComponent::HandleTakeRadialDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy,
	AActor* DamageCauser)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Simple RadialDamage received: actor='%s' damage=%.2f bone='%s' origin=%s instigator='%s' causer='%s'."),
		*GetNameSafe(DamagedActor), Damage, *HitInfo.BoneName.ToString(), *Origin.ToCompactString(),
		*GetNameSafe(InstigatedBy), *GetNameSafe(DamageCauser));
	MarkSpecializedDamageForAnySuppression(Damage, InstigatedBy, DamageCauser, DamageType);
	ProcessRadialDamage(Damage, Origin, HitInfo, InstigatedBy, DamageCauser,
		DamageType ? DamageType->GetClass() : nullptr);
}

bool UTDSimpleDamageComponent::ResolveDamage(FTDDamageResult& Result)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Resolving simple damage: owner='%s' event=%s original=%.2f multiplier=%.3f receiver='%s'."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Result.DamageEventType),
		Result.OriginalDamage, IncomingDamageMultiplier, *GetNameSafe(BodyDamageReceiver.Get()));
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

	if (!FMath::IsFinite(IncomingDamageMultiplier) || IncomingDamageMultiplier < 0.0f)
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidConfiguration);
		return false;
	}

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

	Result.FinalBodyDamage = Result.OriginalDamage * IncomingDamageMultiplier;
	if (!FMath::IsFinite(Result.FinalBodyDamage) || Result.FinalBodyDamage < 0.0f)
	{
		RejectDamage(Result, ETDDamageRejectionReason::InvalidDamage);
		return false;
	}

	Result.bSucceeded = true;
	Result.RejectionReason = ETDDamageRejectionReason::None;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Forwarding simple body damage: owner='%s' receiver='%s' finalDamage=%.2f."),
		*GetNameSafe(GetOwner()), *GetNameSafe(ResolvedReceiver), Result.FinalBodyDamage);
	ITDBodyDamageReceiver* Receiver = Cast<ITDBodyDamageReceiver>(ResolvedReceiver);
	if (!Receiver || !Receiver->ApplyResolvedBodyDamage(Result.FinalBodyDamage, Result))
	{
		RejectDamage(Result, ETDDamageRejectionReason::BodyDamageReceiverRejected);
		return false;
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Simple body damage accepted: owner='%s' finalDamage=%.2f."),
		*GetNameSafe(GetOwner()), Result.FinalBodyDamage);
	OnBodyDamageResolved.Broadcast(Result);
	return true;
}

void UTDSimpleDamageComponent::RejectDamage(FTDDamageResult& Result, ETDDamageRejectionReason Reason)
{
	Result.bSucceeded = false;
	Result.RejectionReason = Reason;
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Simple damage rejected: owner='%s' reason=%s event=%s original=%.2f final=%.2f receiver='%s'."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Reason),
		*UEnum::GetValueAsString(Result.DamageEventType), Result.OriginalDamage,
		Result.FinalBodyDamage, *GetNameSafe(BodyDamageReceiver.Get()));
	OnDamageRejected.Broadcast(Result);
}

void UTDSimpleDamageComponent::MarkSpecializedDamageForAnySuppression(float Damage,
	AController* InstigatorController, AActor* DamageCauser, const UDamageType* DamageType)
{
	bHasPendingAnySuppression = true;
	PendingSuppressionFrame = GFrameCounter;
	PendingSuppressionDamage = Damage;
	PendingSuppressionInstigator = InstigatorController;
	PendingSuppressionCauser = DamageCauser;
	PendingSuppressionDamageType = DamageType;
}

bool UTDSimpleDamageComponent::ConsumeMatchingAnyDamage(float Damage, AController* InstigatorController,
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

