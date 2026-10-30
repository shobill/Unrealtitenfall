#include "TitanDefence/Robot/Destruction/TDRobotPartSeparationComponent.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "PhysicsEngine/BodySetup.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartDebrisActor.h"

namespace
{
	struct FTDRobotPartSeparationWorkItem
	{
		const FTDRobotPartDefinition* Definition = nullptr;
		FTDRobotPartSeparationSettings Settings;
		FTransform WorldTransform = FTransform::Identity;
		int32 BoneIndex = INDEX_NONE;
		bool bBoneWasHidden = false;
		ATDRobotPartDebrisActor* DebrisActor = nullptr;
		FString FailureDetail;
	};

	FString GetPartTagForLog(const FGameplayTag& PartTag)
	{
		return PartTag.IsValid() ? PartTag.ToString() : TEXT("<None>");
	}

	void LogPartSeparationFailure(
		const AActor* Robot,
		const FGameplayTag& RequestedPart,
		const FTDRobotPartDefinition* FailedDefinition,
		const FString& FailureItem,
		ELogVerbosity::Type Verbosity = ELogVerbosity::Error)
	{
		const FString FailedPartTag = FailedDefinition
			? GetPartTagForLog(FailedDefinition->PartTag)
			: GetPartTagForLog(RequestedPart);

		if (Verbosity == ELogVerbosity::Warning)
		{
			UE_LOG(LogTDRobot, Warning,
				TEXT("Robot part separation failed. Robot='%s' RequestedPartTag='%s' FailedPartTag='%s' FailureItem='%s'"),
				*GetNameSafe(Robot),
				*GetPartTagForLog(RequestedPart),
				*FailedPartTag,
				*FailureItem);
		}
		else
		{
			UE_LOG(LogTDRobot, Error,
				TEXT("Robot part separation failed. Robot='%s' RequestedPartTag='%s' FailedPartTag='%s' FailureItem='%s'"),
				*GetNameSafe(Robot),
				*GetPartTagForLog(RequestedPart),
				*FailedPartTag,
				*FailureItem);
		}
	}

	bool AreSettingsValid(const FTDRobotPartSeparationSettings& Settings, FString& OutFailureItem)
	{
		if (!FMath::IsFinite(Settings.MassKg) || Settings.MassKg <= 0.0f)
		{
			OutFailureItem = TEXT("MassKg must be greater than zero.");
			return false;
		}
		if (!FMath::IsFinite(Settings.LinearDamping) || Settings.LinearDamping < 0.0f)
		{
			OutFailureItem = TEXT("LinearDamping must not be negative.");
			return false;
		}
		if (!FMath::IsFinite(Settings.AngularDamping) || Settings.AngularDamping < 0.0f)
		{
			OutFailureItem = TEXT("AngularDamping must not be negative.");
			return false;
		}
		if (!FMath::IsFinite(Settings.MaxAngularVelocityDegrees)
			|| Settings.MaxAngularVelocityDegrees < 0.0f)
		{
			OutFailureItem = TEXT("MaxAngularVelocityDegrees must not be negative.");
			return false;
		}
		if (!FMath::IsFinite(Settings.SeparationImpulse) || Settings.SeparationImpulse < 0.0f)
		{
			OutFailureItem = TEXT("SeparationImpulse must not be negative.");
			return false;
		}
		if (!FMath::IsFinite(Settings.UpwardImpulseRatio)
			|| Settings.UpwardImpulseRatio < 0.0f
			|| Settings.UpwardImpulseRatio > 1.0f)
		{
			OutFailureItem = TEXT("UpwardImpulseRatio must be between zero and one.");
			return false;
		}
		if (!FMath::IsFinite(Settings.Friction) || Settings.Friction < 0.0f)
		{
			OutFailureItem = TEXT("Friction must not be negative.");
			return false;
		}
		if (!FMath::IsFinite(Settings.Restitution)
			|| Settings.Restitution < 0.0f
			|| Settings.Restitution > 1.0f)
		{
			OutFailureItem = TEXT("Restitution must be between zero and one.");
			return false;
		}
		if (!FMath::IsFinite(Settings.DebrisLifetime) || Settings.DebrisLifetime <= 0.0f)
		{
			OutFailureItem = TEXT("DebrisLifetime must be greater than zero.");
			return false;
		}

		return true;
	}
}

