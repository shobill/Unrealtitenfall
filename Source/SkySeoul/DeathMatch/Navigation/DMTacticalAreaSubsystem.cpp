#include "DMTacticalAreaSubsystem.h"

#include "DeathMatch/Core/DeathMatchLog.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "DeathMatch/PlayerStart/PlayerStartByTeam.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UDMTacticalAreaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RefreshSpawnTeams();
}

void UDMTacticalAreaSubsystem::Deinitialize()
{
	TArray<TWeakObjectPtr<AActor>> TrackedActors;
	PresenceByActor.GetKeys(TrackedActors);
	for (const TPair<TWeakObjectPtr<AActor>, FReservationRecord>& Pair : ReservationByActor)
	{
		TrackedActors.AddUnique(Pair.Key);
	}
	for (const TWeakObjectPtr<AActor>& Actor : TrackedActors)
	{
		if (Actor.IsValid())
		{
			Actor->OnDestroyed.RemoveDynamic(this, &UDMTacticalAreaSubsystem::HandleTrackedActorDestroyed);
		}
	}
	PresenceByActor.Reset();
	ReservationByActor.Reset();
	RegisteredVolumes.Reset();
	Super::Deinitialize();
}

void UDMTacticalAreaSubsystem::RegisterVolume(ADMTacticalAreaVolume* Volume)
{
	if (!IsValid(Volume)) return;
	RegisteredVolumes.RemoveAll([](const TWeakObjectPtr<ADMTacticalAreaVolume>& Item) { return !Item.IsValid(); });
	if (RegisteredVolumes.Contains(Volume)) return;
	for (const TWeakObjectPtr<ADMTacticalAreaVolume>& Existing : RegisteredVolumes)
	{
		if (Existing.IsValid() && Existing->GetAreaId() == Volume->GetAreaId())
		{
			UE_LOG(LogDM, Warning, TEXT("DM TacticalArea: duplicate AreaId '%s' (%s, %s)"),
				*Volume->GetAreaId().ToString(), *GetNameSafe(Existing.Get()), *GetNameSafe(Volume));
		}
	}
	RegisteredVolumes.Add(Volume);
	// Handles streamed/late-created areas without relying on actor BeginPlay order.
	RefreshSpawnTeams();
}

void UDMTacticalAreaSubsystem::UnregisterVolume(ADMTacticalAreaVolume* Volume)
{
	if (!Volume) return;
	RegisteredVolumes.Remove(Volume);
	for (auto It = PresenceByActor.CreateIterator(); It; ++It)
	{
		if (It.Value().Volume.Get() == Volume)
		{
			AActor* Actor = It.Key().Get();
			It.RemoveCurrent();
			StopTrackingActorDestruction(Actor);
		}
	}
	for (auto It = ReservationByActor.CreateIterator(); It; ++It)
	{
		if (It.Value().Volume.Get() == Volume)
		{
			AActor* Actor = It.Key().Get();
			It.RemoveCurrent();
			StopTrackingActorDestruction(Actor);
		}
	}
}

void UDMTacticalAreaSubsystem::GetRegisteredVolumes(
	TArray<ADMTacticalAreaVolume*>& OutVolumes, bool bActiveOnly) const
{
	OutVolumes.Reset();
	for (const TWeakObjectPtr<ADMTacticalAreaVolume>& Item : RegisteredVolumes)
	{
		ADMTacticalAreaVolume* Volume = Item.Get();
		if (Volume && (!bActiveOnly || Volume->IsAreaEnabled()))
		{
			OutVolumes.Add(Volume);
		}
	}
}

ADMTacticalAreaVolume* UDMTacticalAreaSubsystem::FindVolumeAtLocation(
	const FVector& Location, bool bActiveOnly) const
{
	ADMTacticalAreaVolume* Best = nullptr;
	double BestVolume = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<ADMTacticalAreaVolume>& Item : RegisteredVolumes)
	{
		ADMTacticalAreaVolume* Volume = Item.Get();
		if (!Volume || (bActiveOnly && !Volume->IsAreaEnabled()) || !Volume->IsPointInsideVolume(Location))
		{
			continue;
		}
		const FVector Size = Volume->GetScaledAreaExtent() * 2.f;
		const double BoundsVolume = static_cast<double>(Size.X) * Size.Y * Size.Z;
		const bool bSmallerVolume = BoundsVolume < BestVolume;
		const bool bStableTieBreak = Best && FMath::IsNearlyEqual(BoundsVolume, BestVolume)
			&& Volume->GetPathName().Compare(Best->GetPathName(), ESearchCase::CaseSensitive) < 0;
		if (!Best || bSmallerVolume || bStableTieBreak)
		{
			Best = Volume;
			BestVolume = BoundsVolume;
		}
	}
	return Best;
}

void UDMTacticalAreaSubsystem::RegisterSpawnTeamAtLocation(uint8 TeamId, const FVector& SpawnLocation)
{
	if (TeamId == 255) return;
	if (ADMTacticalAreaVolume* Volume = FindVolumeAtLocation(SpawnLocation, false))
	{
		Volume->RegisterSpawnTeam(TeamId);
	}
}

