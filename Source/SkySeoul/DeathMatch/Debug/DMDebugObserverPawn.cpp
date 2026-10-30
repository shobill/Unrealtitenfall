#include "DMDebugObserverPawn.h"

#include "Character/CharacterBase.h"
#include "DeathMatch/Core/DMHealthResolver.h"
#include "DeathMatch/Navigation/DMNavigationSafety.h"
#include "DeathMatch/Presentation/HealthBars/DMDebugAllHealthBarsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "PartsCharacter/PartsHitCollision.h"
#include "Perception/AIPerceptionSystem.h"
#include "UObject/ConstructorHelpers.h"

ADMDebugObserverPawn::ADMDebugObserverPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	SetReplicates(false);
	SetReplicateMovement(false);
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ObserverCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ObserverCamera"));
	ObserverCamera->SetupAttachment(SceneRoot);
	ObserverCamera->bUsePawnControlRotation = true;

	ObserverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObserverMesh"));
	ObserverMesh->SetupAttachment(SceneRoot);
	ObserverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ObserverMesh->SetGenerateOverlapEvents(false);
	ObserverMesh->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		ObserverMesh->SetStaticMesh(SphereMesh.Object);
	}

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	FloatingMovement->UpdatedComponent = SceneRoot;
	FloatingMovement->Acceleration = 12000.0f;
	FloatingMovement->Deceleration = 12000.0f;
	FloatingMovement->MaxSpeed = MoveSpeed;

	DebugHealthBars = CreateDefaultSubobject<UDMDebugAllHealthBarsComponent>(TEXT("DebugAllHealthBars"));
}

void ADMDebugObserverPawn::BeginPlay()
{
	Super::BeginPlay();
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	ResetOrbitState();

	// Sight auto-registers every Pawn by default. Remove this observer from all
	// instantiated senses as well as any pending source registrations.
	if (UWorld* World = GetWorld())
	{
		if (UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(*World))
		{
			PerceptionSystem->UnregisterSource(*this);
		}
	}

	if (DebugHealthBars)
	{
		DebugHealthBars->SetDebugHealthBarsEnabled(bStartWithDebugHealthBarsEnabled);
	}
	if (ObserverMesh)
	{
		ObserverMesh->SetHiddenInGame(!bShowObserverMesh);
	}
}

void ADMDebugObserverPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FollowTarget.Reset();
	FollowCandidates.Reset();
	bFollowTarget = false;
	FollowTargetIndex = INDEX_NONE;
	Super::EndPlay(EndPlayReason);
}

void ADMDebugObserverPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (ObserverMesh)
	{
		ObserverMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, ObserverMeshScale)));
		ObserverMesh->SetVisibility(bShowObserverMesh, true);
		ObserverMesh->SetHiddenInGame(!bShowObserverMesh);
	}
}

void ADMDebugObserverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController() || !FloatingMovement)
	{
		return;
	}

	if (PlayerController->WasInputKeyJustPressed(EKeys::F))
	{
		ToggleFollowTarget();
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::PageUp) || PlayerController->WasInputKeyJustPressed(EKeys::RightBracket))
	{
		FollowNextCharacter();
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::PageDown) || PlayerController->WasInputKeyJustPressed(EKeys::LeftBracket))
	{
		FollowPreviousCharacter();
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::R))
	{
		FollowNearestCharacter();
	}

	const bool bFast = PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift);
	const float CurrentMoveSpeed = bFast ? FMath::Max(MoveSpeed, FastMoveSpeed) : MoveSpeed;
	FloatingMovement->MaxSpeed = CurrentMoveSpeed;

	float ForwardInput = 0.0f;
	float RightInput = 0.0f;
	float UpInput = 0.0f;
	ForwardInput += PlayerController->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f;
	ForwardInput -= PlayerController->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f;
	RightInput += PlayerController->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f;
	RightInput -= PlayerController->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f;
	UpInput += PlayerController->IsInputKeyDown(EKeys::E) || PlayerController->IsInputKeyDown(EKeys::SpaceBar) ? 1.0f : 0.0f;
	UpInput -= PlayerController->IsInputKeyDown(EKeys::Q) || PlayerController->IsInputKeyDown(EKeys::LeftControl) ? 1.0f : 0.0f;

	const FRotator ViewRotation = PlayerController->GetControlRotation();
	if (bFollowTarget && !IsValidFollowCandidate(FollowTarget.Get()))
	{
		HandleFollowTargetLost();
	}
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PlayerController->GetInputMouseDelta(MouseX, MouseY);
	if (IsFollowingTarget())
	{
		FloatingMovement->StopMovementImmediately();
		UpdateOrbitFollow(PlayerController, DeltaSeconds, MouseX, MouseY);
	}
	else
	{
		AddMovementInput(ViewRotation.Vector(), ForwardInput);
		AddMovementInput(FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y), RightInput);
		AddMovementInput(FVector::UpVector, UpInput);

		FRotator NewViewRotation = ViewRotation;
		const float LookX = MouseX * FMath::Max(0.0f, LookSensitivityX) * (bInvertLookX ? -1.0f : 1.0f);
		const float LookY = MouseY * FMath::Max(0.0f, LookSensitivityY) * (bInvertLookY ? -1.0f : 1.0f);
		NewViewRotation.Yaw += LookX;
		NewViewRotation.Pitch = FMath::ClampAngle(NewViewRotation.Pitch + LookY, -89.0f, 89.0f);
		PlayerController->SetControlRotation(NewViewRotation);
	}
}

bool ADMDebugObserverPawn::IsValidFollowCandidate(const AActor* Candidate) const
{
	const ACharacterBase* Character = Cast<ACharacterBase>(Candidate);
	if (!IsValid(Character) || Candidate == this)
	{
		return false;
	}
	if (Candidate->GetClass()->ImplementsInterface(UDMNonCombatActor::StaticClass())
		|| Candidate->ActorHasTag(PartsHitCollision::DetachedDebrisTag))
	{
		return false;
	}
	const FDMHealthSnapshot Health = DMHealth::Resolve(Candidate);
	return bAllowFollowDeadTarget || (!Character->IsDead && (!Health.bValid || Health.bAlive));
}

void ADMDebugObserverPawn::RefreshFollowCandidates(bool bForce)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		FollowCandidates.Reset();
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (!bForce && Now - LastFollowCandidateRefreshTime < 1.0)
	{
		return;
	}

	FollowCandidates.Reset();
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (IsValidFollowCandidate(*It))
		{
			FollowCandidates.Add(*It);
		}
	}
	FollowCandidates.Sort([](const TWeakObjectPtr<AActor>& Left, const TWeakObjectPtr<AActor>& Right)
	{
		const AActor* LeftActor = Left.Get();
		const AActor* RightActor = Right.Get();
		if (!LeftActor || !RightActor)
		{
			return LeftActor != nullptr;
		}
		const int32 NameOrder = LeftActor->GetPathName().Compare(RightActor->GetPathName(), ESearchCase::CaseSensitive);
		return NameOrder == 0 ? LeftActor->GetUniqueID() < RightActor->GetUniqueID() : NameOrder < 0;
	});
	LastFollowCandidateRefreshTime = Now;
	FollowTargetIndex = FollowCandidates.IndexOfByPredicate([this](const TWeakObjectPtr<AActor>& Candidate)
	{
		return Candidate == FollowTarget;
	});
}

void ADMDebugObserverPawn::FollowCandidateAtIndex(int32 CandidateIndex)
{
	if (FollowCandidates.IsEmpty())
	{
		ClearFollowTarget();
		return;
	}
	const int32 WrappedIndex = (CandidateIndex % FollowCandidates.Num() + FollowCandidates.Num()) % FollowCandidates.Num();
	SetFollowTarget(FollowCandidates[WrappedIndex].Get());
}