UTDRobotPartSeparationComponent::UTDRobotPartSeparationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTDRobotPartSeparationComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildResolvedParts(FGameplayTag(), false);
}

ETDRobotPartSeparationResult UTDRobotPartSeparationComponent::BuildResolvedParts(
	FGameplayTag RequestedPart,
	bool bRequireSkeletalMesh)
{
	bResolvedPartsValid = false;
	bSkeletalMeshValidated = false;
	ResolvedDataAsset.Reset();
	ResolvedParts.Reset();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(PartDataAsset))
	{
		LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr, TEXT("PartDataAsset is not assigned."));
		return ETDRobotPartSeparationResult::DataAssetNotSet;
	}

	if (PartDataAsset->Parts.IsEmpty())
	{
		LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr, TEXT("PartDataAsset.Parts is empty."));
		return ETDRobotPartSeparationResult::ConfigurationError;
	}

	FString SettingsFailure;
	if (!AreSettingsValid(DefaultSeparationSettings, SettingsFailure))
	{
		LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr,
			FString::Printf(TEXT("DefaultSeparationSettings: %s"), *SettingsFailure));
		return ETDRobotPartSeparationResult::ConfigurationError;
	}

	TMap<FGameplayTag, int32> DefinitionIndices;
	bool bConfigurationValid = true;
	for (int32 DefinitionIndex = 0; DefinitionIndex < PartDataAsset->Parts.Num(); ++DefinitionIndex)
	{
		const FTDRobotPartDefinition& Definition = PartDataAsset->Parts[DefinitionIndex];
		if (!Definition.PartTag.IsValid())
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition, TEXT("PartTag is empty or invalid."));
			bConfigurationValid = false;
			continue;
		}

		if (DefinitionIndices.Contains(Definition.PartTag))
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition, TEXT("PartTag is duplicated."));
			bConfigurationValid = false;
			continue;
		}

		DefinitionIndices.Add(Definition.PartTag, DefinitionIndex);
	}

	USkeletalMeshComponent* OwnerMesh = ResolveOwnerMesh();
	if (bRequireSkeletalMesh && !IsValid(OwnerMesh))
	{
		LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr,
			TEXT("The owning robot has no valid SkeletalMeshComponent."));
		return ETDRobotPartSeparationResult::InternalProcessingFailed;
	}

	for (const FTDRobotPartDefinition& Definition : PartDataAsset->Parts)
	{
		if (Definition.ParentPartTag.IsValid())
		{
			if (Definition.ParentPartTag == Definition.PartTag)
			{
				LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
					TEXT("ParentPartTag refers to the part itself."));
				bConfigurationValid = false;
			}
			else if (!DefinitionIndices.Contains(Definition.ParentPartTag))
			{
				LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
					FString::Printf(TEXT("ParentPartTag '%s' does not exist."),
						*Definition.ParentPartTag.ToString()));
				bConfigurationValid = false;
			}
		}

		if (Definition.BoneName.IsNone())
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition, TEXT("BoneName is empty."));
			bConfigurationValid = false;
		}
		else if (IsValid(OwnerMesh) && OwnerMesh->GetBoneIndex(Definition.BoneName) == INDEX_NONE)
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
				FString::Printf(TEXT("BoneName '%s' does not exist on the robot SkeletalMesh."),
					*Definition.BoneName.ToString()));
			bConfigurationValid = false;
		}

		if (!IsValid(Definition.StaticMesh))
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition, TEXT("StaticMesh is empty."));
			bConfigurationValid = false;
		}
		else
		{
			const UBodySetup* BodySetup = Definition.StaticMesh->GetBodySetup();
			if (!BodySetup)
			{
				LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
					TEXT("StaticMesh has no BodySetup and cannot simulate physics."));
				bConfigurationValid = false;
			}
			else
			{
				if (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
				{
					LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
						TEXT("StaticMesh uses Complex Collision As Simple and cannot simulate physics."));
					bConfigurationValid = false;
				}
				if (BodySetup->AggGeom.GetElementCount() <= 0)
				{
					LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
						TEXT("StaticMesh has no Simple Collision geometry."));
					bConfigurationValid = false;
				}
			}
		}

		if (!Definition.MeshSpaceTransform.IsValid())
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
				TEXT("MeshSpaceTransform is invalid."));
			bConfigurationValid = false;
		}

		if (!FMath::IsFinite(Definition.MaxHealth) || Definition.MaxHealth <= 0.0f)
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, &Definition,
				TEXT("MaxHealth must be greater than zero."));
			bConfigurationValid = false;
		}
	}

	TSet<FGameplayTag> ReportedCycleParts;
	for (const FTDRobotPartDefinition& Definition : PartDataAsset->Parts)
	{
		if (!Definition.PartTag.IsValid())
		{
			continue;
		}

		TSet<FGameplayTag> CurrentPath;
		FGameplayTag CurrentPart = Definition.PartTag;
		while (CurrentPart.IsValid())
		{
			if (CurrentPath.Contains(CurrentPart))
			{
				if (!ReportedCycleParts.Contains(CurrentPart))
				{
					const int32* CycleIndex = DefinitionIndices.Find(CurrentPart);
					const FTDRobotPartDefinition* CycleDefinition = CycleIndex
						? &PartDataAsset->Parts[*CycleIndex]
						: &Definition;
					LogPartSeparationFailure(OwnerActor, RequestedPart, CycleDefinition,
						TEXT("A circular ParentPartTag hierarchy was detected."));
					ReportedCycleParts.Add(CurrentPart);
				}
				bConfigurationValid = false;
				break;
			}

			CurrentPath.Add(CurrentPart);
			const int32* CurrentIndex = DefinitionIndices.Find(CurrentPart);
			if (!CurrentIndex)
			{
				break;
			}
			CurrentPart = PartDataAsset->Parts[*CurrentIndex].ParentPartTag;
		}
	}

	TMap<FGameplayTag, const FTDRobotPartSeparationOverride*> OverrideByPart;
	for (const FTDRobotPartSeparationOverride& Override : PerPartSettingsOverrides)
	{
		if (!Override.PartTag.IsValid())
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr,
				TEXT("PerPartSettingsOverrides contains an invalid PartTag."));
			bConfigurationValid = false;
			continue;
		}
		if (!DefinitionIndices.Contains(Override.PartTag))
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart, nullptr,
				FString::Printf(TEXT("Settings override PartTag '%s' does not exist in the DataAsset."),
					*Override.PartTag.ToString()));
			bConfigurationValid = false;
			continue;
		}
		if (OverrideByPart.Contains(Override.PartTag))
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart,
				&PartDataAsset->Parts[DefinitionIndices[Override.PartTag]],
				TEXT("More than one settings override targets this part."));
			bConfigurationValid = false;
			continue;
		}
		if (Override.bOverrideSettings && !AreSettingsValid(Override.Settings, SettingsFailure))
		{
			LogPartSeparationFailure(OwnerActor, RequestedPart,
				&PartDataAsset->Parts[DefinitionIndices[Override.PartTag]],
				FString::Printf(TEXT("Per-part separation settings: %s"), *SettingsFailure));
			bConfigurationValid = false;
		}
		OverrideByPart.Add(Override.PartTag, &Override);
	}

	if (!bConfigurationValid)
	{
		return ETDRobotPartSeparationResult::ConfigurationError;
	}

	TMap<FGameplayTag, FTDRobotResolvedPart> NewResolvedParts;
	for (const FTDRobotPartDefinition& Definition : PartDataAsset->Parts)
	{
		FTDRobotResolvedPart& ResolvedPart = NewResolvedParts.Add(Definition.PartTag);
		ResolvedPart.Definition = &Definition;
		ResolvedPart.SeparationSettings = DefaultSeparationSettings;
		if (const FTDRobotPartSeparationOverride* const* Override = OverrideByPart.Find(Definition.PartTag))
		{
			if ((*Override)->bOverrideSettings)
			{
				ResolvedPart.SeparationSettings = (*Override)->Settings;
			}
		}

		FGameplayTag ParentPart = Definition.ParentPartTag;
		while (ParentPart.IsValid())
		{
			++ResolvedPart.HierarchyDepth;
			const int32* ParentIndex = DefinitionIndices.Find(ParentPart);
			ParentPart = ParentIndex
				? PartDataAsset->Parts[*ParentIndex].ParentPartTag
				: FGameplayTag();
		}
	}

	for (const FTDRobotPartDefinition& Definition : PartDataAsset->Parts)
	{
		if (Definition.ParentPartTag.IsValid())
		{
			NewResolvedParts.FindChecked(Definition.ParentPartTag).ChildPartTags.Add(Definition.PartTag);
		}
	}

	ResolvedParts = MoveTemp(NewResolvedParts);
	ResolvedDataAsset = PartDataAsset;
	bResolvedPartsValid = true;
	bSkeletalMeshValidated = IsValid(OwnerMesh);
	return ETDRobotPartSeparationResult::Succeeded;
}