void UDMTacticalAreaSubsystem::RefreshSpawnTeams()
{
	UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<APlayerStartByTeam> It(World); It; ++It)
	{
		if (It->TeamId != 255) RegisterSpawnTeamAtLocation(It->TeamId, It->GetActorLocation());
	}
}

void UDMTacticalAreaSubsystem::NotifyVolumeEntered(ADMTacticalAreaVolume* Volume, AActor* Actor)
{
	if (!IsValid(Volume) || !IsValid(Actor)) return;
	const uint8 TeamId = DeathMatchTeamUtils::GetTeamIdFromActor(Actor).GetId();
	if (TeamId != 255) SetPresence(Actor, TeamId, Volume);
}

void UDMTacticalAreaSubsystem::NotifyVolumeExited(ADMTacticalAreaVolume* Volume, AActor* Actor)
{
	if (!Volume || !Actor) return;
	const FPresenceRecord* Existing = PresenceByActor.Find(Actor);
	if (!Existing || Existing->Volume.Get() != Volume) return;
	ADMTacticalAreaVolume* Replacement = FindVolumeAtLocation(Actor->GetActorLocation(), false);
	SetPresence(Actor, Existing->TeamId, Replacement == Volume ? nullptr : Replacement);
}

void UDMTacticalAreaSubsystem::ReportCharacterSpawn(AActor* Character, uint8 TeamId)
{
	if (!IsValid(Character) || TeamId == 255) return;
	// Confirmed DeathMatch spawn fallback. Authored PlayerStartByTeam actors are the primary source.
	RegisterSpawnTeamAtLocation(TeamId, Character->GetActorLocation());
	SetPresence(Character, TeamId, FindVolumeAtLocation(Character->GetActorLocation(), false));
}

void UDMTacticalAreaSubsystem::ReportCharacterDeath(AActor* Character, uint8 TeamId, float DangerAmount)
{
	if (!Character) return;
	ApplyDanger(TeamId, Character->GetActorLocation(), DangerAmount);
	RemoveCharacter(Character);
}

void UDMTacticalAreaSubsystem::RemoveCharacter(AActor* Character)
{
	if (!Character) return;
	ReleaseIncomingAI(Character);
	RoamFailureCooldowns.Remove(Character);
	RecentRoamAreaByActor.Remove(Character);
	if (FPresenceRecord* Existing = PresenceByActor.Find(Character))
	{
		if (ADMTacticalAreaVolume* Volume = Existing->Volume.Get())
		{
			Volume->AdjustOccupancy(Existing->TeamId, -1);
			if (Existing->bInCombat) Volume->AdjustCombatParticipants(-1);
		}
		PresenceByActor.Remove(Character);
	}
	StopTrackingActorDestruction(Character);
}

bool UDMTacticalAreaSubsystem::IsRoamAreaOnCooldown(AActor* AIActor, ADMTacticalAreaVolume* Volume, float Now) const
{
	const TMap<TWeakObjectPtr<ADMTacticalAreaVolume>, float>* ByVolume = RoamFailureCooldowns.Find(AIActor);
	const float* Until = ByVolume ? ByVolume->Find(Volume) : nullptr;
	return Until && Now < *Until;
}

void UDMTacticalAreaSubsystem::RecordRoamAreaFailure(AActor* AIActor, ADMTacticalAreaVolume* Volume, float Now, float CooldownSeconds)
{
	if (!IsValid(AIActor) || !IsValid(Volume)) return;
	RoamFailureCooldowns.FindOrAdd(AIActor).FindOrAdd(Volume) = Now + FMath::Max(0.f, CooldownSeconds);
}

void UDMTacticalAreaSubsystem::ReportRoamAreaReached(AActor* AIActor, ADMTacticalAreaVolume* Volume)
{
	if (IsValid(AIActor) && IsValid(Volume)) RecentRoamAreaByActor.FindOrAdd(AIActor) = Volume;
}

ADMTacticalAreaVolume* UDMTacticalAreaSubsystem::GetRecentRoamArea(AActor* AIActor) const
{
	const TWeakObjectPtr<ADMTacticalAreaVolume>* Found = RecentRoamAreaByActor.Find(AIActor);
	return Found ? Found->Get() : nullptr;
}

void UDMTacticalAreaSubsystem::ReportCombatStart(AActor* Character, uint8 TeamId, float DangerAmount)
{
	if (!IsValid(Character) || TeamId == 255) return;
	ADMTacticalAreaVolume* Volume = FindVolumeAtLocation(Character->GetActorLocation(), false);
	SetPresence(Character, TeamId, Volume);
	if (!Volume) return;
	FPresenceRecord& Record = PresenceByActor.FindOrAdd(Character);
	if (!Record.bInCombat)
	{
		Record.bInCombat = true;
		if (Volume) Volume->AdjustCombatParticipants(1);
	}
	ApplyDanger(TeamId, Character->GetActorLocation(), DangerAmount);
}