void ADMDebugObserverPawn::SetFollowTarget(AActor* NewTarget)
{
	if (!IsValidFollowCandidate(NewTarget))
	{
		ClearFollowTarget();
		return;
	}
	const bool bWasFollowing = IsFollowingTarget();
	const bool bNewTarget = FollowTarget.Get() != NewTarget;
	FollowTarget = NewTarget;
	bFollowTarget = true;
	if (bNewTarget && bResetOrbitOnNewTarget)
	{
		ResetOrbitState();
	}
	else
	{
		ClampOrbitState();
	}
	bSnapFollowCameraOnNextTick = bNewTarget || !bWasFollowing;
	RefreshFollowCandidates(true);
	FollowTargetIndex = FollowCandidates.IndexOfByPredicate([NewTarget](const TWeakObjectPtr<AActor>& Candidate)
	{
		return Candidate.Get() == NewTarget;
	});
	if (FloatingMovement)
	{
		FloatingMovement->StopMovementImmediately();
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()); PlayerController && PlayerController->IsLocalController())
	{
		const FVector Pivot = ResolveFollowPivot(NewTarget);
		if (bNewTarget)
		{
			const FVector DesiredLocation = ResolveOrbitLocation(NewTarget, Pivot);
			SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);
			PlayerController->SetControlRotation((Pivot - ObserverCamera->GetComponentLocation()).Rotation());
			bSnapFollowCameraOnNextTick = false;
		}
		else
		{
			PlayerController->SetControlRotation((Pivot - ObserverCamera->GetComponentLocation()).Rotation());
		}
	}
}

void ADMDebugObserverPawn::ClearFollowTarget()
{
	FollowTarget.Reset();
	bFollowTarget = false;
	FollowTargetIndex = INDEX_NONE;
	bSnapFollowCameraOnNextTick = false;
}

void ADMDebugObserverPawn::ToggleFollowTarget()
{
	if (IsFollowingTarget())
	{
		ClearFollowTarget();
	}
	else
	{
		FollowCharacterUnderCrosshair();
	}
}

void ADMDebugObserverPawn::FollowNextCharacter()
{
	const AActor* PreviousTarget = FollowTarget.Get();
	RefreshFollowCandidates(true);
	const int32 CurrentIndex = FollowCandidates.IndexOfByPredicate([PreviousTarget](const TWeakObjectPtr<AActor>& Candidate)
	{
		return Candidate.Get() == PreviousTarget;
	});
	FollowCandidateAtIndex(CurrentIndex == INDEX_NONE ? 0 : CurrentIndex + 1);
}

void ADMDebugObserverPawn::FollowPreviousCharacter()
{
	const AActor* PreviousTarget = FollowTarget.Get();
	RefreshFollowCandidates(true);
	const int32 CurrentIndex = FollowCandidates.IndexOfByPredicate([PreviousTarget](const TWeakObjectPtr<AActor>& Candidate)
	{
		return Candidate.Get() == PreviousTarget;
	});
	FollowCandidateAtIndex(CurrentIndex == INDEX_NONE ? FollowCandidates.Num() - 1 : CurrentIndex - 1);
}

void ADMDebugObserverPawn::FollowNearestCharacter()
{
	RefreshFollowCandidates(true);
	double BestDistanceSquared = TNumericLimits<double>::Max();
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < FollowCandidates.Num(); ++Index)
	{
		if (const AActor* Candidate = FollowCandidates[Index].Get())
		{
			const double DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				BestIndex = Index;
			}
		}
	}
	FollowCandidateAtIndex(BestIndex);
}

void ADMDebugObserverPawn::FollowCharacterUnderCrosshair()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PlayerController || !PlayerController->IsLocalController() || !World)
	{
		return;
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	PlayerController->GetPlayerViewPoint(EyeLocation, EyeRotation);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMDebugObserverFollow), true, this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, EyeLocation,
		EyeLocation + EyeRotation.Vector() * FMath::Max(100.0f, FollowSelectionTraceDistance), ECC_Visibility, QueryParams))
	{
		return;
	}
	AActor* Candidate = Hit.GetActor();
	if (Candidate && Candidate->ActorHasTag(PartsHitCollision::DetachedDebrisTag))
	{
		return;
	}
	while (Candidate && !Candidate->IsA<ACharacter>())
	{
		Candidate = Candidate->GetOwner();
	}
	if (IsValidFollowCandidate(Candidate))
	{
		SetFollowTarget(Candidate);
	}
}

