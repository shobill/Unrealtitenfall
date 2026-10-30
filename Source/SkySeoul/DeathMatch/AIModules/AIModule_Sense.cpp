#include "AIModule_Sense.h"
#include "DeathMatch/Core/DeathMatchLog.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "AIController.h"
#include "TimerManager.h"
#include "GenericTeamAgentInterface.h"

#include "Character/CharacterBase.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"

UAIModule_Sense::UAIModule_Sense()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UAIModule_Sense::BeginPlay()
{
	Super::BeginPlay();

	CachedPerception = ResolvePerceptionComponent();
	if (CachedPerception)
	{
		CachedPerception->OnPerceptionUpdated.AddDynamic(this, &UAIModule_Sense::OnPerceptionUpdated);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CleanupTimer,
			this,
			&UAIModule_Sense::CleanupKnownTargets,
			CleanupInterval,
			true
		);
	}
}

void UAIModule_Sense::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedPerception)
	{
		CachedPerception->OnPerceptionUpdated.RemoveDynamic(this, &UAIModule_Sense::OnPerceptionUpdated);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UAIModule_Sense::ResetAwareness()
{
    if (UAIPerceptionComponent* Perception = ResolvePerceptionComponent()) Perception->ForgetAll();
    SeenTargets.Reset();
    KnownTargets.Reset();
    AwarenessMap.Reset();
    FocusTarget.Reset();
    FocusLocation = FVector::ZeroVector;
}

float UAIModule_Sense::GetWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

APawn* UAIModule_Sense::ResolveOwnerPawn() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (const AAIController* AIC = Cast<AAIController>(OwnerActor))
	{
		return AIC->GetPawn();
	}
	if (APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		return Pawn;
	}
	if (const AController* C = Cast<AController>(OwnerActor))
	{
		return C->GetPawn();
	}
	return nullptr;
}

UAIPerceptionComponent* UAIModule_Sense::ResolvePerceptionComponent() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (AAIController* AIC = Cast<AAIController>(OwnerActor))
	{
		return AIC->GetPerceptionComponent()
			? AIC->GetPerceptionComponent()
			: AIC->FindComponentByClass<UAIPerceptionComponent>();
	}

	if (APawn* Pawn = Cast<APawn>(OwnerActor))
	{
		if (AAIController* AIC = Cast<AAIController>(Pawn->GetController()))
		{
			return AIC->GetPerceptionComponent()
				? AIC->GetPerceptionComponent()
				: AIC->FindComponentByClass<UAIPerceptionComponent>();
		}
		return Pawn->FindComponentByClass<UAIPerceptionComponent>();
	}

	return OwnerActor->FindComponentByClass<UAIPerceptionComponent>();
}

bool UAIModule_Sense::IsValidHostileTarget(AActor* Target) const
{
	if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Target) || Target == GetOwner())
	{
		return false;
	}

	if (const APawn* OwnerPawn = ResolveOwnerPawn())
	{
		if (Target == OwnerPawn)
		{
			return false;
		}
	}

	if (const ACharacterBase* Character = Cast<ACharacterBase>(Target))
	{
		// IsDead is authoritative. Do NOT use IsAlive()/GetHealth() alone —
		// AI robots often have AttributeSet unset → GetHealth()==0 → false negative
		// (attitude=Hostile but hostileOk=0 forever).
		if (Character->IsDead)
		{
			return false;
		}
		if (Character->GetMaxHealth() > 0.0f && Character->GetHealth() <= 0.0f)
		{
			return false;
		}
	}

	// Same path Perception uses: AIC::GetTeamAttitudeTowards (resolves Pawn→PS/Controller).
	if (const IGenericTeamAgentInterface* SelfAgent = Cast<IGenericTeamAgentInterface>(GetOwner()))
	{
		return SelfAgent->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
	}

	if (const AAIController* AIC = Cast<AAIController>(GetOwner()))
	{
		if (const IGenericTeamAgentInterface* PSAgent = Cast<IGenericTeamAgentInterface>(AIC->PlayerState))
		{
			return PSAgent->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
		}

		return DeathMatchTeamUtils::GetAttitudeTowards(AIC->GetGenericTeamId(), *Target)
			== ETeamAttitude::Hostile;
	}

	return false;
}