void UDMTacticalAreaSubsystem::ReportCombatEnd(AActor* Character)
{
	if (!Character) return;
	if (FPresenceRecord* Record = PresenceByActor.Find(Character); Record && Record->bInCombat)
	{
		Record->bInCombat = false;
		if (ADMTacticalAreaVolume* Volume = Record->Volume.Get()) Volume->AdjustCombatParticipants(-1);
	}
}

void UDMTacticalAreaSubsystem::ReportEnemySeen(
	uint8 ObserverTeamId, const FVector& Location, float DangerAmount, AActor* Source)
{
	(void)Source;
	ApplyDanger(ObserverTeamId, Location, DangerAmount);
}

void UDMTacticalAreaSubsystem::ReportDamageReceived(
	uint8 VictimTeamId, const FVector& Location, float DangerAmount, AActor* Victim)
{
	(void)Victim;
	ApplyDanger(VictimTeamId, Location, DangerAmount);
}

void UDMTacticalAreaSubsystem::ReportSoundEvent(
	uint8 ObserverTeamId, const FVector& Location, float Strength, AActor* Source)
{
	// First-pass API only. Sound does not affect danger until an authored policy is added.
	(void)ObserverTeamId;
	(void)Location;
	(void)Strength;
	(void)Source;
}

bool UDMTacticalAreaSubsystem::ReserveIncomingAI(
	AActor* AIActor, uint8 TeamId, ADMTacticalAreaVolume* Volume)
{
	if (!IsValid(AIActor) || !IsValid(Volume) || !Volume->IsAreaEnabled() || TeamId == 255) return false;
	if (const FReservationRecord* Existing = ReservationByActor.Find(AIActor))
	{
		if (Existing->Volume.Get() == Volume && Existing->TeamId == TeamId) return true;
	}
	ReleaseIncomingAI(AIActor);
	FReservationRecord& Record = ReservationByActor.Add(AIActor);
	Record.Volume = Volume;
	Record.TeamId = TeamId;
	Volume->AdjustIncoming(TeamId, 1);
	TrackActorDestruction(AIActor);
	return true;
}

void UDMTacticalAreaSubsystem::ReleaseIncomingAI(AActor* AIActor)
{
	if (!AIActor) return;
	if (FReservationRecord* Existing = ReservationByActor.Find(AIActor))
	{
		if (ADMTacticalAreaVolume* Volume = Existing->Volume.Get())
		{
			Volume->AdjustIncoming(Existing->TeamId, -1);
		}
		ReservationByActor.Remove(AIActor);
	}
	StopTrackingActorDestruction(AIActor);
}

ADMTacticalAreaVolume* UDMTacticalAreaSubsystem::GetReservedVolume(AActor* AIActor) const
{
	const FReservationRecord* Record = AIActor ? ReservationByActor.Find(AIActor) : nullptr;
	return Record ? Record->Volume.Get() : nullptr;
}

void UDMTacticalAreaSubsystem::SetPresence(
	AActor* Actor, uint8 TeamId, ADMTacticalAreaVolume* Volume)
{
	if (!IsValid(Actor) || TeamId == 255) return;
	FPresenceRecord* Existing = PresenceByActor.Find(Actor);
	if (Existing && Existing->Volume.Get() == Volume && Existing->TeamId == TeamId) return;
	const bool bWasInCombat = Existing && Existing->bInCombat;
	if (Existing)
	{
		if (ADMTacticalAreaVolume* OldVolume = Existing->Volume.Get())
		{
			OldVolume->AdjustOccupancy(Existing->TeamId, -1);
			if (Existing->bInCombat) OldVolume->AdjustCombatParticipants(-1);
		}
	}
	if (!Volume)
	{
		PresenceByActor.Remove(Actor);
		StopTrackingActorDestruction(Actor);
		return;
	}
	FPresenceRecord& Record = PresenceByActor.FindOrAdd(Actor);
	Record.Volume = Volume;
	Record.TeamId = TeamId;
	Record.bInCombat = bWasInCombat;
	Volume->AdjustOccupancy(TeamId, 1);
	if (bWasInCombat) Volume->AdjustCombatParticipants(1);
	TrackActorDestruction(Actor);
}

void UDMTacticalAreaSubsystem::ApplyDanger(uint8 TeamId, const FVector& Location, float Amount)
{
	if (TeamId == 255 || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
	if (ADMTacticalAreaVolume* Volume = FindVolumeAtLocation(Location, false))
	{
		Volume->AddDanger(TeamId, Amount);
	}
}

void UDMTacticalAreaSubsystem::TrackActorDestruction(AActor* Actor)
{
	if (IsValid(Actor)) Actor->OnDestroyed.AddUniqueDynamic(this, &UDMTacticalAreaSubsystem::HandleTrackedActorDestroyed);
}

void UDMTacticalAreaSubsystem::StopTrackingActorDestruction(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	if (PresenceByActor.Contains(Actor) || ReservationByActor.Contains(Actor)) return;
	Actor->OnDestroyed.RemoveDynamic(this, &UDMTacticalAreaSubsystem::HandleTrackedActorDestroyed);
}

void UDMTacticalAreaSubsystem::HandleTrackedActorDestroyed(AActor* DestroyedActor)
{
	RemoveCharacter(DestroyedActor);
}
