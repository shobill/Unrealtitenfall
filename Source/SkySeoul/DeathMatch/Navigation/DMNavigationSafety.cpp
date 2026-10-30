#include "DMNavigationSafety.h"

#include "Components/CapsuleComponent.h"
#include "DeathMatch/AI/Combat/DMCombatDebug.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationPath.h"

FDMNavDimensions DMNavigationSafety::FromCapsule(float ScaledRadius, float ScaledHalfHeight)
{
	return {FMath::Max(0.0f, ScaledRadius), FMath::Max(0.0f, ScaledHalfHeight) * 2.0f};
}

FDMNavMismatch DMNavigationSafety::EvaluateMismatch(
	const FDMNavDimensions& Capsule,
	const FDMNavDimensions& CharacterAgent,
	const FDMNavDimensions& NavMeshAgent,
	float CharacterStepHeight,
	float NavMeshStepHeight,
	float SizeTolerance,
	float StepTolerance)
{
	const float SizeEpsilon = FMath::Max(0.0f, SizeTolerance);
	FDMNavMismatch Result;
	Result.bCharacterAgentMismatch = FMath::Abs(CharacterAgent.Radius - Capsule.Radius) > SizeEpsilon
		|| FMath::Abs(CharacterAgent.Height - Capsule.Height) > SizeEpsilon;
	Result.bNavMeshTooSmall = NavMeshAgent.Radius + SizeEpsilon < Capsule.Radius
		|| NavMeshAgent.Height + SizeEpsilon < Capsule.Height;
	Result.bStepHeightMismatch = FMath::Abs(NavMeshStepHeight - CharacterStepHeight) > FMath::Max(0.0f, StepTolerance);
	Result.bWrongNavData = NavMeshAgent.Radius + SizeEpsilon < CharacterAgent.Radius
		|| NavMeshAgent.Height + SizeEpsilon < CharacterAgent.Height;
	return Result;
}

float DMNavigationSafety::ApplyOrbitInput(float Current, float MouseDelta, float Sensitivity, bool bInvert)
{
	// Orbit position is the inverse of the LookAt direction, so its input sign is
	// intentionally opposite to free-look. The independent invert flag is applied last.
	const float Direction = bInvert ? 1.0f : -1.0f;
	return Current + MouseDelta * FMath::Max(0.0f, Sensitivity) * Direction;
}

bool DMNavigationSafety::IsAvoidanceConfigurationValid(bool bCrowdFollowing, bool bRVOAvoidance)
{
	return !(bCrowdFollowing && bRVOAvoidance);
}

bool DMNavigationSafety::IsCandidatePathUsable(bool bProjected, bool bPathValid, bool bPartial, bool bClearancePassed)
{
	return bProjected && bPathValid && !bPartial && bClearancePassed;
}

bool DMNavigationSafety::ShouldRejectMoveForClearance(bool bPassabilityClear, bool bExtraQualityClear)
{
	(void)bExtraQualityClear;
	return !bPassabilityClear;
}

void DMNavigationSafety::CalculateProbeSize(float ActualRadius, float ActualHalfHeight, float RadiusScale,
	float HalfHeightScale, float& OutRadius, float& OutHalfHeight)
{
	OutRadius = FMath::Max(0.0f, ActualRadius) * FMath::Max(0.0f, RadiusScale);
	OutHalfHeight = FMath::Max(OutRadius,
		FMath::Max(0.0f, ActualHalfHeight) * FMath::Max(0.0f, HalfHeightScale));
}

bool DMNavigationSafety::ShouldRaiseProbe(const FVector& SegmentStart, const FVector& SegmentEnd,
	bool bEnabled, float VerticalSegmentThreshold)
{
	return bEnabled && FMath::Abs(SegmentEnd.Z - SegmentStart.Z) >= FMath::Max(0.0f, VerticalSegmentThreshold);
}