ETDRobotPartSeparationResult UTDRobotPartSeparationComponent::DestroyRobotPart(
	FGameplayTag PartTag,
	AActor* Requester,
	FVector HitLocation,
	FVector HitDirection,
	float ImpulseMultiplier)
{
	if (!bSeparationEnabled)
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::ComponentDisabled,
			TEXT("bSeparationEnabled is false."));
	}

	if (!IsValid(PartDataAsset))
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::DataAssetNotSet,
			TEXT("PartDataAsset is not assigned."));
	}

	if (!PartTag.IsValid())
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::PartNotFound,
			TEXT("The requested PartTag is empty or invalid."));
	}

	if (!bResolvedPartsValid
		|| !bSkeletalMeshValidated
		|| ResolvedDataAsset.Get() != PartDataAsset)
	{
		const ETDRobotPartSeparationResult BuildResult = BuildResolvedParts(PartTag, true);
		if (BuildResult != ETDRobotPartSeparationResult::Succeeded)
		{
			return FailRequest(PartTag, Requester, BuildResult,
				TEXT("Runtime part data could not be resolved."));
		}
	}

	const FTDRobotResolvedPart* RequestedResolvedPart = ResolvedParts.Find(PartTag);
	if (!RequestedResolvedPart)
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::PartNotFound,
			TEXT("The requested PartTag does not exist in PartDataAsset."));
	}

	if (DestroyedParts.Contains(PartTag))
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::AlreadySeparated,
			TEXT("The requested part was already separated."), RequestedResolvedPart->Definition);
	}

	TArray<FGameplayTag> HierarchyParts;
	CollectHierarchy(PartTag, HierarchyParts);
	for (const FGameplayTag& HierarchyPart : HierarchyParts)
	{
		if (PartsBeingSeparated.Contains(HierarchyPart))
		{
			return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::InternalProcessingFailed,
				TEXT("The hierarchy is already being processed."),
				ResolvedParts.FindChecked(HierarchyPart).Definition);
		}
	}

	TArray<FGameplayTag> PlannedParts;
	for (const FGameplayTag& HierarchyPart : HierarchyParts)
	{
		if (!DestroyedParts.Contains(HierarchyPart))
		{
			PlannedParts.Add(HierarchyPart);
		}
	}

	if (PlannedParts.IsEmpty())
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::AlreadySeparated,
			TEXT("Every part in the requested hierarchy was already separated."),
			RequestedResolvedPart->Definition);
	}

	AActor* OwnerActor = GetOwner();
	USkeletalMeshComponent* OwnerMesh = ResolveOwnerMesh();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !IsValid(OwnerMesh) || !IsValid(World))
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::InternalProcessingFailed,
			TEXT("The owning robot, its SkeletalMeshComponent, or the World is invalid."),
			RequestedResolvedPart->Definition);
	}

	const USkeletalMesh* SkeletalMeshAsset = OwnerMesh->GetSkeletalMeshAsset();
	if (!IsValid(SkeletalMeshAsset))
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::InternalProcessingFailed,
			TEXT("The robot SkeletalMesh asset is unavailable."), RequestedResolvedPart->Definition);
	}

	// Validate the complete hierarchy and capture every transform before hiding any bone.
	TArray<FTDRobotPartSeparationWorkItem> WorkItems;
	WorkItems.Reserve(PlannedParts.Num());
	for (const FGameplayTag& PlannedPart : PlannedParts)
	{
		const FTDRobotResolvedPart* ResolvedPart = ResolvedParts.Find(PlannedPart);
		if (!ResolvedPart || !ResolvedPart->Definition || !IsValid(ResolvedPart->Definition->StaticMesh))
		{
			return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::ConfigurationError,
				TEXT("A resolved hierarchy part has invalid definition data."));
		}

		FTDRobotPartSeparationWorkItem& WorkItem = WorkItems.AddDefaulted_GetRef();
		WorkItem.Definition = ResolvedPart->Definition;
		WorkItem.Settings = ResolvedPart->SeparationSettings;
		WorkItem.BoneIndex = OwnerMesh->GetBoneIndex(WorkItem.Definition->BoneName);
		if (WorkItem.BoneIndex == INDEX_NONE)
		{
			return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::ConfigurationError,
				TEXT("The configured bone is unavailable at separation time."), WorkItem.Definition);
		}

		WorkItem.bBoneWasHidden = OwnerMesh->IsBoneHidden(WorkItem.BoneIndex);
		const FTransform ReferenceBoneComponentTransform =
			FAnimationRuntime::GetComponentSpaceTransformRefPose(
				SkeletalMeshAsset->GetRefSkeleton(), WorkItem.BoneIndex);
		const FTransform CurrentBoneWorldTransform = OwnerMesh->GetBoneTransform(WorkItem.BoneIndex);
		const FTransform RelativeToBone =
			WorkItem.Definition->MeshSpaceTransform * ReferenceBoneComponentTransform.Inverse();
		WorkItem.WorldTransform = RelativeToBone * CurrentBoneWorldTransform;
		WorkItem.WorldTransform.NormalizeRotation();

		if (!WorkItem.WorldTransform.IsValid())
		{
			return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::InternalProcessingFailed,
				TEXT("The calculated debris world transform is invalid."), WorkItem.Definition);
		}
	}

	if (WorkItems.Num() != PlannedParts.Num())
	{
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::InternalProcessingFailed,
			TEXT("The hierarchy work list is incomplete."), RequestedResolvedPart->Definition);
	}

	for (const FTDRobotPartSeparationWorkItem& WorkItem : WorkItems)
	{
		PartsBeingSeparated.Add(WorkItem.Definition->PartTag);
		OwnerMesh->HideBoneByName(WorkItem.Definition->BoneName, EPhysBodyOp::PBO_None);
	}

	for (FTDRobotPartSeparationWorkItem& WorkItem : WorkItems)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerActor;
		SpawnParameters.Instigator = Cast<APawn>(Requester);
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		WorkItem.DebrisActor = World->SpawnActor<ATDRobotPartDebrisActor>(
			ATDRobotPartDebrisActor::StaticClass(),
			WorkItem.WorldTransform,
			SpawnParameters);
		if (!IsValid(WorkItem.DebrisActor))
		{
			WorkItem.DebrisActor = nullptr;
			WorkItem.FailureDetail = TEXT("The debris actor could not be spawned.");
			break;
		}
	}

	const bool bAllActorsSpawned = WorkItems.ContainsByPredicate(
		[](const FTDRobotPartSeparationWorkItem& WorkItem)
		{
			return !IsValid(WorkItem.DebrisActor);
		}) == false;

	if (bAllActorsSpawned)
	{
		for (FTDRobotPartSeparationWorkItem& WorkItem : WorkItems)
		{
			if (!WorkItem.DebrisActor->InitializeDebris(
				WorkItem.Definition->StaticMesh,
				WorkItem.Settings,
				OwnerActor,
				HitLocation,
				HitDirection,
				ImpulseMultiplier))
			{
				WorkItem.FailureDetail = TEXT("The debris mesh could not start physics simulation.");
				break;
			}
		}
	}

	FTDRobotPartSeparationWorkItem* FailedWorkItem = WorkItems.FindByPredicate(
		[](const FTDRobotPartSeparationWorkItem& WorkItem)
		{
			return !WorkItem.FailureDetail.IsEmpty();
		});

	if (!bAllActorsSpawned || FailedWorkItem)
	{
		for (FTDRobotPartSeparationWorkItem& WorkItem : WorkItems)
		{
			if (IsValid(WorkItem.DebrisActor))
			{
				WorkItem.DebrisActor->Destroy();
				WorkItem.DebrisActor = nullptr;
			}
			PartsBeingSeparated.Remove(WorkItem.Definition->PartTag);
		}

		if (IsValid(OwnerMesh))
		{
			for (int32 WorkIndex = WorkItems.Num() - 1; WorkIndex >= 0; --WorkIndex)
			{
				const FTDRobotPartSeparationWorkItem& WorkItem = WorkItems[WorkIndex];
				if (!WorkItem.bBoneWasHidden)
				{
					OwnerMesh->UnHideBoneByName(WorkItem.Definition->BoneName);
				}
			}
		}

		const FTDRobotPartDefinition* FailedDefinition = FailedWorkItem
			? FailedWorkItem->Definition
			: RequestedResolvedPart->Definition;
		const FString FailureDetail = FailedWorkItem
			? FailedWorkItem->FailureDetail
			: TEXT("At least one debris actor could not be created.");
		return FailRequest(PartTag, Requester, ETDRobotPartSeparationResult::PhysicsCreationFailed,
			FailureDetail, FailedDefinition);
	}

	TArray<FGameplayTag> SeparatedParts;
	TArray<ATDRobotPartDebrisActor*> CreatedDebris;
	SeparatedParts.Reserve(WorkItems.Num());
	CreatedDebris.Reserve(WorkItems.Num());
	for (FTDRobotPartSeparationWorkItem& WorkItem : WorkItems)
	{
		const FGameplayTag SeparatedPart = WorkItem.Definition->PartTag;
		DestroyedParts.Add(SeparatedPart);
		PartsBeingSeparated.Remove(SeparatedPart);
		SeparatedParts.Add(SeparatedPart);
		CreatedDebris.Add(WorkItem.DebrisActor);
	}

	OnPartSeparationSucceeded.Broadcast(PartTag, Requester, SeparatedParts, CreatedDebris);
	return ETDRobotPartSeparationResult::Succeeded;
}

