#include "DMTacticalAreaVolume.h"

#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADMTacticalAreaVolume::ADMTacticalAreaVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bGenerateOverlapEventsDuringLevelStreaming = true;

	AreaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AreaBounds"));
	SetRootComponent(AreaBounds);
	AreaBounds->SetBoxExtent(FVector(500.f, 500.f, 250.f));
	AreaBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AreaBounds->SetCollisionObjectType(ECC_WorldDynamic);
	AreaBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AreaBounds->SetGenerateOverlapEvents(true);
	AreaBounds->CanCharacterStepUpOn = ECB_No;
	AreaBounds->SetHiddenInGame(true);
	AreaBounds->SetCanEverAffectNavigation(false);
	AreaBounds->ShapeColor = FColor(26, 191, 255);
	AreaBounds->bDrawOnlyIfSelected = false;
	AreaBounds->OnComponentBeginOverlap.AddDynamic(this, &ADMTacticalAreaVolume::HandleAreaBeginOverlap);
	AreaBounds->OnComponentEndOverlap.AddDynamic(this, &ADMTacticalAreaVolume::HandleAreaEndOverlap);
}

void ADMTacticalAreaVolume::BeginPlay()
{
	Super::BeginPlay();
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->RegisterVolume(this);
	}
}

void ADMTacticalAreaVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->UnregisterVolume(this);
	}
	OverlappingComponentsByActor.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADMTacticalAreaVolume::HandleAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	if (!IsTrackedPawn(OtherActor) || !IsValid(OtherComponent)) return;

	for (auto It = OverlappingComponentsByActor.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	TSet<TWeakObjectPtr<UPrimitiveComponent>>& Components = OverlappingComponentsByActor.FindOrAdd(OtherActor);
	const bool bWasAlreadyOverlapping = !Components.IsEmpty();
	Components.Add(OtherComponent);
	if (!bWasAlreadyOverlapping)
	{
		if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
		{
			Areas->NotifyVolumeEntered(this, OtherActor);
		}
	}
}

void ADMTacticalAreaVolume::HandleAreaEndOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherBodyIndex;
	if (!OtherActor) return;

	TSet<TWeakObjectPtr<UPrimitiveComponent>>* Components = OverlappingComponentsByActor.Find(OtherActor);
	if (!Components) return;
	Components->Remove(OtherComponent);
	for (auto It = Components->CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}
	if (Components->IsEmpty())
	{
		OverlappingComponentsByActor.Remove(OtherActor);
		if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
		{
			Areas->NotifyVolumeExited(this, OtherActor);
		}
	}
}

bool ADMTacticalAreaVolume::IsTrackedPawn(const AActor* Actor) const
{
	return Cast<APawn>(Actor)
		&& DeathMatchTeamUtils::GetTeamIdFromActor(Actor) != FGenericTeamId::NoTeam;
}

FString ADMTacticalAreaVolume::GetAreaDisplayName() const
{
	return DebugDisplayName.IsEmpty() ? GetAreaId().ToString() : DebugDisplayName.ToString();
}

bool ADMTacticalAreaVolume::IsPointInsideVolume(const FVector& Point) const
{
	if (!AreaBounds) return false;
	const FVector LocalPoint = AreaBounds->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = AreaBounds->GetUnscaledBoxExtent();
	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}

FVector ADMTacticalAreaVolume::GetRepresentativeLocation() const
{
	return AreaBounds ? AreaBounds->GetComponentLocation() : GetActorLocation();
}

float ADMTacticalAreaVolume::GetSearchRadius() const
{
	const FVector Extent = GetScaledAreaExtent();
	return FMath::Max(100.f, FVector2D(Extent.X, Extent.Y).Size());
}

FVector ADMTacticalAreaVolume::GetScaledAreaExtent() const
{
	return AreaBounds ? AreaBounds->GetScaledBoxExtent() : FVector::ZeroVector;
}

FVector ADMTacticalAreaVolume::GetWorldBoundsExtent() const
{
	return AreaBounds ? AreaBounds->Bounds.BoxExtent : FVector::ZeroVector;
}

int32 ADMTacticalAreaVolume::GetEnemyCount(uint8 TeamId) const
{
	int32 Result = 0;
	for (const TPair<uint8, int32>& Pair : TeamOccupancy)
	{
		if (Pair.Key != TeamId)
		{
			Result += FMath::Max(0, Pair.Value);
		}
	}
	return Result;
}

float ADMTacticalAreaVolume::GetTeamBaseDanger(uint8 TeamId) const
{
	if (TeamId == 255) return 0.f;
	for (const uint8 SpawnTeam : SpawnTeams)
	{
		if (SpawnTeam != 255 && SpawnTeam != TeamId) return 10.f;
	}
	return 0.f;
}

float ADMTacticalAreaVolume::GetTeamDynamicDanger(uint8 TeamId) const
{
	return TeamId == 255 ? 0.f : FMath::Clamp(TeamDynamicDanger.FindRef(TeamId), 0.f, 10.f);
}

float ADMTacticalAreaVolume::GetTeamDanger(uint8 TeamId) const
{
	return FMath::Clamp(FMath::Max(GetTeamBaseDanger(TeamId), GetTeamDynamicDanger(TeamId)), 0.f, 10.f);
}

int32 ADMTacticalAreaVolume::GetTotalIncomingAICount() const
{
	int32 Result = 0;
	for (const TPair<uint8, int32>& Pair : TeamIncomingAI)
	{
		Result += FMath::Max(0, Pair.Value);
	}
	return Result;
}

void ADMTacticalAreaVolume::RegisterSpawnTeam(uint8 TeamId)
{
	if (TeamId != 255) SpawnTeams.Add(TeamId);
}

void ADMTacticalAreaVolume::AddDanger(uint8 TeamId, float Amount)
{
	if (TeamId == 255 || !FMath::IsFinite(Amount) || FMath::IsNearlyZero(Amount))
	{
		return;
	}
	const float NewDanger = FMath::Clamp(TeamDynamicDanger.FindRef(TeamId) + Amount, 0.f, 10.f);
	if (NewDanger <= 0.f) TeamDynamicDanger.Remove(TeamId);
	else TeamDynamicDanger.FindOrAdd(TeamId) = NewDanger;
}

void ADMTacticalAreaVolume::AdjustOccupancy(uint8 TeamId, int32 Delta)
{
	if (TeamId == 255 || Delta == 0) return;
	int32& Count = TeamOccupancy.FindOrAdd(TeamId);
	Count = FMath::Max(0, Count + Delta);
	if (Count == 0) TeamOccupancy.Remove(TeamId);
}

void ADMTacticalAreaVolume::AdjustIncoming(uint8 TeamId, int32 Delta)
{
	if (TeamId == 255 || Delta == 0) return;
	int32& Count = TeamIncomingAI.FindOrAdd(TeamId);
	Count = FMath::Max(0, Count + Delta);
	if (Count == 0) TeamIncomingAI.Remove(TeamId);
}

void ADMTacticalAreaVolume::AdjustCombatParticipants(int32 Delta)
{
	CurrentCombatParticipantCount = FMath::Max(0, CurrentCombatParticipantCount + Delta);
}