AActor* ADMDebugObserverPawn::GetFollowTarget() const
{
	return IsFollowingTarget() ? FollowTarget.Get() : nullptr;
}

bool ADMDebugObserverPawn::IsFollowingTarget() const
{
	return bFollowTarget && IsValidFollowCandidate(FollowTarget.Get());
}

AActor* ADMDebugObserverPawn::GetCombatDebugSelectedActor() const
{
	return GetFollowTarget();
}

void ADMDebugObserverPawn::HandleFollowTargetLost()
{
	const int32 LostIndex = FollowTargetIndex;
	FollowTarget.Reset();
	bFollowTarget = false;
	FollowTargetIndex = INDEX_NONE;
	if (!bAutoFollowNextOnTargetLost)
	{
		return;
	}
	RefreshFollowCandidates(true);
	FollowCandidateAtIndex(LostIndex == INDEX_NONE ? 0 : LostIndex);
}

FVector ADMDebugObserverPawn::ResolveFollowPivot(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	FVector Pivot = FVector::ZeroVector;
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		bool bResolvedSocket = false;
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh)
		{
			if (!CameraFollowSocketName.IsNone() && Mesh->DoesSocketExist(CameraFollowSocketName))
			{
				Pivot = Mesh->GetSocketLocation(CameraFollowSocketName);
				bResolvedSocket = true;
			}
			else if (!DebugFollowSocketName.IsNone() && Mesh->DoesSocketExist(DebugFollowSocketName))
			{
				Pivot = Mesh->GetSocketLocation(DebugFollowSocketName);
				bResolvedSocket = true;
			}
		}
		if (!bResolvedSocket)
		{
			const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
			const float UpperBodyHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.5f : 45.0f;
			Pivot = Target->GetActorLocation() + Target->GetActorUpVector() * UpperBodyHeight;
		}
	}
	else
	{
		FVector BoundsExtent = FVector::ZeroVector;
		Target->GetActorBounds(false, Pivot, BoundsExtent);
	}
	return Pivot + Target->GetActorUpVector() * FollowPivotHeightOffset;
}

FVector ADMDebugObserverPawn::ResolveOrbitLocation(const AActor* Target, const FVector& Pivot) const
{
	const float BaseYaw = bUseTargetRotationAsOrbitBase && Target ? Target->GetActorRotation().Yaw : 0.0f;
	const FRotator OrbitRotation(-OrbitPitch, BaseYaw + OrbitYaw, 0.0f);
	return Pivot - OrbitRotation.Vector() * CurrentFollowDistance;
}

void ADMDebugObserverPawn::ResetOrbitState()
{
	OrbitYaw = DefaultOrbitYaw;
	OrbitPitch = DefaultOrbitPitch;
	const float MinDistance = FMath::Max(1.0f, FMath::Min(MinimumFollowDistance, MaximumFollowDistance));
	const float MaxDistance = FMath::Max(MinDistance, FMath::Max(MinimumFollowDistance, MaximumFollowDistance));
	TargetFollowDistance = FMath::Clamp(DefaultFollowDistance, MinDistance, MaxDistance);
	CurrentFollowDistance = TargetFollowDistance;
	ClampOrbitState();
}

