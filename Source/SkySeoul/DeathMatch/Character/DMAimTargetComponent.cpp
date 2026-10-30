#include "DMAimTargetComponent.h"

#include "DeathMatch/Core/DeathMatchLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UDMAimTargetComponent::UDMAimTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	BodySensor = CreateDefaultSubobject<USceneComponent>(TEXT("BodySensor"));
	BodySensor->SetupAttachment(this);
	BodySensor->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));

	HeadSensor = CreateDefaultSubobject<USceneComponent>(TEXT("HeadSensor"));
	HeadSensor->SetupAttachment(this);
	HeadSensor->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f));
}

void UDMAimTargetComponent::OnRegister()
{
    // Nested default subobjects in older BP assets can retain an archetype parent.
    // Repair only that invalid attachment before child registration; preserve authored
    // offsets and valid runtime attachments (including explicit mesh/bone attachments).
    if (!IsTemplate())
        for (USceneComponent* Sensor : {BodySensor.Get(), HeadSensor.Get()})
            if (Sensor && !Sensor->IsTemplate() && Sensor->GetAttachParent() && Sensor->GetAttachParent()->IsTemplate())
                Sensor->SetupAttachment(this);
    Super::OnRegister();
}

void UDMAimTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	TryAutoAttachSensors();

	if (DMIsAimSightDiagEnabled())
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM AimSight BOOT: owner=%s AimTarget=%s Body=%s Head=%s bodyLoc=%s headLoc=%s actorLoc=%s"),
			*GetNameSafe(GetOwner()),
			*GetName(),
			*GetNameSafe(BodySensor),
			*GetNameSafe(HeadSensor),
			*GetBodyLocation().ToCompactString(),
			*GetHeadLocation().ToCompactString(),
			*GetOwner()->GetActorLocation().ToCompactString());
	}
}

USkeletalMeshComponent* UDMAimTargetComponent::ResolveOwnerMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UDMAimTargetComponent::TryAutoAttachSensors()
{
	if (!bAutoAttachToMeshBones)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = ResolveOwnerMesh();
	if (!Mesh)
	{
		return;
	}

	if (BodySensor && !BodyAttachBone.IsNone() && Mesh->DoesSocketExist(BodyAttachBone))
	{
		BodySensor->AttachToComponent(
			Mesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			BodyAttachBone);
	}

	if (HeadSensor && !HeadAttachBone.IsNone() && Mesh->DoesSocketExist(HeadAttachBone))
	{
		HeadSensor->AttachToComponent(
			Mesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			HeadAttachBone);
	}
}

UDMAimTargetComponent* UDMAimTargetComponent::FindOnActor(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDMAimTargetComponent>() : nullptr;
}

FVector UDMAimTargetComponent::GetSensorLocation(EDMAimSensorSlot Slot) const
{
	const USceneComponent* Sensor = (Slot == EDMAimSensorSlot::Head) ? HeadSensor : BodySensor;
	if (Sensor)
	{
		return Sensor->GetComponentLocation();
	}
	return GetComponentLocation();
}

void UDMAimTargetComponent::CollectSensorLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	OutLocations.Add(GetHeadLocation());
	OutLocations.Add(GetBodyLocation());
}

FVector UDMAimTargetComponent::GetPreferredAimLocation() const
{
	const FVector Body = GetBodyLocation();
	if (IsSensorLocationSane(Body))
	{
		return Body;
	}

	const FVector Head = GetHeadLocation();
	if (IsSensorLocationSane(Head))
	{
		return Head;
	}

	return GetSanitizedActorChestLocation();
}

bool UDMAimTargetComponent::IsSensorLocationSane(const FVector& SensorWorldLoc) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector ActorLoc = Owner->GetActorLocation();
	const float HeightDelta = SensorWorldLoc.Z - ActorLoc.Z;
	const float PlanarDist = FVector::Dist2D(SensorWorldLoc, ActorLoc);
	return HeightDelta >= MinSensorHeightAboveActor && PlanarDist <= MaxSensorPlanarOffset;
}

FVector UDMAimTargetComponent::GetSanitizedActorChestLocation() const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation() + FVector(0.0f, 0.0f, FallbackChestHeight);
	}
	return GetComponentLocation();
}

