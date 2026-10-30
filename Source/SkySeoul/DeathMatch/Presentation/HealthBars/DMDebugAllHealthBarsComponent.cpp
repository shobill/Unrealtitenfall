#include "DMDebugAllHealthBarsComponent.h"

#include "DeathMatch/Core/DMHealthResolver.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UDMDebugAllHealthBarsComponent::UDMDebugAllHealthBarsComponent()
{
	PrimaryComponentTick.TickInterval = 0.0f;
	DisplayOptions.bShowActorName = true;
	DisplayOptions.bShowNumericHealth = true;
	DisplayOptions.bShowHealthSource = true;
	DrawSize = FIntPoint(160, 38);
}

void UDMDebugAllHealthBarsComponent::BeginPlay()
{
	Super::BeginPlay();
#if UE_BUILD_SHIPPING
	bEnabled = false;
	SetComponentTickEnabled(false);
	Candidates.Reset();
	HideAllTargets();
#endif
}

void UDMDebugAllHealthBarsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if UE_BUILD_SHIPPING
	return;
#else
	if (!bEnabled)
	{
		HideAllTargets();
		return;
	}

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
		ScanCharacters(Now);
		NextScanTime = Now + FMath::Max(0.05f, ScanInterval);
	}

	BeginDisplayPass();
	InvalidHealthCount = InvalidFromLastScan;
	const FVector ViewerLocation = Viewer->PlayerCameraManager
		? Viewer->PlayerCameraManager->GetCameraLocation()
		: Viewer->GetFocalLocation();

	for (FDMDebugHealthCandidate& Candidate : Candidates)
	{
		AActor* Target = Candidate.Target.Get();
		if (!ShouldIncludeTarget(Target, Viewer))
		{
			continue;
		}
		if (MaximumDisplayDistance > 0.0f && FVector::DistSquared(ViewerLocation, Target->GetActorLocation()) > FMath::Square(MaximumDisplayDistance))
		{
			continue;
		}
		if (Now >= Candidate.NextHealthUpdateTime)
		{
			Candidate.Snapshot = DMHealth::Resolve(Target);
			Candidate.NextHealthUpdateTime = Now + FMath::Max(0.02f, HealthUpdateInterval);
		}
		if (!Candidate.Snapshot.bValid)
		{
			++InvalidHealthCount;
			continue;
		}
		if (!bShowDead && !Candidate.Snapshot.bAlive)
		{
			continue;
		}
		ShowTarget(Target, Candidate.Snapshot, Now);
		SetNextHealthRefresh(Target, Candidate.NextHealthUpdateTime);
	}

	EndDisplayPass(Now, FMath::Max(1.0f, ScanInterval * 3.0f));
#endif
}

void UDMDebugAllHealthBarsComponent::SetDebugHealthBarsEnabled(bool bNewEnabled)
{
#if UE_BUILD_SHIPPING
	bEnabled = false;
	SetComponentTickEnabled(false);
	HideAllTargets();
#else
	bEnabled = bNewEnabled;
	SetComponentTickEnabled(true);
	if (!bEnabled)
	{
		HideAllTargets();
	}
#endif
}

bool UDMDebugAllHealthBarsComponent::AreDebugHealthBarsEnabled() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bEnabled;
#endif
}

bool UDMDebugAllHealthBarsComponent::IsDebugFeatureAvailable()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

void UDMDebugAllHealthBarsComponent::ScanCharacters(float Now)
{
#if UE_BUILD_SHIPPING
	return;
#else
	Candidates.Reset();
	InvalidFromLastScan = 0;
	APlayerController* Viewer = ResolveLocalViewerController();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Target = *It;
		if (!ShouldIncludeTarget(Target, Viewer))
		{
			continue;
		}
		const FDMHealthSnapshot Snapshot = DMHealth::Resolve(Target);
		if (!Snapshot.bValid)
		{
			++InvalidFromLastScan;
			continue;
		}
		FDMDebugHealthCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Target = Target;
		Candidate.Snapshot = Snapshot;
		Candidate.NextHealthUpdateTime = Now + FMath::Max(0.02f, HealthUpdateInterval);
	}
	TrackedTargetCount = Candidates.Num();
	InvalidHealthCount = InvalidFromLastScan;
#endif
}

bool UDMDebugAllHealthBarsComponent::ShouldIncludeTarget(const AActor* Target, const APlayerController* Viewer) const
{
	if (!IsValid(Target))
	{
		return false;
	}
	if (Target == GetOwner())
	{
		return false;
	}
	if (!bShowSelf && Viewer && Target == Viewer->GetPawn())
	{
		return false;
	}
	return true;
}