void UAIModule_Sense::UpsertAwareness(AActor* Target, bool bSensedNow, const FVector& SenseLocation)
{
	if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Target))
	{
		return;
	}

	FDMAwareTarget& Data = AwarenessMap.FindOrAdd(Target);
	Data.Actor = Target;
	Data.bCurrentlySensed = bSensedNow;
	Data.bKnown = true;
    if (bSensedNow)
    {
        Data.LastSensedLocation = SenseLocation;
        Data.LastSensedTime = GetWorldTimeSeconds();
    }

	if (const APawn* OwnerPawn = ResolveOwnerPawn())
	{
		Data.DistanceSq = FVector::DistSquared(OwnerPawn->GetActorLocation(), Data.LastSensedLocation);
	}
}

void UAIModule_Sense::RemoveAwareness(AActor* Target)
{
	if (!Target)
	{
		return;
	}
	AwarenessMap.Remove(Target);
}

void UAIModule_Sense::RefreshDistanceCaches()
{
	const APawn* OwnerPawn = ResolveOwnerPawn();
	if (!OwnerPawn)
	{
		return;
	}

	const FVector OwnerLoc = OwnerPawn->GetActorLocation();
	for (TPair<TWeakObjectPtr<AActor>, FDMAwareTarget>& Pair : AwarenessMap)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}

		const FVector Loc = Pair.Value.bCurrentlySensed
			? Pair.Key->GetActorLocation()
			: Pair.Value.LastSensedLocation;
		Pair.Value.DistanceSq = FVector::DistSquared(OwnerLoc, Loc);
		if (Pair.Value.bCurrentlySensed)
		{
			Pair.Value.LastSensedLocation = Loc;
            Pair.Value.LastSensedTime = GetWorldTimeSeconds();
		}
	}
}

bool UAIModule_Sense::HasHostile() const
{
	for (const TWeakObjectPtr<AActor>& Target : KnownTargets)
	{
		if (Target.IsValid() && IsValidHostileTarget(Target.Get()))
		{
			return true;
		}
	}
	return false;
}

void UAIModule_Sense::GetSeenHostiles(TArray<AActor*>& OutHostiles) const
{
	OutHostiles.Reset();
	for (const TWeakObjectPtr<AActor>& Target : SeenTargets)
	{
		if (Target.IsValid() && IsValidHostileTarget(Target.Get()))
		{
			OutHostiles.Add(Target.Get());
		}
	}
}

void UAIModule_Sense::GetKnownHostiles(TArray<AActor*>& OutHostiles) const
{
	OutHostiles.Reset();
	for (const TWeakObjectPtr<AActor>& Target : KnownTargets)
	{
		if (Target.IsValid() && IsValidHostileTarget(Target.Get()))
		{
			OutHostiles.Add(Target.Get());
		}
	}
}

bool UAIModule_Sense::GetAwareTarget(AActor* Target, FDMAwareTarget& OutData) const
{
	if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Target))
	{
		return false;
	}

	if (const FDMAwareTarget* Found = AwarenessMap.Find(Target))
	{
		OutData = *Found;
		return Found->IsValid();
	}
	return false;
}

AActor* UAIModule_Sense::FindBestHostile(bool bPreferSeenOnly) const
{
	const APawn* OwnerPawn = ResolveOwnerPawn();
	if (!OwnerPawn)
	{
		return nullptr;
	}

	const TSet<TWeakObjectPtr<AActor>>& Candidates = bPreferSeenOnly ? SeenTargets : KnownTargets;
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector OwnerLoc = OwnerPawn->GetActorLocation();

	for (const TWeakObjectPtr<AActor>& Target : Candidates)
	{
		if (!Target.IsValid() || !IsValidHostileTarget(Target.Get()))
		{
			continue;
		}

        const FDMAwareTarget* Data=AwarenessMap.Find(Target);
        if (!Data || (bPreferSeenOnly && !Data->bCurrentlySensed)) continue;
        const FVector Loc=Data->LastSensedLocation;

		float DistSq = FVector::DistSquared(OwnerLoc, Loc);
		if (FocusTarget.Get() == Target.Get())
		{
			DistSq *= FocusHoldDistanceScale;
		}

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Target.Get();
		}
	}

	if (!Best && bPreferSeenOnly)
	{
		return FindBestHostile(false);
	}

	return Best;
}