FVector UDMAimTargetComponent::GetSanitizedActorHeadLocation() const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation() + FVector(0.0f, 0.0f, FallbackHeadHeight);
	}
	return GetComponentLocation();
}

FVector UDMAimTargetComponent::GetCombatAimLocationFrom(
	const FVector& ObserverLocation,
	const AActor* IgnoreActor) const
{
	int32 LosChecks = 0;

	// Shooting prefers Body (stable center mass), then Head.
	const FVector Body = IsSensorLocationSane(GetBodyLocation()) ? GetBodyLocation() : GetSanitizedActorChestLocation();
	if (IsLocationVisibleFrom(ObserverLocation, Body, IgnoreActor, LosChecks))
	{
		return Body;
	}

	const FVector Head = IsSensorLocationSane(GetHeadLocation()) ? GetHeadLocation() : GetSanitizedActorHeadLocation();
	if (IsLocationVisibleFrom(ObserverLocation, Head, IgnoreActor, LosChecks))
	{
		return Head;
	}

	return GetPreferredAimLocation();
}

bool UDMAimTargetComponent::IsLocationVisibleFrom(
	const FVector& ObserverLocation,
	const FVector& TargetLocation,
	const AActor* IgnoreActor,
	int32& InOutLosChecks,
	FString* OutBlockerName) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		if (OutBlockerName)
		{
			*OutBlockerName = TEXT("no-world/owner");
		}
		return false;
	}

	++InOutLosChecks;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMAimSensorLOS), false);
	// Do NOT ignore Owner: first hit on self = visible. Ignore only the observer.
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ObserverLocation,
		TargetLocation,
		VisibilityTraceChannel,
		Params);

	if (!bHit)
	{
		return true;
	}

	AActor* HitActor = Hit.GetActor();
	if (HitActor == Owner || (HitActor && (HitActor->IsAttachedTo(Owner) || HitActor->GetOwner() == Owner)))
	{
		return true;
	}

	if (OutBlockerName)
	{
		*OutBlockerName = GetNameSafe(HitActor);
	}
	return false;
}

bool UDMAimTargetComponent::FindVisibleSensorFrom(
	const FVector& ObserverLocation,
	const AActor* IgnoreActor,
	FVector& OutSeenLocation,
	int32& OutLosChecks,
	float& OutSightStrength) const
{
	return FindVisibleSensorFrom(
		ObserverLocation,
		IgnoreActor,
		OutSeenLocation,
		OutLosChecks,
		OutSightStrength,
		nullptr);
}

bool UDMAimTargetComponent::FindVisibleSensorFrom(
	const FVector& ObserverLocation,
	const AActor* IgnoreActor,
	FVector& OutSeenLocation,
	int32& OutLosChecks,
	float& OutSightStrength,
	FString* OutFailReason) const
{
	OutLosChecks = 0;
	OutSightStrength = 0.0f;

	struct FCandidate
	{
		EDMAimSensorSlot Slot;
		float Strength;
		const TCHAR* Label;
	};

	const FCandidate Order[] = {
		{ EDMAimSensorSlot::Head, 1.0f, TEXT("Head") },
		{ EDMAimSensorSlot::Body, 0.85f, TEXT("Body") },
	};

	FString Blockers;
	for (const FCandidate& Cand : Order)
	{
		FVector Loc = GetSensorLocation(Cand.Slot);
		if (!IsSensorLocationSane(Loc))
		{
			Loc = (Cand.Slot == EDMAimSensorSlot::Head)
				? GetSanitizedActorHeadLocation()
				: GetSanitizedActorChestLocation();
		}

		FString Blocker;
		if (IsLocationVisibleFrom(ObserverLocation, Loc, IgnoreActor, OutLosChecks, &Blocker))
		{
			OutSeenLocation = Loc;
			OutSightStrength = Cand.Strength;
			return true;
		}

		if (!Blockers.IsEmpty())
		{
			Blockers += TEXT("|");
		}
		Blockers += FString::Printf(TEXT("%s<-%s"), Cand.Label, *Blocker);
	}

	if (bFallbackToActorLocationIfSensorsBlocked)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			FString Blocker;
			const FVector ActorChest = GetSanitizedActorChestLocation();
			if (IsLocationVisibleFrom(ObserverLocation, ActorChest, IgnoreActor, OutLosChecks, &Blocker))
			{
				OutSeenLocation = ActorChest;
				OutSightStrength = 0.5f;
				if (OutFailReason)
				{
					*OutFailReason = FString::Printf(TEXT("sensors-blocked(%s) fallback-chest OK"), *Blockers);
				}
				return true;
			}
			Blockers += FString::Printf(TEXT("|Chest<-%s"), *Blocker);
		}
	}

	if (OutFailReason)
	{
		*OutFailReason = Blockers.IsEmpty() ? TEXT("no-sensor") : Blockers;
	}
	return false;
}