void ADMDebugObserverPawn::ClampOrbitState()
{
	const float MaxYaw = FMath::Clamp(FMath::Abs(MaximumOrbitYaw), 0.0f, 180.0f);
	OrbitYaw = FMath::Clamp(OrbitYaw, -MaxYaw, MaxYaw);
	const float MinPitch = FMath::Clamp(FMath::Min(MinimumOrbitPitch, MaximumOrbitPitch), -89.0f, 89.0f);
	const float MaxPitch = FMath::Clamp(FMath::Max(MinimumOrbitPitch, MaximumOrbitPitch), MinPitch, 89.0f);
	OrbitPitch = FMath::Clamp(OrbitPitch, MinPitch, MaxPitch);
	const float MinDistance = FMath::Max(1.0f, FMath::Min(MinimumFollowDistance, MaximumFollowDistance));
	const float MaxDistance = FMath::Max(MinDistance, FMath::Max(MinimumFollowDistance, MaximumFollowDistance));
	TargetFollowDistance = FMath::Clamp(TargetFollowDistance, MinDistance, MaxDistance);
	CurrentFollowDistance = FMath::Clamp(CurrentFollowDistance, MinDistance, MaxDistance);
}

void ADMDebugObserverPawn::UpdateOrbitFollow(APlayerController* PlayerController, float DeltaSeconds, float MouseX, float MouseY)
{
	AActor* Target = FollowTarget.Get();
	if (!PlayerController || !Target || !ObserverCamera)
	{
		return;
	}

	OrbitYaw = DMNavigationSafety::ApplyOrbitInput(OrbitYaw, MouseX, OrbitSensitivityX, bInvertOrbitX);
	OrbitPitch = DMNavigationSafety::ApplyOrbitInput(OrbitPitch, MouseY, OrbitSensitivityY, bInvertOrbitY);
	float WheelDelta = PlayerController->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (FMath::IsNearlyZero(WheelDelta))
	{
		WheelDelta = PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1.0f
			: PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? -1.0f : 0.0f;
	}
	if (!FMath::IsNearlyZero(WheelDelta))
	{
		TargetFollowDistance -= WheelDelta * FMath::Max(1.0f, FollowZoomStep);
	}
	ClampOrbitState();
	CurrentFollowDistance = FMath::FInterpTo(CurrentFollowDistance, TargetFollowDistance, DeltaSeconds, FMath::Max(0.0f, FollowZoomInterpSpeed));

	const FVector Pivot = ResolveFollowPivot(Target);
	const FVector DesiredLocation = ResolveOrbitLocation(Target, Pivot);
	const float TeleportDistance = FMath::Max(0.0f, FollowTeleportDistance);
	const bool bTeleport = bSnapFollowCameraOnNextTick || (TeleportDistance > 0.0f
		&& FVector::DistSquared(GetActorLocation(), DesiredLocation) > FMath::Square(TeleportDistance));
	const FVector NextLocation = bTeleport
		? DesiredLocation
		: FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaSeconds, FMath::Max(0.0f, FollowPositionInterpSpeed));
	SetActorLocation(NextLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bSnapFollowCameraOnNextTick = false;

	const FRotator LookAtRotation = (Pivot - ObserverCamera->GetComponentLocation()).Rotation();
	FRotator NextRotation = FMath::RInterpTo(PlayerController->GetControlRotation(), LookAtRotation, DeltaSeconds,
		FMath::Max(0.0f, FollowRotationInterpSpeed));
	FVector2D ScreenPosition;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	const bool bProjected = PlayerController->ProjectWorldLocationToScreen(Pivot, ScreenPosition, true);
	const float SafeZone = FMath::Clamp(TargetScreenSafeZone, 0.05f, 0.5f);
	const bool bOutsideSafeZone = !bProjected || ViewportWidth <= 0 || ViewportHeight <= 0
		|| FMath::Abs(ScreenPosition.X - ViewportWidth * 0.5f) > ViewportWidth * SafeZone
		|| FMath::Abs(ScreenPosition.Y - ViewportHeight * 0.5f) > ViewportHeight * SafeZone;
	if (bTeleport || bOutsideSafeZone)
	{
		NextRotation = LookAtRotation;
	}
	PlayerController->SetControlRotation(NextRotation);
}

float ADMDebugObserverPawn::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return 0.0f;
}