void UAIModule_Sense::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	if (!CachedPerception)
	{
		return;
	}

	if (UpdatedActors.Num() == 0)
	{
		return;
	}

	UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Warning,
		TEXT("DM CombatDiag PercUpdate: %s actors=%d (Perception callback — before Hostile filter)"),
		*GetNameSafe(GetOwner()),
		UpdatedActors.Num());


	TArray<TObjectPtr<AActor>> NewlySeen;
	TArray<TObjectPtr<AActor>> LostNow;
	bool bExcludedTargetPurged = false;

	const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();

	FGenericTeamId SelfTeam = FGenericTeamId::NoTeam;
	const IGenericTeamAgentInterface* SelfAgent = Cast<IGenericTeamAgentInterface>(GetOwner());
	if (SelfAgent)
	{
		SelfTeam = SelfAgent->GetGenericTeamId();
	}
	else if (const AAIController* AIC = Cast<AAIController>(GetOwner()))
	{
		SelfTeam = AIC->GetGenericTeamId();
	}

	for (AActor* Updated : UpdatedActors)
	{
		if (!IsValid(Updated))
		{
			continue;
		}

		if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Updated))
		{
			const bool bWasTracked = SeenTargets.Contains(Updated)
				|| KnownTargets.Contains(Updated)
				|| AwarenessMap.Contains(Updated)
				|| FocusTarget.Get() == Updated;
			SeenTargets.Remove(Updated);
			KnownTargets.Remove(Updated);
			RemoveAwareness(Updated);
			CachedPerception->ForgetActor(Updated);
			bExcludedTargetPurged |= bWasTracked;
			continue;
		}

		const FGenericTeamId OtherTeam = DeathMatchTeamUtils::GetTeamIdFromActor(Updated);
		const ETeamAttitude::Type Attitude = SelfAgent
			? SelfAgent->GetTeamAttitudeTowards(*Updated)
			: DeathMatchTeamUtils::GetAttitudeTowards(SelfTeam, *Updated);

		const FActorPerceptionInfo* Info = CachedPerception->GetActorInfo(*Updated);
		bool bStimulusValid = false;
		bool bSuccessfullySensed = false;
		FVector SenseLoc = FVector::ZeroVector;
        if (const auto* Previous=AwarenessMap.Find(Updated)) SenseLoc=Previous->LastSensedLocation;

		if (Info)
		{
			// Prefer iterate-by-Type — SenseID as array index is unreliable across UE versions.
			for (const FAIStimulus& Stim : Info->LastSensedStimuli)
			{
				if (Stim.Type != SightID)
				{
					continue;
				}
				bStimulusValid = Stim.IsValid();
				bSuccessfullySensed = bStimulusValid && Stim.WasSuccessfullySensed();
				if (bSuccessfullySensed)
				{
					SenseLoc = Stim.StimulusLocation;
				}
				break;
			}

			// Fallback: legacy index if Type walk found nothing.
			if (!bStimulusValid && Info->LastSensedStimuli.IsValidIndex(SightID))
			{
				const FAIStimulus& SightStimulus = Info->LastSensedStimuli[SightID];
				bStimulusValid = SightStimulus.IsValid();
				bSuccessfullySensed = bStimulusValid && SightStimulus.WasSuccessfullySensed();
				if (bSuccessfullySensed)
				{
					SenseLoc = SightStimulus.StimulusLocation;
				}
			}
		}

		const bool bHostileOk = (Attitude == ETeamAttitude::Hostile) && IsValidHostileTarget(Updated);

		UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Warning,
			TEXT("DM CombatDiag Sight: %s -> %s attitude=%s selfTeam=%u otherTeam=%u sensed=%d stim=%d hostileOk=%d percHostile=%d"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Updated),
			DeathMatchTeamUtils::AttitudeToString(Attitude),
			SelfTeam.GetId(),
			OtherTeam.GetId(),
			bSuccessfullySensed ? 1 : 0,
			bStimulusValid ? 1 : 0,
			bHostileOk ? 1 : 0,
			(Info && Info->bIsHostile) ? 1 : 0);

		if (!bHostileOk)
		{
			SeenTargets.Remove(Updated);
			continue;
		}

		// Affiliation is not visibility. Failed sight must retain only the previous observation.
		const bool bSensedNow = bSuccessfullySensed;

		if (bSensedNow)
		{
			if (!SeenTargets.Contains(Updated))
			{
				NewlySeen.Add(Updated);
			}

			SeenTargets.Add(Updated);
			KnownTargets.Add(Updated);
			UpsertAwareness(Updated, true, SenseLoc);
		}
		else
		{
			if (SeenTargets.Contains(Updated))
			{
				LostNow.Add(Updated);
				SeenTargets.Remove(Updated);
				KnownTargets.Add(Updated);
				UpsertAwareness(Updated, false, SenseLoc);
			}
			else
			{
				UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Warning,
					TEXT("DM CombatDiag SightSkip: %s -> %s hostile but not sensed (LOS/stim)"),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(Updated));
			}
		}
	}

	RefreshDistanceCaches();


	if (bExcludedTargetPurged || !NewlySeen.IsEmpty() || !LostNow.IsEmpty())
	{
		if (!NewlySeen.IsEmpty())
		{
			UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Warning,
				TEXT("DM CombatDiag: %s HostileSeen n=%d first=%s known=%d (before Framework wake)"),
				*GetNameSafe(GetOwner()),
				NewlySeen.Num(),
				*GetNameSafe(NewlySeen[0]),
				KnownTargets.Num());
		}

		UpdateFocus(false);
		if (ADeathMatchRobotAIController* AIC = Cast<ADeathMatchRobotAIController>(GetOwner()))
			if (UDMCombatKnowledgeComponent* Knowledge = AIC->GetCombatKnowledge()) Knowledge->RefreshFromPerception();
	}
}

