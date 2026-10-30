#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Core/DMNonCombatActor.h"
#include "DeathMatch/Debug/DMCombatDebugSelectionProvider.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "DMDebugObserverPawn.generated.h"

class UCameraComponent;
class UDMDebugAllHealthBarsComponent;
class UFloatingPawnMovement;
class USceneComponent;
class UStaticMeshComponent;
class APlayerController;

/** Local, non-replicated, invulnerable free-flight observer. It is never a combat team member. */
UCLASS(Blueprintable)
class SKYSEOUL_API ADMDebugObserverPawn : public APawn, public IGenericTeamAgentInterface, public IDMNonCombatActor, public IDMCombatDebugSelectionProvider
{
	GENERATED_BODY()

public:
	ADMDebugObserverPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId::NoTeam; }
	virtual AActor* GetCombatDebugSelectedActor() const override;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Debug Observer")
	UDMDebugAllHealthBarsComponent* GetDebugHealthBars() const { return DebugHealthBars; }

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void SetFollowTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void ClearFollowTarget();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void ToggleFollowTarget();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void FollowNextCharacter();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void FollowPreviousCharacter();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void FollowNearestCharacter();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Observer|Follow")
	void FollowCharacterUnderCrosshair();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Debug Observer|Follow")
	AActor* GetFollowTarget() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Debug Observer|Follow")
	bool IsFollowingTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	bool IsValidFollowCandidate(const AActor* Candidate) const;
	void RefreshFollowCandidates(bool bForce = false);
	void FollowCandidateAtIndex(int32 CandidateIndex);
	void HandleFollowTargetLost();
	FVector ResolveFollowPivot(const AActor* Target) const;
	FVector ResolveOrbitLocation(const AActor* Target, const FVector& Pivot) const;
	void ResetOrbitState();
	void ClampOrbitState();
	void UpdateOrbitFollow(APlayerController* PlayerController, float DeltaSeconds, float MouseX, float MouseY);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Observer")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Observer")
	TObjectPtr<UCameraComponent> ObserverCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Observer")
	TObjectPtr<UStaticMeshComponent> ObserverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Observer")
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Observer")
	TObjectPtr<UDMDebugAllHealthBarsComponent> DebugHealthBars;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float MoveSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Movement", meta = (ClampMin = "1.0", Units = "cm/s"))
	float FastMoveSpeed = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookSensitivityX = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookSensitivityY = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Look")
	bool bInvertLookX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Look")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Appearance")
	bool bShowObserverMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Appearance", meta = (ClampMin = "0.01"))
	float ObserverMeshScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer")
	bool bStartWithDebugHealthBarsEnabled = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> FollowTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "DeathMatch|Debug Observer|Follow")
	bool bFollowTarget = false;

	/** Legacy serialized value. Orbit follow does not use positional offsets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Follow Offset (Legacy, Ignored)"))
	FVector FollowOffset = FVector(-300.0f, 0.0f, 150.0f);

	/** Legacy serialized value. Use FollowPositionInterpSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Follow Interp Speed (Legacy, Ignored)", ClampMin = "0.0"))
	float FollowInterpSpeed = 8.0f;

	/** Legacy serialized value. Use bUseTargetRotationAsOrbitBase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Use Target Local Offset (Legacy, Ignored)"))
	bool bUseTargetLocalOffset = true;

	/** Legacy serialized value. Orbit follow always looks at the target pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Look At Follow Target (Legacy, Always On)"))
	bool bLookAtFollowTarget = false;

	/** Legacy serialized value. Use FollowRotationInterpSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Follow Look Interp Speed (Legacy, Ignored)", ClampMin = "0.0"))
	float FollowLookInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "1.0", Units = "cm"))
	float DefaultFollowDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "1.0", Units = "cm"))
	float MinimumFollowDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "1.0", Units = "cm"))
	float MaximumFollowDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "1.0", Units = "cm"))
	float FollowZoomStep = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0"))
	float FollowZoomInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0"))
	float FollowPositionInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0"))
	float FollowRotationInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0"))
	float OrbitSensitivityX = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0"))
	float OrbitSensitivityY = 1.0f;

	/** Inverts horizontal orbit only. Free-look continues to use bInvertLookX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	bool bInvertOrbitX = false;

	/** Inverts vertical orbit only. Free-look continues to use bInvertLookY. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	bool bInvertOrbitY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float MaximumOrbitYaw = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float MinimumOrbitPitch = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float MaximumOrbitPitch = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (Units = "deg"))
	float DefaultOrbitYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (Units = "deg"))
	float DefaultOrbitPitch = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	bool bResetOrbitOnNewTarget = true;

	/** False keeps the orbit in a stable world frame; true makes yaw relative to the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	bool bUseTargetRotationAsOrbitBase = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (Units = "cm"))
	float FollowPivotHeightOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	FName CameraFollowSocketName = TEXT("CameraFollow");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera")
	FName DebugFollowSocketName = TEXT("DebugFollow");

	/** Fraction of each half-viewport treated as the central rotation-safe region. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow Camera", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float TargetScreenSafeZone = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow", meta = (ClampMin = "0.0", Units = "cm"))
	float FollowTeleportDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow")
	bool bAllowFollowDeadTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow")
	bool bAutoFollowNextOnTargetLost = true;

	/** Legacy serialized value. Follow movement keys are now ignored instead of breaking follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Manual Movement Breaks Follow (Legacy, Ignored)"))
	bool bManualMovementBreaksFollow = false;

	/** Legacy serialized value. Orbit distance uses Minimum/MaximumFollowDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "DeathMatch|Debug Observer|Follow|Legacy", meta = (DisplayName = "Max Follow Offset Distance (Legacy, Ignored)", ClampMin = "0.0", Units = "cm"))
	float MaxFollowOffsetDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Observer|Follow", meta = (ClampMin = "100.0", Units = "cm"))
	float FollowSelectionTraceDistance = 100000.0f;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> FollowCandidates;

	int32 FollowTargetIndex = INDEX_NONE;
	double LastFollowCandidateRefreshTime = -TNumericLimits<double>::Max();
	float OrbitYaw = 0.0f;
	float OrbitPitch = 15.0f;
	float CurrentFollowDistance = 500.0f;
	float TargetFollowDistance = 500.0f;
	bool bSnapFollowCameraOnNextTick = false;
};

