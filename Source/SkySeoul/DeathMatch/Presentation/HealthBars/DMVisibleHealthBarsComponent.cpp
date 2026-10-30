#include "DMVisibleHealthBarsComponent.h"

#include "DeathMatch/Presentation/HealthBars/DMHealthBarProviders.h"
#include "DeathMatch/Core/DMHealthResolver.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

UDMVisibleHealthBarsComponent::UDMVisibleHealthBarsComponent()
{
	PrimaryComponentTick.TickInterval = 0.0f;
	DisplayOptions.bShowActorName = false;
	DisplayOptions.bShowNumericHealth = true;
	DisplayOptions.bShowHealthSource = false;
}

void UDMVisibleHealthBarsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* Viewer = ResolveLocalViewerController();
	UWorld* World = GetWorld();
	if (!Viewer || !World)
	{
		HideAllTargets();
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now >= NextScanTime)
	{
		ScanCandidates(Viewer);
		NextScanTime = Now + FMath::Max(0.05f, ScanInterval);
	}
	if (Now >= NextVisibilityCheckTime)
	{
		EvaluateCandidates(Viewer, Now);
		NextVisibilityCheckTime = Now + FMath::Max(0.02f, VisibilityCheckInterval);
	}
}

void UDMVisibleHealthBarsComponent::SetPublicHealthProvider(UObject* Provider)
{
	PublicHealthProvider = Provider && Provider->GetClass()->ImplementsInterface(UDMPublicHealthProvider::StaticClass()) ? Provider : nullptr;
}

void UDMVisibleHealthBarsComponent::SetAuthorizationProvider(UObject* Provider)
{
	AuthorizationProvider = Provider && Provider->GetClass()->ImplementsInterface(UDMHealthBarAuthorizationProvider::StaticClass()) ? Provider : nullptr;
}

void UDMVisibleHealthBarsComponent::ScanCandidates(const APlayerController* Viewer)
{
	Candidates.Reset();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Target = *It;
		if (!IsValid(Target) || Target == GetOwner() || (!bShowSelf && Target == Viewer->GetPawn()))
		{
			continue;
		}
		FDMVisibleHealthCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Target = Target;
	}
	CandidateCount = Candidates.Num();
}

void UDMVisibleHealthBarsComponent::EvaluateCandidates(APlayerController* Viewer, float Now)
{
	BeginDisplayPass();
	OnScreenCount = 0;
	LOSVisibleCount = 0;
	AuthorizedHealthCount = 0;

	const APlayerCameraManager* CameraManager = Viewer->PlayerCameraManager;
	if (!CameraManager)
	{
		EndDisplayPass(Now, FMath::Max(1.0f, ScanInterval * 3.0f));
		return;
	}
	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector CameraForward = CameraManager->GetCameraRotation().Vector();

	for (const FDMVisibleHealthCandidate& Candidate : Candidates)
	{
		AActor* Target = Candidate.Target.Get();
		if (!IsValid(Target) || !CanDisplayCandidate(Viewer, Target))
		{
			continue;
		}
		if (FVector::DistSquared(CameraLocation, Target->GetActorLocation()) > FMath::Square(FMath::Max(100.0f, MaximumDisplayDistance)))
		{
			continue;
		}
		if (!IsOnScreen(Viewer, Target, CameraLocation, CameraForward))
		{
			continue;
		}
		++OnScreenCount;

		if (bRequireLineOfSight && !HasLineOfSight(Viewer, Target, CameraLocation))
		{
			const FDMHealthBarTargetEntry* Existing = FindTargetEntry(Target);
			if (Existing && Existing->bCurrentlyDisplayed && Now - Existing->LastVisibleTime <= FMath::Clamp(VisibilityGraceSeconds, 0.0f, 0.5f))
			{
				ShowTargetUsingCachedSnapshot(Target, Now, false);
			}
			continue;
		}
		++LOSVisibleCount;

		FDMHealthSnapshot Snapshot;
		if (!IsHealthRefreshDue(Target, Now))
		{
			if (const FDMHealthBarTargetEntry* Existing = FindTargetEntry(Target))
			{
				Snapshot = Existing->LastHealthSnapshot;
			}
		}
		else if (ResolveAuthorizedSnapshot(Viewer, Target, Snapshot))
		{
			SetNextHealthRefresh(Target, Now + FMath::Max(0.02f, HealthUpdateInterval));
		}

		if (!Snapshot.bValid || (!bShowDead && !Snapshot.bAlive))
		{
			continue;
		}
		++AuthorizedHealthCount;
		ShowTarget(Target, Snapshot, Now, true);
		SetNextHealthRefresh(Target, Now + FMath::Max(0.02f, HealthUpdateInterval));
	}

	EndDisplayPass(Now, FMath::Max(1.0f, ScanInterval * 3.0f));
}

bool UDMVisibleHealthBarsComponent::CanDisplayCandidate(const APlayerController* Viewer, const AActor* Target) const
{
	if (UObject* Provider = ResolveAuthorizationProvider(Viewer))
	{
		return IDMHealthBarAuthorizationProvider::Execute_CanDisplayHealthBar(Provider, Viewer->GetPawn(), Target);
	}
	if (bRequireAuthorizationProvider)
	{
		return false;
	}

	const FGenericTeamId ViewerTeam = DeathMatchTeamUtils::GetTeamIdFromActor(Viewer->GetPawn());
	const ETeamAttitude::Type Attitude = DeathMatchTeamUtils::GetAttitudeTowards(ViewerTeam, *Target);
	switch (Attitude)
	{
	case ETeamAttitude::Friendly: return bShowFriendlies;
	case ETeamAttitude::Hostile: return bShowHostiles;
	default: return bShowNeutral;
	}
}