bool UTDRobotPartSeparationComponent::IsPartSeparated(FGameplayTag PartTag) const
{
	return DestroyedParts.Contains(PartTag) || PartsBeingSeparated.Contains(PartTag);
}

void UTDRobotPartSeparationComponent::CollectHierarchy(
	FGameplayTag RootPartTag,
	TArray<FGameplayTag>& OutParts) const
{
	OutParts.Reset();

	TSet<FGameplayTag> VisitedParts;
	TArray<FGameplayTag> PendingParts;
	PendingParts.Add(RootPartTag);
	for (int32 PendingIndex = 0; PendingIndex < PendingParts.Num(); ++PendingIndex)
	{
		const FGameplayTag CurrentPart = PendingParts[PendingIndex];
		if (VisitedParts.Contains(CurrentPart))
		{
			continue;
		}

		VisitedParts.Add(CurrentPart);
		OutParts.Add(CurrentPart);
		if (const FTDRobotResolvedPart* ResolvedPart = ResolvedParts.Find(CurrentPart))
		{
			for (const FGameplayTag& ChildPart : ResolvedPart->ChildPartTags)
			{
				PendingParts.Add(ChildPart);
			}
		}
	}
}

USkeletalMeshComponent* UTDRobotPartSeparationComponent::ResolveOwnerMesh() const
{
	if (const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		return CharacterOwner->GetMesh();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

ETDRobotPartSeparationResult UTDRobotPartSeparationComponent::FailRequest(
	FGameplayTag RequestedPart,
	AActor* Requester,
	ETDRobotPartSeparationResult FailureReason,
	const FString& Detail,
	const FTDRobotPartDefinition* FailedDefinition)
{
	LogPartSeparationFailure(GetOwner(), RequestedPart, FailedDefinition, Detail, ELogVerbosity::Warning);
	OnPartSeparationFailed.Broadcast(RequestedPart, Requester, FailureReason);
	return FailureReason;
}
