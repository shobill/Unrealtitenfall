#pragma once

#include "CoreMinimal.h"

class APawn;
class AActor;
class UPrimitiveComponent;
class UNavigationPath;
class UWorld;

struct FDMNavDimensions
{
	float Radius = 0.0f;
	float Height = 0.0f;
};

struct FDMNavMismatch
{
	bool bCharacterAgentMismatch = false;
	bool bNavMeshTooSmall = false;
	bool bStepHeightMismatch = false;
	bool bWrongNavData = false;

	bool HasWarning() const
	{
		return bCharacterAgentMismatch || bNavMeshTooSmall || bStepHeightMismatch || bWrongNavData;
	}
};

struct FDMStuckEvidence
{
	float SecondsWithoutProgress = 0.0f;
	float ActualSpeed = 0.0f;
	float RequestedSpeed = 0.0f;
	float VelocityAlignment = 1.0f;
	int32 SameGoalRepathCount = 0;
	bool bWallContact = false;
};

struct FDMClearanceResult
{
	bool bClear = true;
	bool bDynamicObstacleDetected = false;
	bool bRaisedStepProbeUsed = false;
	FName FailureReason = NAME_None;
	FName QualityReason = NAME_None;
	FVector FailureLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HitActor;
	TWeakObjectPtr<UPrimitiveComponent> HitComponent;
	float TestedRadius = 0.0f;
	float TestedHalfHeight = 0.0f;
	float RaisedHeight = 0.0f;
	float ClearanceScale = 1.0f;
	int32 FailedSegment = INDEX_NONE;
	bool bPassabilityFailure = true;
};

struct FDMPathProbeSettings
{
	float RadiusScale = 0.70f;
	float HalfHeightScale = 0.80f;
	float SampleSpacing = 250.0f;
	float GroundMargin = 2.0f;
	float VerticalSegmentThreshold = 5.0f;
	float StepHeightProbeMultiplier = 1.0f;
	float MaxStepHeight = 0.0f;
	bool bRaiseOnVerticalSegments = true;
	bool bPassabilityProbe = true;
};

struct FDMSweepInterval
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	int32 PathSegment = INDEX_NONE;
	bool bRaised = false;
};

namespace DMNavigationSafety
{
	SKYSEOUL_API FDMNavDimensions FromCapsule(float ScaledRadius, float ScaledHalfHeight);
	SKYSEOUL_API FDMNavMismatch EvaluateMismatch(
		const FDMNavDimensions& Capsule,
		const FDMNavDimensions& CharacterAgent,
		const FDMNavDimensions& NavMeshAgent,
		float CharacterStepHeight,
		float NavMeshStepHeight,
		float SizeTolerance = 1.0f,
		float StepTolerance = 10.0f);
	SKYSEOUL_API float ApplyOrbitInput(float Current, float MouseDelta, float Sensitivity, bool bInvert);
	SKYSEOUL_API bool IsAvoidanceConfigurationValid(bool bCrowdFollowing, bool bRVOAvoidance);
	SKYSEOUL_API bool IsCandidatePathUsable(bool bProjected, bool bPathValid, bool bPartial, bool bClearancePassed);
	/** Scale 1.0 is passability. Extra clearance is a quality signal and never rejects by itself. */
	SKYSEOUL_API bool ShouldRejectMoveForClearance(bool bPassabilityClear, bool bExtraQualityClear);
	SKYSEOUL_API void CalculateProbeSize(float ActualRadius, float ActualHalfHeight, float RadiusScale,
		float HalfHeightScale, float& OutRadius, float& OutHalfHeight);
	SKYSEOUL_API bool ShouldRaiseProbe(const FVector& SegmentStart, const FVector& SegmentEnd,
		bool bEnabled, float VerticalSegmentThreshold);
	SKYSEOUL_API float CalculateRaisedHeight(float MaxStepHeight, float StepHeightProbeMultiplier);
	SKYSEOUL_API void BuildPathSweepIntervals(const TArray<FVector>& PathPoints, float SampleSpacing,
		bool bRaiseOnVerticalSegments, float VerticalSegmentThreshold, TArray<FDMSweepInterval>& OutIntervals);
	SKYSEOUL_API bool IsHardPathFailureReason(FName Reason);
	SKYSEOUL_API bool ShouldHardBlockPathHit(bool bWorldStatic, bool bPawn,
		bool bStartPenetrating, bool bSameStaticAtPathCentre);
	SKYSEOUL_API bool ShouldRetryStuckMove(int32 CompletedRepathCount);
	SKYSEOUL_API bool IsStuck(const FDMStuckEvidence& Evidence, float RequiredSeconds, float MinimumSpeedRatio, FName& OutReason);
	SKYSEOUL_API FDMClearanceResult ValidateCapsulePath(
		UWorld* World,
		const APawn* Pawn,
		const UNavigationPath* Path,
		const FDMPathProbeSettings& Settings);
	SKYSEOUL_API FDMClearanceResult ValidateDestinationCapsule(
		UWorld* World,
		const APawn* Pawn,
		const FVector& ProjectedGoal,
		float DestinationCapsuleScale,
		float GroundMargin = 2.0f);
}