bool UDMVisibleHealthBarsComponent::CanRevealCandidate(const APlayerController* Viewer, const AActor* Target) const
{
	if (UObject* Provider = ResolveAuthorizationProvider(Viewer))
	{
		return IDMHealthBarAuthorizationProvider::Execute_CanRevealHealth(Provider, Viewer->GetPawn(), Target);
	}
	return !bRequireAuthorizationProvider;
}

bool UDMVisibleHealthBarsComponent::IsOnScreen(APlayerController* Viewer, const AActor* Target, const FVector& CameraLocation, const FVector& CameraForward) const
{
	const FVector Anchor = DMHealthBars::ResolveAnchorLocation(Target, HealthBarSocketName, HeightOffset);
	const FVector Direction = Anchor - CameraLocation;
	if (FVector::DotProduct(CameraForward, Direction) <= 0.0f)
	{
		return false;
	}

	FVector2D ScreenPosition;
	if (!Viewer->ProjectWorldLocationToScreen(Anchor, ScreenPosition, true))
	{
		return false;
	}
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Viewer->GetViewportSize(ViewportWidth, ViewportHeight);
	return ViewportWidth > 0 && ViewportHeight > 0
		&& ScreenPosition.X >= 0.0f && ScreenPosition.Y >= 0.0f
		&& ScreenPosition.X <= ViewportWidth && ScreenPosition.Y <= ViewportHeight;
}

bool UDMVisibleHealthBarsComponent::HasLineOfSight(const APlayerController* Viewer, const AActor* Target, const FVector& CameraLocation) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMVisibleHealthBarLOS), true);
	QueryParams.AddIgnoredActor(GetOwner());
	if (Viewer->GetPawn())
	{
		QueryParams.AddIgnoredActor(Viewer->GetPawn());
		TArray<AActor*> AttachedActors;
		Viewer->GetPawn()->GetAttachedActors(AttachedActors, true, true);
		QueryParams.AddIgnoredActors(AttachedActors);
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		CameraLocation,
		DMHealthBars::ResolveLineOfSightLocation(Target),
		ECC_Visibility,
		QueryParams);
	if (!bHit)
	{
		return true;
	}
	const AActor* HitActor = Hit.GetActor();
	return HitActor == Target || (HitActor && HitActor->IsOwnedBy(Target));
}

bool UDMVisibleHealthBarsComponent::ResolveAuthorizedSnapshot(const APlayerController* Viewer, const AActor* Target, FDMHealthSnapshot& OutSnapshot) const
{
	OutSnapshot = FDMHealthSnapshot();
	if (!CanRevealCandidate(Viewer, Target))
	{
		return false;
	}

	if (UObject* Provider = ResolvePublicHealthProvider(Viewer))
	{
		FDMHealthSnapshot PublicSnapshot;
		if (IDMPublicHealthProvider::Execute_GetPublicHealthSnapshot(Provider, Viewer->GetPawn(), Target, PublicSnapshot) && PublicSnapshot.bValid)
		{
			OutSnapshot = DMHealth::MakeSnapshot(PublicSnapshot.CurrentHealth, PublicSnapshot.MaxHealth, PublicSnapshot.bAlive, EDMHealthSource::PublicData);
			return OutSnapshot.bValid;
		}
		return false;
	}

	const bool bSelf = Target == Viewer->GetPawn();
	if (bSelf && bAllowRawSelfHealthFallback)
	{
		OutSnapshot = DMHealth::Resolve(Target);
		return OutSnapshot.bValid;
	}

	const ENetMode NetMode = GetWorld()->GetNetMode();
	const bool bLocalAuthorityEnvironment = NetMode == NM_Standalone || NetMode == NM_ListenServer;
	if (bAllowStandaloneOrAuthorityResolverFallback && bLocalAuthorityEnvironment)
	{
		OutSnapshot = DMHealth::Resolve(Target);
		return OutSnapshot.bValid;
	}

	// Remote clients never fall back to another pawn's raw CurHP/GAS values.
	return false;
}

UObject* UDMVisibleHealthBarsComponent::ResolvePublicHealthProvider(const APlayerController* Viewer) const
{
	if (PublicHealthProvider && PublicHealthProvider->GetClass()->ImplementsInterface(UDMPublicHealthProvider::StaticClass()))
	{
		return PublicHealthProvider;
	}
	return FindInterfaceProvider(Viewer, GetOwner(), UDMPublicHealthProvider::StaticClass());
}

UObject* UDMVisibleHealthBarsComponent::ResolveAuthorizationProvider(const APlayerController* Viewer) const
{
	if (AuthorizationProvider && AuthorizationProvider->GetClass()->ImplementsInterface(UDMHealthBarAuthorizationProvider::StaticClass()))
	{
		return AuthorizationProvider;
	}
	return FindInterfaceProvider(Viewer, GetOwner(), UDMHealthBarAuthorizationProvider::StaticClass());
}

UObject* UDMVisibleHealthBarsComponent::FindInterfaceProvider(const APlayerController* Viewer, const AActor* Owner, UClass* InterfaceClass)
{
	UObject* CandidatesToCheck[] = {
		const_cast<AActor*>(Owner),
		const_cast<APlayerController*>(Viewer),
		Viewer ? Viewer->GetPawn() : nullptr,
		Viewer && Viewer->GetWorld() ? Viewer->GetWorld()->GetGameState() : nullptr
	};
	for (UObject* Candidate : CandidatesToCheck)
	{
		if (Candidate && Candidate->GetClass()->ImplementsInterface(InterfaceClass))
		{
			return Candidate;
		}
	}
	return nullptr;
}