float UAIModule_Sense::GetHostileMemoryLifetime() const
{
    const auto* Config = CachedPerception ? Cast<UAISenseConfig_Sight>(CachedPerception->GetSenseConfig(UAISense::GetSenseID<UAISense_Sight>())) : nullptr;
    return Config ? Config->GetMaxAge() : 0.f;
}

void UAIModule_Sense::CleanupKnownTargets()
{
	if (!CachedPerception)
	{
		return;
	}

	const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
	const UAISenseConfig_Sight* SightConfig = Cast<UAISenseConfig_Sight>(CachedPerception->GetSenseConfig(SightID));
	const float MaxAge = SightConfig ? SightConfig->GetMaxAge() : 0.0f;

	TArray<TObjectPtr<AActor>> Forgot;

	for (auto It = KnownTargets.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<AActor> Target = *It;
		if (!Target.IsValid() || !IsValidHostileTarget(Target.Get()))
		{
			if (Target.IsValid())
			{
				Forgot.Add(Target.Get());
			}
			SeenTargets.Remove(Target);
			RemoveAwareness(Target.Get());
			It.RemoveCurrent();
			continue;
		}

		const FActorPerceptionInfo* Info = CachedPerception->GetActorInfo(*Target.Get());
		if (!Info || !Info->LastSensedStimuli.IsValidIndex(SightID))
		{
			Forgot.Add(Target.Get());
			SeenTargets.Remove(Target);
			RemoveAwareness(Target.Get());
			It.RemoveCurrent();
			continue;
		}

		const FAIStimulus& SightStimulus = Info->LastSensedStimuli[SightID];
		const bool bVisibleNow = SeenTargets.Contains(Target);
		if (!bVisibleNow)
		{
			if (MaxAge > 0.0f && SightStimulus.GetAge() > MaxAge)
			{
				Forgot.Add(Target.Get());
				RemoveAwareness(Target.Get());
				It.RemoveCurrent();
			}
			else if (FDMAwareTarget* Data = AwarenessMap.Find(Target))
			{
				Data->bCurrentlySensed = false;
				Data->bKnown = true;
			}
		}
	}

	RefreshDistanceCaches();

	if (!Forgot.IsEmpty())
	{
		for (AActor* A : Forgot)
		{
			SeenTargets.Remove(A);
			if (FocusTarget == A)
			{
				FocusTarget = nullptr;
			}
		}


		UpdateFocus(true);
	}
}

void UAIModule_Sense::UpdateFocus(bool bForce)
{
	AActor* Best = FindBestHostile(true);
	const bool bChanged = (FocusTarget.Get() != Best);

	auto ResolveFocusLocation = [this](AActor* Target) -> FVector
	{
		if (!Target)
		{
			return FVector::ZeroVector;
		}
        const FDMAwareTarget* Data=AwarenessMap.Find(Target);
        if (!Data) return FVector::ZeroVector;
        if (!Data->bCurrentlySensed) return Data->LastSensedLocation;
		if (const UDMAimTargetComponent* Aim = UDMAimTargetComponent::FindOnActor(Target))
		{
			return Aim->GetPreferredAimLocation();
		}
		return Target->GetActorLocation();
	};

	if (!bForce && !bChanged)
	{
		if (Best)
		{
			FocusLocation = ResolveFocusLocation(Best);
		}
		return;
	}

	FocusTarget = Best;
	FocusLocation = Best ? ResolveFocusLocation(Best) : FocusLocation;
	if (bChanged)
		if (ADeathMatchRobotAIController* AIC = Cast<ADeathMatchRobotAIController>(GetOwner()))
			if (UDMCombatKnowledgeComponent* Knowledge = AIC->GetCombatKnowledge()) Knowledge->RefreshFromPerception();
}
