#include "DMCombatKnowledgeComponent.h"

#include "DeathMatch/AIModules/AIModule_Sense.h"
#include "DeathMatch/AIModules/DMPerceptionTypes.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "Engine/World.h"

UDMCombatKnowledgeComponent::UDMCombatKnowledgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .1f;
}

void UDMCombatKnowledgeComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshFromPerception();
}

void UDMCombatKnowledgeComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFromPerception();
}

UAIModule_Sense* UDMCombatKnowledgeComponent::ResolveSense() const
{
	if (const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner()))
	{
		return AI->GetSenseModule();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<UAIModule_Sense>() : nullptr;
}

FDMRememberedThreat* UDMCombatKnowledgeComponent::FindThreat(AActor* Actor)
{
	return Threats.FindByPredicate([Actor](const FDMRememberedThreat& Entry) { return Entry.Actor == Actor; });
}

const FDMRememberedThreat* UDMCombatKnowledgeComponent::FindThreat(const AActor* Actor) const
{
	return Threats.FindByPredicate([Actor](const FDMRememberedThreat& Entry) { return Entry.Actor == Actor; });
}

void UDMCombatKnowledgeComponent::RefreshFromPerception()
{
	UAIModule_Sense* Sense = ResolveSense();
	if (!Sense)
	{
		Threats.Reset();
		ThreatAwareness = EDMThreatAwareness::Disengaged;
		return;
	}

	TArray<AActor*> Known;
	Sense->GetKnownHostiles(Known);
	TSet<TObjectPtr<AActor>> SeenThisRefresh;
	for (AActor* Actor : Known)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		FDMAwareTarget Aware;
		if (!Sense->GetAwareTarget(Actor, Aware))
		{
			continue;
		}
		FDMRememberedThreat* Entry = FindThreat(Actor);
		if (!Entry)
		{
			Entry = &Threats.AddDefaulted_GetRef();
			Entry->Actor = Actor;
		}
		if (Entry->bVisible && !Aware.bCurrentlySensed && CombatTarget.Get() == Actor && GetWorld())
		{
			TargetLostTime = GetWorld()->GetTimeSeconds();
		}
		Entry->bVisible = Aware.bCurrentlySensed;
		Entry->LastKnownLocation = Aware.LastSensedLocation;
		Entry->LastSensedTime = Aware.LastSensedTime;
		SeenThisRefresh.Add(Actor);
	}

	Threats.RemoveAll([&SeenThisRefresh](const FDMRememberedThreat& Entry)
	{
		return !IsValid(Entry.Actor) || !SeenThisRefresh.Contains(Entry.Actor);
	});
	UpdateThreatAwareness();
}

void UDMCombatKnowledgeComponent::UpdateThreatAwareness()
{
	if (CombatTarget.IsValid())
	{
		const FDMRememberedThreat* TargetRecord = FindThreat(CombatTarget.Get());
		ThreatAwareness = TargetRecord && TargetRecord->bVisible
			? EDMThreatAwareness::Visible
			: TargetRecord ? EDMThreatAwareness::LostGrace : EDMThreatAwareness::Disengaged;
	}
	else
	{
		ThreatAwareness = Threats.IsEmpty() ? EDMThreatAwareness::Disengaged : EDMThreatAwareness::WatchingLastKnown;
	}
}

void UDMCombatKnowledgeComponent::SetCombatTarget(AActor* NewTarget)
{
	CombatTarget = NewTarget;
	UpdateThreatAwareness();
}

void UDMCombatKnowledgeComponent::ClearCombatTarget()
{
	CombatTarget.Reset();
	ThreatAwareness = Threats.IsEmpty() ? EDMThreatAwareness::Disengaged : EDMThreatAwareness::WatchingLastKnown;
}

void UDMCombatKnowledgeComponent::NotifyDamageReceived(const FDMCombatDamageStimulus& Stimulus)
{
	RefreshFromPerception();
	LastDamageOrigin = Stimulus.bHasReliableOrigin ? Stimulus.DamageOrigin : FVector::ZeroVector;
	LastDamageDirection = Stimulus.DamageDirection;
	LastDamageTime = Stimulus.DamageTime > -BIG_NUMBER ? Stimulus.DamageTime
		: GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastDamageAwarenessType = Stimulus.AwarenessType;
	if (LastDamageAwarenessType == EDMDamageAwarenessType::Unknown && Stimulus.Attacker.IsValid())
	{
		const FDMRememberedThreat* Entry = FindThreat(Stimulus.Attacker.Get());
		LastDamageAwarenessType = Entry && Entry->bVisible
			? EDMDamageAwarenessType::Expected : EDMDamageAwarenessType::Surprise;
	}
}

void UDMCombatKnowledgeComponent::ResetKnowledge()
{
	CombatTarget.Reset();
	Threats.Reset();
	LastDamageOrigin = FVector::ZeroVector;
	LastDamageDirection = FVector::ZeroVector;
	LastDamageTime = TargetLostTime = -BIG_NUMBER;
	LastDamageAwarenessType = EDMDamageAwarenessType::Unknown;
	ThreatAwareness = EDMThreatAwareness::Disengaged;
}

bool UDMCombatKnowledgeComponent::HasVisibleTarget() const
{
	return Threats.ContainsByPredicate([](const FDMRememberedThreat& Entry)
	{
		return Entry.bVisible && IsValid(Entry.Actor);
	});
}

bool UDMCombatKnowledgeComponent::HasRememberedTarget() const
{
	return Threats.ContainsByPredicate([](const FDMRememberedThreat& Entry)
	{
		return IsValid(Entry.Actor);
	});
}

bool UDMCombatKnowledgeComponent::IsCombatTargetVisible() const
{
	const FDMRememberedThreat* Entry = FindThreat(CombatTarget.Get());
	return Entry && Entry->bVisible;
}

bool UDMCombatKnowledgeComponent::IsCombatTargetRemembered() const
{
	return CombatTarget.IsValid() && FindThreat(CombatTarget.Get()) != nullptr;
}

FVector UDMCombatKnowledgeComponent::GetLastKnownThreatLocation() const
{
	if (const FDMRememberedThreat* Entry = FindThreat(CombatTarget.Get()))
	{
		return Entry->LastKnownLocation;
	}
	return Threats.IsEmpty() ? FVector::ZeroVector : Threats[0].LastKnownLocation;
}

void UDMCombatKnowledgeComponent::GetVisibleThreatActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	for (const FDMRememberedThreat& Entry : Threats)
	{
		if (Entry.bVisible && IsValid(Entry.Actor))
		{
			OutActors.Add(Entry.Actor);
		}
	}
}

void UDMCombatKnowledgeComponent::GetRememberedThreatLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	for (const FDMRememberedThreat& Entry : Threats)
	{
		if (IsValid(Entry.Actor))
		{
			OutLocations.Add(Entry.LastKnownLocation);
		}
	}
}