bool UDMAimTargetComponent::QueryCanBeSeenFrom(
	const AActor* TargetActor,
	const FVector& ObserverLocation,
	const AActor* IgnoreActor,
	FVector& OutSeenLocation,
	int32& OutLosChecks,
	float& OutSightStrength)
{
	OutLosChecks = 0;
	OutSightStrength = 0.0f;

	if (!TargetActor)
	{
		return false;
	}

	if (const UDMAimTargetComponent* Aim = FindOnActor(TargetActor))
	{
		FString FailReason;
		const bool bSeen = Aim->FindVisibleSensorFrom(
			ObserverLocation,
			IgnoreActor,
			OutSeenLocation,
			OutLosChecks,
			OutSightStrength,
			&FailReason);

		if (DMIsAimSightDiagEnabled())
		{
			UE_LOG(LogDM, Warning,
				TEXT("DM AimSight: observerIgnore=%s -> target=%s seen=%d strength=%.2f checks=%d loc=%s reason=%s head=%s body=%s"),
				*GetNameSafe(IgnoreActor),
				*GetNameSafe(TargetActor),
				bSeen ? 1 : 0,
				OutSightStrength,
				OutLosChecks,
				*OutSeenLocation.ToCompactString(),
				bSeen ? TEXT("ok") : *FailReason,
				*Aim->GetHeadLocation().ToCompactString(),
				*Aim->GetBodyLocation().ToCompactString());
		}

		return bSeen;
	}

	++OutLosChecks;
	UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMAimSensorLOSFallback), false);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	const FVector TargetLoc = TargetActor->GetActorLocation();
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ObserverLocation,
		TargetLoc,
		ECC_Visibility,
		Params);

	const bool bSeen = !bHit
		|| Hit.GetActor() == TargetActor
		|| (Hit.GetActor() && Hit.GetActor()->GetOwner() == TargetActor);

	if (bSeen)
	{
		OutSeenLocation = TargetLoc;
		OutSightStrength = 0.5f;
	}

	if (DMIsAimSightDiagEnabled())
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM AimSight FALLBACK(no AimTarget): target=%s seen=%d hit=%s"),
			*GetNameSafe(TargetActor),
			bSeen ? 1 : 0,
			*GetNameSafe(Hit.GetActor()));
	}

	return bSeen;
}

bool UDMAimTargetComponent::CanBeSeenFrom(
	const FVector& ObserverLocation,
	FVector& OutSeenLocation,
	int32& NumberOfLoSChecksPerformed,
	float& OutSightStrength,
	const AActor* IgnoreActor,
	const bool* bWasVisible,
	int32* UserData) const
{
	FString FailReason;
	const bool bSeen = FindVisibleSensorFrom(
		ObserverLocation,
		IgnoreActor,
		OutSeenLocation,
		NumberOfLoSChecksPerformed,
		OutSightStrength,
		&FailReason);

	if (DMIsAimSightDiagEnabled())
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM AimSight iface: self=%s ignore=%s seen=%d strength=%.2f checks=%d reason=%s head=%s body=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(IgnoreActor),
			bSeen ? 1 : 0,
			OutSightStrength,
			NumberOfLoSChecksPerformed,
			bSeen ? TEXT("ok") : *FailReason,
			*GetHeadLocation().ToCompactString(),
			*GetBodyLocation().ToCompactString());
	}

	return bSeen;
}