float DMNavigationSafety::CalculateRaisedHeight(float MaxStepHeight, float StepHeightProbeMultiplier)
{
	return FMath::Max(0.0f, MaxStepHeight) * FMath::Max(0.0f, StepHeightProbeMultiplier);
}

void DMNavigationSafety::BuildPathSweepIntervals(const TArray<FVector>& PathPoints, float SampleSpacing,
	bool bRaiseOnVerticalSegments, float VerticalSegmentThreshold, TArray<FDMSweepInterval>& OutIntervals)
{
	OutIntervals.Reset();
	const float Spacing = FMath::Max(1.0f, SampleSpacing);
	for (int32 Segment = 0; Segment + 1 < PathPoints.Num(); ++Segment)
	{
		const FVector& SegmentStart = PathPoints[Segment];
		const FVector& SegmentEnd = PathPoints[Segment + 1];
		const float Length = FVector::Distance(SegmentStart, SegmentEnd);
		if (Length <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const int32 Subdivisions = FMath::Max(1, FMath::CeilToInt(Length / Spacing));
		const bool bRaised = ShouldRaiseProbe(
			SegmentStart, SegmentEnd, bRaiseOnVerticalSegments, VerticalSegmentThreshold);
		for (int32 Subdivision = 0; Subdivision < Subdivisions; ++Subdivision)
		{
			FDMSweepInterval& Interval = OutIntervals.AddDefaulted_GetRef();
			Interval.Start = FMath::Lerp(SegmentStart, SegmentEnd,
				static_cast<float>(Subdivision) / Subdivisions);
			Interval.End = FMath::Lerp(SegmentStart, SegmentEnd,
				static_cast<float>(Subdivision + 1) / Subdivisions);
			Interval.PathSegment = Segment;
			Interval.bRaised = bRaised;
		}
	}
}

bool DMNavigationSafety::IsHardPathFailureReason(FName Reason)
{
	return Reason == TEXT("NavigationUnavailable")
		|| Reason == TEXT("NavProjectionFailed")
		|| Reason == TEXT("InvalidOrPartialPath")
		|| Reason == TEXT("DestinationStaticBlocked")
		|| Reason == TEXT("StaticPassabilityBlocked");
}

bool DMNavigationSafety::ShouldHardBlockPathHit(bool bWorldStatic, bool bPawn,
	bool bStartPenetrating, bool bSameStaticAtPathCentre)
{
	return bWorldStatic && !bPawn && !bStartPenetrating && bSameStaticAtPathCentre;
}

bool DMNavigationSafety::ShouldRetryStuckMove(int32 CompletedRepathCount)
{
	return CompletedRepathCount <= 0;
}

bool DMNavigationSafety::IsStuck(const FDMStuckEvidence& Evidence, float RequiredSeconds, float MinimumSpeedRatio, FName& OutReason)
{
	OutReason = NAME_None;
	if (Evidence.SecondsWithoutProgress < FMath::Max(0.05f, RequiredSeconds))
	{
		return false;
	}

	const float SpeedRatio = Evidence.RequestedSpeed > 1.0f ? Evidence.ActualSpeed / Evidence.RequestedSpeed : 1.0f;
	if (Evidence.bWallContact)
	{
		OutReason = TEXT("WallContact");
		return true;
	}
	if (Evidence.SameGoalRepathCount >= 2)
	{
		OutReason = TEXT("RepeatedRepath");
		return true;
	}
	if (SpeedRatio < FMath::Clamp(MinimumSpeedRatio, 0.0f, 1.0f))
	{
		OutReason = TEXT("LowRequestedSpeedRatio");
		return true;
	}
	if (Evidence.ActualSpeed > 10.0f && Evidence.VelocityAlignment < 0.25f)
	{
		OutReason = TEXT("VelocityDirectionMismatch");
		return true;
	}

	OutReason = TEXT("NoGoalProgress");
	return true;
}

FDMClearanceResult DMNavigationSafety::ValidateCapsulePath(
	UWorld* World,
	const APawn* Pawn,
	const UNavigationPath* Path,
	const FDMPathProbeSettings& Settings)
{
	FDMClearanceResult Result;
	Result.ClearanceScale = Settings.RadiusScale;
	Result.bPassabilityFailure = Settings.bPassabilityProbe;
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!World || !Pawn || !Capsule || !Path
		|| !IsCandidatePathUsable(true, Path->IsValid(), Path->IsPartial(), true)
		|| Path->PathPoints.Num() < 2)
	{
		Result.bClear = false;
		Result.FailureReason = TEXT("InvalidOrPartialPath");
		return Result;
	}

	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
	const float ActualHalfHeight = HalfHeight;
	CalculateProbeSize(Radius, HalfHeight, Settings.RadiusScale, Settings.HalfHeightScale, Radius, HalfHeight);
	Result.TestedRadius = Radius;
	Result.TestedHalfHeight = HalfHeight;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMNavigationClearance), false, Pawn);
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionObjectQueryParams StaticObjectParams;
	StaticObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	TArray<FDMSweepInterval> Intervals;
	BuildPathSweepIntervals(Path->PathPoints, Settings.SampleSpacing,
		Settings.bRaiseOnVerticalSegments, Settings.VerticalSegmentThreshold, Intervals);
	if (Intervals.IsEmpty())
	{
		Result.bClear = false;
		Result.FailureReason = TEXT("InvalidOrPartialPath");
		return Result;
	}

	const float RaisedHeight = CalculateRaisedHeight(Settings.MaxStepHeight, Settings.StepHeightProbeMultiplier);
	const bool bDrawDebug = DMCombatDebug::Mode() >= 2;
	auto ToCapsuleCenter = [&](const FVector& Point, bool bPathOrigin, bool bRaised)
	{
		const bool bAlreadyCenter = bPathOrigin
			&& FMath::Abs(Point.Z - Pawn->GetActorLocation().Z) <= FMath::Max(5.0f, ActualHalfHeight * 0.35f);
		const FVector GroundPoint = bAlreadyCenter
			? Point - FVector(0.0f, 0.0f, ActualHalfHeight)
			: Point;
		const float BaseOffset = HalfHeight + FMath::Max(0.0f, Settings.GroundMargin);
		return GroundPoint + FVector(0.0f, 0.0f, BaseOffset + (bRaised ? RaisedHeight : 0.0f));
	};

	for (int32 IntervalIndex = 0; IntervalIndex < Intervals.Num(); ++IntervalIndex)
	{
		const FDMSweepInterval& Interval = Intervals[IntervalIndex];
		const bool bPathOrigin = Interval.PathSegment == 0
			&& Interval.Start.Equals(Path->PathPoints[0], KINDA_SMALL_NUMBER);
		const FVector Start = ToCapsuleCenter(Interval.Start, bPathOrigin, Interval.bRaised);
		const FVector End = ToCapsuleCenter(Interval.End, false, Interval.bRaised);
		if (Interval.bRaised)
		{
			Result.bRaisedStepProbeUsed = true;
			Result.RaisedHeight = RaisedHeight;
		}

		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams, Shape, Params);
		bool bQualityLow = false;
		for (const FHitResult& Hit : Hits)
		{
			if (Cast<APawn>(Hit.GetActor()))
			{
				Result.bDynamicObstacleDetected = true;
				continue;
			}
			if (!Hit.GetComponent() || Hit.GetComponent()->GetCollisionObjectType() != ECC_WorldStatic)
			{
				continue;
			}
			bQualityLow = true;
			if (!Result.HitActor.IsValid())
			{
				Result.FailureLocation = Hit.ImpactPoint.IsNearlyZero() ? End : Hit.ImpactPoint;
				Result.HitActor = Hit.GetActor();
				Result.HitComponent = Hit.GetComponent();
				Result.FailedSegment = Interval.PathSegment;
			}
			if (!Settings.bPassabilityProbe || Hit.bStartPenetrating)
			{
				continue;
			}

			// A side brush against the loose capsule is ambiguous. Confirm that the same
			// fixed component crosses the path centre before converting it into a hard fail.
			TArray<FHitResult> CentreHits;
			const float CentreRadius = FMath::Clamp(Radius * 0.15f, 2.0f, 8.0f);
			World->SweepMultiByObjectType(CentreHits, Start, End, FQuat::Identity, StaticObjectParams,
				FCollisionShape::MakeSphere(CentreRadius), Params);
			const bool bSameStaticAtCentre = CentreHits.ContainsByPredicate([&Hit](const FHitResult& CentreHit)
			{
				return !CentreHit.bStartPenetrating && CentreHit.GetActor() == Hit.GetActor()
					&& CentreHit.GetComponent() == Hit.GetComponent();
			});
			if (ShouldHardBlockPathHit(true, false, Hit.bStartPenetrating, bSameStaticAtCentre))
			{
				Result.bClear = false;
				Result.FailureReason = TEXT("StaticPassabilityBlocked");
				Result.FailureLocation = Hit.ImpactPoint.IsNearlyZero() ? End : Hit.ImpactPoint;
				Result.HitActor = Hit.GetActor();
				Result.HitComponent = Hit.GetComponent();
				Result.FailedSegment = Interval.PathSegment;
				if (bDrawDebug)
				{
					DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Red, false, 2.0f);
				}
				return Result;
			}
		}

		if (bQualityLow)
		{
			Result.bClear = Settings.bPassabilityProbe;
			Result.QualityReason = Settings.bPassabilityProbe ? FName(TEXT("WallClearanceQualityLow"))
				: FName(TEXT("NarrowPath"));
		}
		if (bDrawDebug)
		{
			const FColor Color = bQualityLow ? FColor::Orange
				: (Interval.bRaised ? FColor::Cyan : FColor::Green);
			DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, Color, false, 2.0f);
		}
	}
	return Result;
}

FDMClearanceResult DMNavigationSafety::ValidateDestinationCapsule(
	UWorld* World,
	const APawn* Pawn,
	const FVector& ProjectedGoal,
	float DestinationCapsuleScale,
	float GroundMargin)
{
	FDMClearanceResult Result;
	Result.ClearanceScale = FMath::Clamp(DestinationCapsuleScale, 0.5f, 1.0f);
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!World || !Pawn || !Capsule)
	{
		Result.bClear = false;
		Result.FailureReason = TEXT("NavigationUnavailable");
		return Result;
	}

	float ActualRadius = 0.0f;
	float ActualHalfHeight = 0.0f;
	Capsule->GetScaledCapsuleSize(ActualRadius, ActualHalfHeight);
	const float Radius = ActualRadius * Result.ClearanceScale;
	const float HalfHeight = FMath::Max(Radius, ActualHalfHeight * Result.ClearanceScale);
	const FVector Center = ProjectedGoal + FVector(0.0f, 0.0f,
		ActualHalfHeight + FMath::Max(0.0f, GroundMargin));
	Result.TestedRadius = Radius;
	Result.TestedHalfHeight = HalfHeight;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMNavigationDestination), false, Pawn);
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	FCollisionObjectQueryParams StaticObjectParams;
	StaticObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, StaticObjectParams,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	if (!Overlaps.IsEmpty())
	{
		Result.bClear = false;
		Result.FailureReason = TEXT("DestinationStaticBlocked");
		Result.FailureLocation = Center;
		Result.HitActor = Overlaps[0].GetActor();
		Result.HitComponent = Overlaps[0].GetComponent();
	}
	if (DMCombatDebug::Mode() >= 2)
	{
		DrawDebugCapsule(World, Center, HalfHeight, Radius, FQuat::Identity,
			Result.bClear ? FColor::Blue : FColor::Red, false, 2.0f);
	}
	return Result;
}
