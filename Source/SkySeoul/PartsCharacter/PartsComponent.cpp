#include "PartsComponent.h"
#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
static TAutoConsoleVariable<int32> CVarDMPartsDiag(TEXT("dm.PartsDiag"), 0,
    TEXT("0 off; 1 log head initialization, actual destruction definition and death exclusion. No per-tick logging."), ECVF_Default);
#endif

#include "PartsSetupData.h"
#include "PartsCharacterBase.h"
#include "Character/CharacterBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/CollisionProfile.h"
#include "AnimationRuntime.h"

namespace PartsDebrisPrivate
{
	static void ConfigureDebrisPhysicsCollision(UPrimitiveComponent* Prim)
	{
		if (!Prim)
		{
			return;
		}

		Prim->SetMobility(EComponentMobility::Movable);
		Prim->SetCollisionProfileName(TEXT("PhysicsActor"));
		Prim->SetCollisionObjectType(ECC_PhysicsBody);
		Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Prim->SetCollisionResponseToAllChannels(ECR_Block);
		Prim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Prim->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Prim->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Prim->SetGenerateOverlapEvents(false);
		Prim->SetNotifyRigidBodyCollision(false);
		Prim->SetEnableGravity(true);
		Prim->SetUseCCD(true);
		Prim->SetVisibility(true);
		Prim->SetHiddenInGame(false);
	}

	static void IgnoreOwnerCollision(UPrimitiveComponent* PhysicsRoot, AActor* IgnoreOwner)
	{
		if (!PhysicsRoot || !IgnoreOwner)
		{
			return;
		}

		PhysicsRoot->IgnoreActorWhenMoving(IgnoreOwner, true);
		if (ACharacter* Char = Cast<ACharacter>(IgnoreOwner))
		{
			if (USkeletalMeshComponent* Skel = Char->GetMesh())
			{
				PhysicsRoot->IgnoreComponentWhenMoving(Skel, true);
				Skel->IgnoreComponentWhenMoving(PhysicsRoot, true);
			}
			if (UCapsuleComponent* Cap = Char->GetCapsuleComponent())
			{
				PhysicsRoot->IgnoreComponentWhenMoving(Cap, true);
				Cap->IgnoreComponentWhenMoving(PhysicsRoot, true);
			}
		}
	}

	static void ApplyDebrisImpulse(UPrimitiveComponent* PhysicsRoot, float ImpulseStrength, const FVector& ImpulseOrigin)
	{
		if (!PhysicsRoot)
		{
			return;
		}

		PhysicsRoot->SetSimulatePhysics(true);
		PhysicsRoot->WakeRigidBody();
		PhysicsRoot->SetPhysicsMaxAngularVelocityInDegrees(1440.0f);
		PhysicsRoot->SetUseCCD(true);

		if (!PhysicsRoot->IsSimulatingPhysics())
		{
			UE_LOG(LogTemp, Warning, TEXT("Parts debris: SimulatePhysics still false after setup (%s)."),
				*GetNameSafe(PhysicsRoot));
			return;
		}

		if (ImpulseStrength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FVector Outward = (PhysicsRoot->GetComponentLocation() - ImpulseOrigin).GetSafeNormal();
		const FVector ImpulseDir = Outward.IsNearlyZero()
			? FMath::VRand()
			: (Outward + FMath::VRand() * 0.45f).GetSafeNormal();

		PhysicsRoot->AddImpulse(ImpulseDir * ImpulseStrength, NAME_None, true);
		PhysicsRoot->AddAngularImpulseInDegrees(FMath::VRand() * (ImpulseStrength * 0.4f), NAME_None, true);
		PhysicsRoot->AddImpulse(FVector(0.0f, 0.0f, ImpulseStrength * 0.35f), NAME_None, true);
	}

	static void ScheduleDebrisImpulse(UWorld* World, UPrimitiveComponent* PhysicsRoot, float ImpulseStrength, const FVector& ImpulseOrigin)
	{
		if (!World || !PhysicsRoot)
		{
			return;
		}

		ApplyDebrisImpulse(PhysicsRoot, ImpulseStrength, ImpulseOrigin);

		TWeakObjectPtr<UPrimitiveComponent> WeakPhys(PhysicsRoot);
		const FVector OriginCopy = ImpulseOrigin;
		const float StrengthCopy = ImpulseStrength;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[WeakPhys, OriginCopy, StrengthCopy]()
			{
				if (UPrimitiveComponent* Phys = WeakPhys.Get())
				{
					if (!Phys->IsSimulatingPhysics()
						|| (StrengthCopy > KINDA_SMALL_NUMBER && Phys->GetPhysicsLinearVelocity().SizeSquared() < 1.0f))
					{
						ApplyDebrisImpulse(Phys, StrengthCopy, OriginCopy);
					}
				}
			}));
	}

	/**
	 * Spawn a world debris actor: Box physics root + copied visual mesh.
	 * Original attached mesh is hidden/destroyed after a successful spawn (no Rename — Rename was unreliable).
	 */
	static AActor* SpawnDebrisCopyFromPartMesh(
		UStaticMeshComponent* SourceComp,
		AActor* IgnoreOwner,
		float LifeSpan,
		float ImpulseStrength,
		const FVector& ImpulseOrigin)
	{
		if (!SourceComp)
		{
			return nullptr;
		}

		UWorld* World = SourceComp->GetWorld();
		UStaticMesh* Mesh = SourceComp->GetStaticMesh();
		if (!World || !Mesh)
		{
			UE_LOG(LogTemp, Error, TEXT("Parts debris: missing World/StaticMesh on '%s'."), *GetNameSafe(SourceComp));
			return nullptr;
		}

		const FTransform WorldTM = SourceComp->GetComponentTransform();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;

		AActor* Debris = World->SpawnActor<AActor>(AActor::StaticClass(), WorldTM, Params);
		if (!Debris)
		{
			UE_LOG(LogTemp, Error, TEXT("Parts debris: SpawnActor failed for '%s'."), *Mesh->GetName());
			return nullptr;
		}

#if WITH_EDITOR
		Debris->SetActorLabel(FString::Printf(TEXT("PartsDebris_%s"), *Mesh->GetName()));
#endif
		Debris->Tags.AddUnique(PartsHitCollision::DetachedDebrisTag);
		Debris->SetCanBeDamaged(false);
		Debris->SetActorHiddenInGame(false);
		Debris->SetActorEnableCollision(true);

		FVector BoxExtent = Mesh->GetBounds().BoxExtent.ComponentMax(FVector(12.0f)) * 1.15f;

		UBoxComponent* Box = NewObject<UBoxComponent>(Debris, TEXT("DebrisPhys"));
		Debris->SetRootComponent(Box);
		Debris->AddInstanceComponent(Box);
		Box->SetBoxExtent(BoxExtent);
		Box->SetWorldTransform(WorldTM);
		ConfigureDebrisPhysicsCollision(Box);
		Box->RegisterComponent();

		UStaticMeshComponent* Vis = NewObject<UStaticMeshComponent>(Debris, TEXT("DebrisMesh"));
		Vis->SetupAttachment(Box);
		Debris->AddInstanceComponent(Vis);
		Vis->SetMobility(EComponentMobility::Movable);
		Vis->SetStaticMesh(Mesh);
		Vis->SetRelativeTransform(FTransform::Identity);
		for (int32 MatIdx = 0; MatIdx < SourceComp->GetNumMaterials(); ++MatIdx)
		{
			Vis->SetMaterial(MatIdx, SourceComp->GetMaterial(MatIdx));
		}
		Vis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Vis->SetGenerateOverlapEvents(false);
		Vis->SetVisibility(true);
		Vis->SetHiddenInGame(false);
		Vis->RegisterComponent();

		IgnoreOwnerCollision(Box, IgnoreOwner);

		Box->SetSimulatePhysics(true);
		Box->WakeRigidBody();
		Box->SetUseCCD(true);
		ScheduleDebrisImpulse(World, Box, ImpulseStrength, ImpulseOrigin);

		const float ClampedLife = FMath::Max(LifeSpan, 0.5f);
		Debris->SetLifeSpan(ClampedLife);

		// Remove the attached original only after debris exists.
		SourceComp->SetSimulatePhysics(false);
		SourceComp->SetVisibility(false);
		SourceComp->SetHiddenInGame(true);
		SourceComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SourceComp->DestroyComponent();

		UE_LOG(LogTemp, Warning,
			TEXT("Parts debris COPY '%s' at %s box=(%.0f,%.0f,%.0f) life=%.1fs impulse=%.0f sim=%d"),
			*Mesh->GetName(),
			*WorldTM.GetLocation().ToCompactString(),
			BoxExtent.X, BoxExtent.Y, BoxExtent.Z,
			ClampedLife,
			ImpulseStrength,
			Box->IsSimulatingPhysics() ? 1 : 0);

		return Debris;
	}
}

namespace PartsAttachPrivate
{
	/** Resolve FollowBone against component bone map first (case-safe), then ref skeleton. */
	static bool TryGetRefPoseBoneComponentSpace(const USkeletalMeshComponent* Mesh, FName BoneName, FTransform& OutCS)
	{
		OutCS = FTransform::Identity;
		if (!Mesh || BoneName.IsNone())
		{
			return false;
		}

		const USkeletalMesh* SkelMesh = Mesh->GetSkeletalMeshAsset();
		if (!SkelMesh)
		{
			return false;
		}

		const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();

		// Prefer runtime bone name (casing), then map into ref skeleton indices.
		int32 MeshBoneIndex = Mesh->GetBoneIndex(BoneName);
		FName ResolvedName = BoneName;
		if (MeshBoneIndex != INDEX_NONE)
		{
			ResolvedName = Mesh->GetBoneName(MeshBoneIndex);
		}

		const int32 RefBoneIndex = RefSkel.FindBoneIndex(ResolvedName);
		if (RefBoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Parts: REF pose bone '%s' (resolved '%s') not in ref skeleton of %s"),
				*BoneName.ToString(),
				*ResolvedName.ToString(),
				*GetNameSafe(SkelMesh));
			return false;
		}

		// Engine-composed component-space ref pose (avoids hand-rolled parent chain mistakes).
		TArray<FTransform> RefCSPose;
		FAnimationRuntime::FillUpComponentSpaceTransforms(
			RefSkel,
			RefSkel.GetRefBonePose(),
			RefCSPose);

		if (!RefCSPose.IsValidIndex(RefBoneIndex))
		{
			return false;
		}

		OutCS = RefCSPose[RefBoneIndex];
		return true;
	}

	static FName ResolveAttachBoneName(const USkeletalMeshComponent* Mesh, FName BoneName)
	{
		if (!Mesh || BoneName.IsNone())
		{
			return NAME_None;
		}
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			return Mesh->GetBoneName(BoneIndex);
		}
		return BoneName;
	}

	static FTransform GetUnscaledBoneWorld(const USkeletalMeshComponent* Mesh, FName BoneName)
	{
		FTransform BoneWorld = FTransform::Identity;
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			BoneWorld = Mesh->GetBoneTransform(BoneIndex);
		}
		else
		{
			BoneWorld = Mesh->GetSocketTransform(BoneName, RTS_World);
		}
		BoneWorld.SetScale3D(FVector::OneVector);
		return BoneWorld;
	}

	/** Ref/bind pose bone in world — no live-bone fallback (that reintroduces bind-pose parts). */
	static bool TryGetUnscaledRefPoseBoneWorld(const USkeletalMeshComponent* Mesh, FName BoneName, FTransform& OutWorld)
	{
		FTransform BoneCS;
		if (!TryGetRefPoseBoneComponentSpace(Mesh, BoneName, BoneCS))
		{
			return false;
		}
		OutWorld = BoneCS * Mesh->GetComponentTransform();
		OutWorld.SetScale3D(FVector::OneVector);
		return true;
	}

	static bool TryGetUnscaledAnimBoneComponentSpace(const USkeletalMeshComponent* Mesh, FName BoneName, FTransform& OutCS)
	{
		OutCS = FTransform::Identity;
		if (!Mesh || BoneName.IsNone())
		{
			return false;
		}
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return false;
		}
		const TArray<FTransform>& CS = Mesh->GetComponentSpaceTransforms();
		if (!CS.IsValidIndex(BoneIndex))
		{
			return false;
		}
		OutCS = CS[BoneIndex];
		OutCS.SetScale3D(FVector::OneVector);
		return true;
	}

	/** PartCS = MeshSpaceBind * Inv(RefBoneCS) * AnimBoneCS  →  world */
	static FTransform MakePartWorldFromBindAndBones(
		const USkeletalMeshComponent* Mesh,
		const FTransform& MeshSpaceBind,
		const FTransform& RefBoneCS,
		const FTransform& AnimBoneCS)
	{
		const FTransform PartCS = MeshSpaceBind * RefBoneCS.Inverse() * AnimBoneCS;
		FTransform PartWorld = PartCS * Mesh->GetComponentTransform();
		PartWorld.SetScale3D(FVector::OneVector);
		return PartWorld;
	}

	/**
	 * WHY KeepWorld-to-bone alone can look "stuck":
	 * 1) HideBoneByName zeroes bone scale → socket children inherit scale 0 / stop updating correctly.
	 * 2) Even without HideBone, SkeletalMesh UpdateChildTransforms can skip SM children
	 *    (URO / parallel anim finalize) — AttachSocketName stays set but world pose never refreshes.
	 *
	 * RelToBone MUST be baked vs REF pose bone, not the live idle bone:
	 *   SM parts are mesh-space Identity in bind pose. Baking vs idle bone leaves parts in bind pose
	 *   while only breathing deltas follow (exactly the "idle이 아닌데 들썩임" bug).
	 *
	 * Drive: PartWorld = RelToBone * UnscaledBoneWorld(animated) every tick (Absolute).
	 */
	static bool AttachPartToFollowBone(
		USkeletalMeshComponent* Mesh,
		UStaticMeshComponent* PartComp,
		const FPartsDefinition& Def,
		FName& OutSocket,
		FTransform& OutMeshSpaceBind,
		FTransform& OutRelToBone)
	{
		OutSocket = NAME_None;
		OutMeshSpaceBind = FTransform::Identity;
		OutRelToBone = FTransform::Identity;

		if (!Mesh || !PartComp || Def.FollowBoneName.IsNone())
		{
			return false;
		}

		const FName Socket = ResolveAttachBoneName(Mesh, Def.FollowBoneName);
		if (Socket.IsNone()
			|| (!Mesh->DoesSocketExist(Socket) && Mesh->GetBoneIndex(Socket) == INDEX_NONE))
		{
			UE_LOG(LogTemp, Error,
				TEXT("Parts: bone '%s' missing on %s"),
				*Def.FollowBoneName.ToString(),
				*GetNameSafe(Mesh));
			return false;
		}

		Mesh->RefreshBoneTransforms();
		Mesh->UpdateComponentToWorld();

		FTransform RefBoneCS;
		if (!TryGetRefPoseBoneComponentSpace(Mesh, Socket, RefBoneCS))
		{
			UE_LOG(LogTemp, Error,
				TEXT("Parts: REF pose bake FAILED for '%s' — cannot attach (live-bone bake causes bind-pose look)"),
				*Socket.ToString());
			return false;
		}

		FTransform AnimBoneCS;
		if (!TryGetUnscaledAnimBoneComponentSpace(Mesh, Socket, AnimBoneCS))
		{
			UE_LOG(LogTemp, Error, TEXT("Parts: live bone CS missing for '%s'"), *Socket.ToString());
			return false;
		}

		// Mesh-space bind: SK→SM Identity under mesh. Bone-local DA offsets converted via ref bone.
		if (Def.bAttachTransformIsBoneLocal)
		{
			OutMeshSpaceBind = Def.AttachRelativeTransform * RefBoneCS;
		}
		else
		{
			OutMeshSpaceBind = Def.AttachRelativeTransform;
		}

		OutRelToBone = OutMeshSpaceBind * RefBoneCS.Inverse();
		const FTransform DesiredWorld = MakePartWorldFromBindAndBones(Mesh, OutMeshSpaceBind, RefBoneCS, AnimBoneCS);
		OutSocket = Socket;

		PartComp->SetUsingAbsoluteLocation(false);
		PartComp->SetUsingAbsoluteRotation(false);
		PartComp->SetUsingAbsoluteScale(false);
		PartComp->AttachToComponent(
			Mesh,
			FAttachmentTransformRules::KeepRelativeTransform,
			NAME_None);
		PartComp->SetUsingAbsoluteLocation(true);
		PartComp->SetUsingAbsoluteRotation(true);
		PartComp->SetUsingAbsoluteScale(true);
		PartComp->SetWorldTransform(DesiredWorld);
		PartComp->UpdateComponentToWorld();

		FTransform RefBoneWorld = RefBoneCS * Mesh->GetComponentTransform();
		RefBoneWorld.SetScale3D(FVector::OneVector);
		const FTransform LiveBoneWorld = GetUnscaledBoneWorld(Mesh, Socket);
		UE_LOG(LogTemp, Warning,
			TEXT("Parts: RelToBone '%s' REF↔LIVE delta=%.1fcm boneLocal=%d bind=%s"),
			*Socket.ToString(),
			FVector::Dist(RefBoneWorld.GetLocation(), LiveBoneWorld.GetLocation()),
			Def.bAttachTransformIsBoneLocal ? 1 : 0,
			*OutMeshSpaceBind.GetLocation().ToCompactString());
		return true;
	}
}

UPartsComponent::UPartsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After skeletal mesh pose update so FollowBone transforms are current.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UPartsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyBoneFollowTransforms();
}

void UPartsComponent::ApplyBoneFollowTransforms()
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return;
	}

	// After mesh pose: HideBone re-zeroes scale every update — keep unit scale so fingers remain.
	RestoreHiddenBoneScalesForDescendants();

	static int32 DiagFrame = 0;
	++DiagFrame;
	const bool bDiag = (DiagFrame <= 90) && ((DiagFrame % 30) == 0);

	for (TPair<FGameplayTag, FPartsRuntimeState>& Pair : PartsRuntime)
	{
		FPartsRuntimeState& State = Pair.Value;
		if (!State.bFollowBoneAnim || !State.MeshComponent || State.FollowSocket.IsNone())
		{
			continue;
		}

		UStaticMeshComponent* PartComp = State.MeshComponent;

		FTransform RefBoneCS;
		FTransform AnimBoneCS;
		if (PartsAttachPrivate::TryGetRefPoseBoneComponentSpace(OwnerMesh, State.FollowSocket, RefBoneCS)
			&& PartsAttachPrivate::TryGetUnscaledAnimBoneComponentSpace(OwnerMesh, State.FollowSocket, AnimBoneCS))
		{
			const FTransform PartWorld = PartsAttachPrivate::MakePartWorldFromBindAndBones(
				OwnerMesh, State.MeshSpaceBind, RefBoneCS, AnimBoneCS);
			PartComp->SetWorldTransform(PartWorld);
		}
		else
		{
			const FTransform BoneWorld = PartsAttachPrivate::GetUnscaledBoneWorld(OwnerMesh, State.FollowSocket);
			PartComp->SetWorldTransform(State.RelToBone * BoneWorld);
		}
		PartComp->UpdateComponentToWorld();

		if (bDiag && State.FollowSocket == FName(TEXT("upperarm_l")))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Parts DIAG frame=%d upperarm_l partLoc=%s refYaw=%.1f animYaw=%.1f"),
				DiagFrame,
				*PartComp->GetComponentLocation().ToCompactString(),
				RefBoneCS.Rotator().Yaw,
				AnimBoneCS.Rotator().Yaw);
		}
	}
}

void UPartsComponent::StartBoneFollowTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(BoneFollowTimerHandle);
	World->GetTimerManager().SetTimer(
		BoneFollowTimerHandle,
		this,
		&UPartsComponent::ApplyBoneFollowTransforms,
		0.0f,
		true);
}

void UPartsComponent::StopBoneFollowTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BoneFollowTimerHandle);
	}
}

void UPartsComponent::BeginPlay()
{
	Super::BeginPlay();

	// APartsCharacterBase assigns SetupData then calls InitializeParts.
	// Avoid initializing here first with null SetupData (would wipe / race).
	if (!bInitialized && GetOwner() && !GetOwner()->IsA(APartsCharacterBase::StaticClass()))
	{
		InitializeParts();
	}
}

void UPartsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredInitTimerHandle);
		World->GetTimerManager().ClearTimer(PostAttachReconcileTimerHandle);
		World->GetTimerManager().ClearTimer(BoneFollowTimerHandle);
	}
	if (bBoundOwnerAnimInitialized)
	{
		if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
		{
			OwnerMesh->OnAnimInitialized.RemoveDynamic(this, &UPartsComponent::HandleOwnerAnimInitialized);
		}
		bBoundOwnerAnimInitialized = false;
	}
	DestroySpawnedMeshes();
	Super::EndPlay(EndPlayReason);
}

USkeletalMeshComponent* UPartsComponent::GetOwnerMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return nullptr;
}

void UPartsComponent::BuildDefinitionList()
{
	ActiveDefinitions.Reset();

	if (SetupData && SetupData->Parts.Num() > 0)
	{
		ActiveDefinitions = SetupData->Parts;

		// DA often has PartTag + PartMesh but empty FollowBoneName — fill bones from DM defaults by tag.
		TArray<FPartsDefinition> Defaults;
		UPartsSetupData::FillDefaultDeathMatchParts(Defaults);
		for (FPartsDefinition& Def : ActiveDefinitions)
		{
			if (!Def.FollowBoneName.IsNone() || Def.HitBoneAliases.Num() > 0)
			{
				continue;
			}
			for (const FPartsDefinition& DefaultDef : Defaults)
			{
				if (DefaultDef.PartTag == Def.PartTag)
				{
					Def.FollowBoneName = DefaultDef.FollowBoneName;
					Def.HitBoneAliases = DefaultDef.HitBoneAliases;
					if (Def.HideBonesOnAttach.Num() == 0)
					{
						Def.HideBonesOnAttach = DefaultDef.HideBonesOnAttach;
					}
					UE_LOG(LogTemp, Warning,
						TEXT("Parts: %s row '%s' had empty bone names — filled from DM defaults ('%s')."),
						*GetNameSafe(GetOwner()),
						*Def.PartTag.ToString(),
						*Def.FollowBoneName.ToString());
					break;
				}
			}
		}
	}
	else if (bUseDefaultDeathMatchPartsIfNoSetupData)
	{
		UPartsSetupData::FillDefaultDeathMatchParts(ActiveDefinitions);
	}
#if !UE_BUILD_SHIPPING
    if (CVarDMPartsDiag.GetValueOnGameThread() != 0)
    {
        int32 Count = 0;
        for (const auto& D : ActiveDefinitions) if (D.PartTag.GetTagName() == TEXT("Parts.Head")) ++Count;
        UE_LOG(LogTemp, Display, TEXT("DMPartsDiag Init Owner=%s Component=%s Setup=%s Default=%d HeadRows=%d"),
            *GetNameSafe(GetOwner()), *GetName(), *GetPathNameSafe(SetupData),
            (!SetupData || SetupData->Parts.IsEmpty()) && bUseDefaultDeathMatchPartsIfNoSetupData, Count);
        for (int32 I = 0; I < ActiveDefinitions.Num(); ++I)
        {
            const auto& D = ActiveDefinitions[I];
            if (D.PartTag.GetTagName() != TEXT("Parts.Head")) continue;
            UE_LOG(LogTemp, Display, TEXT("DMPartsDiag Head Index=%d Separable=%d Mesh=%s Bone=%s Parent=%s Impair=%d"),
                I, D.bSeparable, *D.PartMesh.ToSoftObjectPath().ToString(), *D.FollowBoneName.ToString(), *D.ParentTag.ToString(), D.bImpairHeadOnDestroy);
        }
    }
#endif
}

TArray<int32> UPartsComponent::BuildAttachOrder() const
{
	TArray<int32> Order;
	Order.Reserve(ActiveDefinitions.Num());

	TSet<FGameplayTag> Placed;
	int32 Guard = 0;
	const int32 MaxGuard = ActiveDefinitions.Num() * ActiveDefinitions.Num() + 1;

	while (Order.Num() < ActiveDefinitions.Num() && Guard++ < MaxGuard)
	{
		bool bProgress = false;
		for (int32 Index = 0; Index < ActiveDefinitions.Num(); ++Index)
		{
			if (Order.Contains(Index))
			{
				continue;
			}

			const FPartsDefinition& Def = ActiveDefinitions[Index];
			const bool bRoot = !Def.ParentTag.IsValid();
			if (bRoot || Placed.Contains(Def.ParentTag))
			{
				Order.Add(Index);
				Placed.Add(Def.PartTag);
				bProgress = true;
			}
		}

		if (!bProgress)
		{
			// Broken parent links: append remaining as roots.
			for (int32 Index = 0; Index < ActiveDefinitions.Num(); ++Index)
			{
				if (!Order.Contains(Index))
				{
					Order.Add(Index);
				}
			}
			break;
		}
	}

	return Order;
}

void UPartsComponent::DestroySpawnedMeshes()
{
	for (TPair<FGameplayTag, FPartsRuntimeState>& Pair : PartsRuntime)
	{
		if (UStaticMeshComponent* Comp = Pair.Value.MeshComponent)
		{
			Comp->DestroyComponent();
			Pair.Value.MeshComponent = nullptr;
		}
	}
	PartsRuntime.Reset();
	BoneToPartMap.Reset();
	bInitialized = false;
}

void UPartsComponent::RegisterBoneAlias(FName BoneOrSocketName, FGameplayTag PartTag)
{
	if (BoneOrSocketName.IsNone() || !PartTag.IsValid())
	{
		return;
	}
	// Keep keys lower-case — ResolvePartTagFromBone also lower-cases lookups.
	const FName Key(*BoneOrSocketName.ToString().ToLower());
	BoneToPartMap.FindOrAdd(Key) = PartTag;
}

void UPartsComponent::RebuildBoneToPartMap()
{
	BoneToPartMap.Reset();
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (!Def.PartTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Parts: %s has a Parts row with invalid PartTag — skipped."),
				*GetNameSafe(GetOwner()));
			continue;
		}

		if (Def.FollowBoneName.IsNone() && Def.HitBoneAliases.Num() == 0)
		{
			UE_LOG(LogTemp, Error,
				TEXT("Parts: %s row '%s' has empty FollowBoneName and HitBoneAliases — hits will never map. Set FollowBoneName (e.g. upperarm_l)."),
				*GetNameSafe(GetOwner()),
				*Def.PartTag.ToString());
		}

		RegisterBoneAlias(Def.FollowBoneName, Def.PartTag);
		for (const FName& Alias : Def.HitBoneAliases)
		{
			RegisterBoneAlias(Alias, Def.PartTag);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Parts: %s bone map (%d entries):"), *GetNameSafe(GetOwner()), BoneToPartMap.Num());
	for (const TPair<FName, FGameplayTag>& Pair : BoneToPartMap)
	{
		UE_LOG(LogTemp, Log, TEXT("  '%s' -> %s"), *Pair.Key.ToString(), *Pair.Value.ToString());
	}

	// Ensure standard DM bones resolve even if DA FollowBoneName is wrong/empty.
	auto EnsureBone = [this](const TCHAR* TagName, const TArray<FName>& Bones)
	{
		const FGameplayTag PartTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		if (!PartTag.IsValid() || FindDefinition(PartTag) == nullptr)
		{
			return;
		}
		for (const FName Bone : Bones)
		{
			RegisterBoneAlias(Bone, PartTag);
		}
	};

	EnsureBone(TEXT("Parts.Head"), { FName(TEXT("head")), FName(TEXT("Head")), FName(TEXT("head_01")) });
	EnsureBone(TEXT("Parts.Arm.L.Upper"), { FName(TEXT("upperarm_l")), FName(TEXT("upper_arm_l")), FName(TEXT("UpperArm_L")), FName(TEXT("clavicle_l")) });
	EnsureBone(TEXT("Parts.Arm.L.Lower"), { FName(TEXT("lowerarm_l")), FName(TEXT("lower_arm_l")), FName(TEXT("LowerArm_L")), FName(TEXT("elbow_l")) });
	EnsureBone(TEXT("Parts.Arm.L.Hand"), { FName(TEXT("hand_l")), FName(TEXT("Hand_L")), FName(TEXT("wrist_l")) });
	EnsureBone(TEXT("Parts.Arm.R.Upper"), { FName(TEXT("upperarm_r")), FName(TEXT("upper_arm_r")), FName(TEXT("UpperArm_R")), FName(TEXT("clavicle_r")) });
	EnsureBone(TEXT("Parts.Arm.R.Lower"), { FName(TEXT("lowerarm_r")), FName(TEXT("lower_arm_r")), FName(TEXT("LowerArm_R")), FName(TEXT("elbow_r")) });
	EnsureBone(TEXT("Parts.Arm.R.Hand"), { FName(TEXT("hand_r")), FName(TEXT("Hand_R")), FName(TEXT("wrist_r")) });

	// Last resort: DA produced no bone keys — inject full DM defaults.
	if (BoneToPartMap.Num() == 0 && bUseDefaultDeathMatchPartsIfNoSetupData)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Parts: %s bone map empty after DA load — injecting DM default definitions. Fix DA PartTag / FollowBoneName."),
			*GetNameSafe(GetOwner()));

		TArray<FPartsDefinition> Defaults;
		UPartsSetupData::FillDefaultDeathMatchParts(Defaults);
		for (const FPartsDefinition& Def : Defaults)
		{
			RegisterBoneAlias(Def.FollowBoneName, Def.PartTag);
			for (const FName& Alias : Def.HitBoneAliases)
			{
				RegisterBoneAlias(Alias, Def.PartTag);
			}
			if (FindDefinition(Def.PartTag) == nullptr)
			{
				ActiveDefinitions.Add(Def);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Parts: %s final bone map (%d entries)"), *GetNameSafe(GetOwner()), BoneToPartMap.Num());
}

void UPartsComponent::ConfigureOwnerMeshForWeaponTrace()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();

	// Capsule often Blocks HitTrace (Pawn profile) but has no BoneName — ignore weapon channel only.
	// Do NOT ignore ECC_Pawn on capsule or characters walk through each other.
	if (OwnerCharacter)
	{
		if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
		{
			Capsule->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Ignore);
		}
	}

	if (!OwnerMesh) return;

	// Need Query so LineTrace can hit PhAT bodies and fill BoneName.
	if (!OwnerMesh->IsQueryCollisionEnabled())
	{
		OwnerMesh->SetCollisionEnabled(OwnerMesh->IsPhysicsCollisionEnabled()
            ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);
	}
	OwnerMesh->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block);
}

void UPartsComponent::CollectAttachedPartComponents(TArray<UPrimitiveComponent*>& OutComponents) const
{
	OutComponents.Reset();
	for (const TPair<FGameplayTag, FPartsRuntimeState>& Pair : PartsRuntime)
	{
		if (UStaticMeshComponent* Comp = Pair.Value.MeshComponent)
		{
			if (!Pair.Value.bDestroyed)
			{
				OutComponents.Add(Comp);
			}
		}
	}
}

bool UPartsComponent::IsOwnerMeshReadyForAttach() const
{
	const USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || !OwnerMesh->GetSkeletalMeshAsset())
	{
		return false;
	}
	if (OwnerMesh->GetNumBones() <= 0)
	{
		return false;
	}

	// Cold editor reopen: bone count can be >0 before AnimInstance / CS poses exist.
	// Live Coding mid-PIE already has AnimInstance → attach "magically" works after compile.
	if (!OwnerMesh->GetAnimInstance())
	{
		return false;
	}

	const TArray<FTransform>& CSTransforms = OwnerMesh->GetComponentSpaceTransforms();
	if (CSTransforms.Num() < OwnerMesh->GetNumBones())
	{
		return false;
	}

	// Every authored FollowBone must resolve with a usable ref-pose — socket existence alone is not enough
	// (cold reopen can report DoesSocketExist while ref-pose / attach table still lags).
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (Def.FollowBoneName.IsNone())
		{
			continue;
		}
		if (!OwnerMesh->DoesSocketExist(Def.FollowBoneName)
			&& OwnerMesh->GetBoneIndex(Def.FollowBoneName) == INDEX_NONE)
		{
			return false;
		}
		FTransform BoneRefCS;
		if (!PartsAttachPrivate::TryGetRefPoseBoneComponentSpace(OwnerMesh, Def.FollowBoneName, BoneRefCS))
		{
			return false;
		}
	}
	return true;
}

void UPartsComponent::ScheduleDeferredInitializeParts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BindOwnerAnimInitialized();

	World->GetTimerManager().ClearTimer(DeferredInitTimerHandle);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UPartsComponent::DeferredInitializePartsTick));
}

void UPartsComponent::BindOwnerAnimInitialized()
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || bBoundOwnerAnimInitialized)
	{
		return;
	}

	bBoundOwnerAnimInitialized = true;
	// FOnAnimInitialized is DYNAMIC — AddLambda is invalid; must use AddDynamic + UFUNCTION.
	OwnerMesh->OnAnimInitialized.AddDynamic(this, &UPartsComponent::HandleOwnerAnimInitialized);
}

void UPartsComponent::HandleOwnerAnimInitialized()
{
	// AnimInstance just came online (typical cold PIE / editor reopen). Attach now or rebuild bad attaches.
	if (!bInitialized)
	{
		bAttachSettleFramePending = false;
		DeferredInitAttempts = 0;
		InitializeParts();
		return;
	}

	if (!AreAttachedPartsOnExpectedBones())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Parts: %s OnAnimInitialized — parts were not on expected bones; re-attaching."),
			*GetNameSafe(GetOwner()));
		bInitialized = false;
		bAttachSettleFramePending = false;
		DeferredInitAttempts = 0;
		InitializeParts();
	}
}

bool UPartsComponent::AreAttachedPartsOnExpectedBones() const
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (Def.FollowBoneName.IsNone() || Def.PartMesh.IsNull())
		{
			continue;
		}

		const FPartsRuntimeState* State = PartsRuntime.Find(Def.PartTag);
		if (!State || !State->MeshComponent || State->bDestroyed)
		{
			continue;
		}

		// Tick-follow parts intentionally use mesh root (empty socket) + Absolute.
		if (State->bFollowBoneAnim)
		{
			if (State->MeshComponent->GetAttachParent() != OwnerMesh)
			{
				return false;
			}
			continue;
		}

		const FName Expected = PartsAttachPrivate::ResolveAttachBoneName(OwnerMesh, Def.FollowBoneName);
		if (!Expected.IsNone() && State->MeshComponent->GetAttachSocketName() != Expected)
		{
			return false;
		}
	}
	return true;
}

void UPartsComponent::DeferredInitializePartsTick()
{
	if (bInitialized || !IsValid(this))
	{
		return;
	}

	++DeferredInitAttempts;
	if (!IsOwnerMeshReadyForAttach() && DeferredInitAttempts < MaxDeferredInitAttempts)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("Parts: %s mesh/bones not ready — defer attach (attempt %d/%d)."),
			*GetNameSafe(GetOwner()),
			DeferredInitAttempts,
			MaxDeferredInitAttempts);
		ScheduleDeferredInitializeParts();
		return;
	}

	if (!IsOwnerMeshReadyForAttach())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Parts: %s mesh/bones still not ready after %d frames — attaching anyway (may snap to mesh root)."),
			*GetNameSafe(GetOwner()),
			DeferredInitAttempts);
		bAttachSettleFramePending = false;
		InitializeParts();
		return;
	}

	// One extra frame after bones report ready — socket table / ref-pose can lag one tick on editor reopen.
	if (!bAttachSettleFramePending)
	{
		bAttachSettleFramePending = true;
		if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
		{
			OwnerMesh->RefreshBoneTransforms();
			OwnerMesh->UpdateComponentToWorld();
		}
		ScheduleDeferredInitializeParts();
		return;
	}

	bAttachSettleFramePending = false;
	InitializeParts();
}

void UPartsComponent::InitializeParts()
{
	// Cold start / post-rebuild: skeletal mesh or bone sockets often not ready in the same BeginPlay frame.
	BuildDefinitionList();
	BindOwnerAnimInitialized();
	ConfigureOwnerMeshForWeaponTrace();
	if (!IsOwnerMeshReadyForAttach())
	{
		bInitialized = false;
		if (DeferredInitAttempts == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Parts: %s InitializeParts deferred — waiting for AnimInstance / bone poses (cold start timing)."),
				*GetNameSafe(GetOwner()));
		}
		ScheduleDeferredInitializeParts();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredInitTimerHandle);
	}
	DeferredInitAttempts = 0;
	bAttachSettleFramePending = false;

	DestroySpawnedMeshes();
	RebuildBoneToPartMap();

	bDeathPartsExploded = false;
	bHeadIntact = true;
	LeftArmState = EPartsArmState::Intact;
	RightArmState = EPartsArmState::Intact;

	if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		OwnerMesh->RefreshBoneTransforms();
	}

	CreateAndAttachMeshes();
	RestoreAllOwnerHitBodies();
	RestoreHiddenOwnerBones();
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		ApplyAttachBodyHides(Def);
	}
	RefreshCombatDebuffsFromParts();
	bInitialized = true;
#if !UE_BUILD_SHIPPING
    if (!bLoggedHitCollision)
    {
        bLoggedHitCollision = true;
        const auto* Mesh = GetOwnerMesh();
        const auto* Character = Cast<ACharacter>(GetOwner());
        const auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
        int32 CollidingVisuals = 0;
        for (const auto& Pair : PartsRuntime)
            if (Pair.Value.MeshComponent && Pair.Value.MeshComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++CollidingVisuals;
        const bool bValid = Mesh && Mesh->GetSkeletalMeshAsset() && Mesh->GetPhysicsAsset()
            && Mesh->IsQueryCollisionEnabled() && Mesh->GetCollisionResponseToChannel(WeaponTraceChannel) == ECR_Block
            && (!Capsule || Capsule->GetCollisionResponseToChannel(WeaponTraceChannel) == ECR_Ignore) && CollidingVisuals == 0;
        UE_LOG(LogTemp, Log, TEXT("Parts HitCollision owner=%s valid=%d mesh=%s asset=%s phat=%s query=%d channel=%d meshResponse=%d capsuleResponse=%d collidingVisuals=%d"),
            *GetNameSafe(GetOwner()), bValid, *GetNameSafe(Mesh), *GetNameSafe(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr),
            *GetNameSafe(Mesh ? Mesh->GetPhysicsAsset() : nullptr), Mesh && Mesh->IsQueryCollisionEnabled(), int32(WeaponTraceChannel),
            Mesh ? int32(Mesh->GetCollisionResponseToChannel(WeaponTraceChannel)) : -1,
            Capsule ? int32(Capsule->GetCollisionResponseToChannel(WeaponTraceChannel)) : -1, CollidingVisuals);
        UE_CLOG(!bValid, LogTemp, Warning, TEXT("Parts HitCollision invalid on %s: inspect skeletal mesh, Physics Asset and component responses."), *GetNameSafe(GetOwner()));
    }
#endif

	// HideBone already ran — drive unscaled bone follow now.
	ApplyBoneFollowTransforms();
	StartBoneFollowTimer();
	SetComponentTickEnabled(true);
	if (USkeletalMeshComponent* MeshForTick = GetOwnerMesh())
	{
		AddTickPrerequisiteComponent(MeshForTick);
		MeshForTick->VisibilityBasedAnimTickOption =
			EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Parts: %s InitializeParts DONE (definitions=%d). Tick-follow + HideBone."),
		*GetNameSafe(GetOwner()),
		ActiveDefinitions.Num());
	SchedulePostAttachReconcile();
}

void UPartsComponent::CreateAndAttachMeshes()
{
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!Owner || !OwnerMesh)
	{
		return;
	}

	const TArray<int32> Order = BuildAttachOrder();

	for (const int32 Index : Order)
	{
		const FPartsDefinition& Def = ActiveDefinitions[Index];
		if (!Def.PartTag.IsValid())
		{
			continue;
		}

		FPartsRuntimeState& State = PartsRuntime.FindOrAdd(Def.PartTag);
		State.CurrentHP = Def.MaxHP;
		State.bDestroyed = false;

		UStaticMesh* MeshAsset = Def.PartMesh.IsNull() ? nullptr : Def.PartMesh.LoadSynchronous();
		if (!MeshAsset)
		{
			// No mesh yet (BP not filled) — still track HP/logic (bone map already built).
			continue;
		}

		const FName CompName = *FString::Printf(TEXT("Part_%s"), *Def.PartTag.ToString().Replace(TEXT("."), TEXT("_")));
		UStaticMeshComponent* PartComp = NewObject<UStaticMeshComponent>(Owner, CompName);
		PartComp->SetStaticMesh(MeshAsset);
		PartComp->SetMobility(EComponentMobility::Movable);

		// Kinematic + no collision BEFORE register — registering an unattached SM with phys can drop it to the floor.
		PartComp->SetSimulatePhysics(false);
		PartComp->SetEnableGravity(false);
		PartComp->BodyInstance.bSimulatePhysics = false;
		PartComp->BodyInstance.bAutoWeld = false;
		PartComp->BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PartComp->SetNotifyRigidBodyCollision(false);
		PartComp->SetGenerateOverlapEvents(false);
		PartComp->SetCanEverAffectNavigation(false);
		PartComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PartComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Register, then ALWAYS bind to FollowBone (empty socket = no anim follow).
		Owner->AddInstanceComponent(PartComp);
		PartComp->RegisterComponent();

		PartComp->SetSimulatePhysics(false);
		PartComp->SetEnableGravity(false);
		PartComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		PartComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (FBodyInstance* BI = PartComp->GetBodyInstance())
		{
			BI->SetInstanceSimulatePhysics(false);
		}

		FName BoundSocket = NAME_None;
		FTransform MeshSpaceBind = FTransform::Identity;
		FTransform RelToBone = FTransform::Identity;
		if (!PartsAttachPrivate::AttachPartToFollowBone(OwnerMesh, PartComp, Def, BoundSocket, MeshSpaceBind, RelToBone))
		{
			UE_LOG(LogTemp, Error,
				TEXT("Parts: %s FAILED bone-link '%s' → '%s'"),
				*GetNameSafe(Owner),
				*Def.PartTag.ToString(),
				*Def.FollowBoneName.ToString());
			State.bFollowBoneAnim = false;
		}
		else
		{
			State.FollowSocket = BoundSocket;
			State.MeshSpaceBind = MeshSpaceBind;
			State.RelToBone = RelToBone;
			State.bFollowBoneAnim = true;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("Parts: %s follow='%s' tick=%d abs=%d parent=%s"),
			*GetNameSafe(Owner),
			*State.FollowSocket.ToString(),
			State.bFollowBoneAnim ? 1 : 0,
			PartComp->IsUsingAbsoluteLocation() ? 1 : 0,
			*GetNameSafe(PartComp->GetAttachParent()));

		State.MeshComponent = PartComp;
	}
}

void UPartsComponent::SchedulePostAttachReconcile()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PostAttachReconcilePass = 0;
	World->GetTimerManager().ClearTimer(PostAttachReconcileTimerHandle);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UPartsComponent::PostAttachReconcileTick));
}

void UPartsComponent::PostAttachReconcileTick()
{
	if (!IsValid(this) || !bInitialized)
	{
		return;
	}

	++PostAttachReconcilePass;

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (OwnerMesh)
	{
		OwnerMesh->RefreshBoneTransforms();
		OwnerMesh->UpdateComponentToWorld();
	}

	int32 FixedCount = 0;
	int32 OkCount = 0;
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (Def.FollowBoneName.IsNone() || Def.PartMesh.IsNull())
		{
			continue;
		}

		FPartsRuntimeState* State = PartsRuntime.Find(Def.PartTag);
		if (!State || !State->MeshComponent || State->bDestroyed)
		{
			continue;
		}

		UStaticMeshComponent* PartComp = State->MeshComponent;
		// Tick-follow: mesh root + Absolute is intentional.
		if (State->bFollowBoneAnim)
		{
			if (PartComp->GetAttachParent() != OwnerMesh)
			{
				if (ReattachPartToFollowBone(Def, PartComp))
				{
					++FixedCount;
				}
			}
			else
			{
				++OkCount;
			}
			continue;
		}

		const FName Expected = PartsAttachPrivate::ResolveAttachBoneName(OwnerMesh, Def.FollowBoneName);
		const bool bSocketWrong = !Expected.IsNone() && PartComp->GetAttachSocketName() != Expected;
		const bool bParentWrong = PartComp->GetAttachParent() != OwnerMesh;

		float DistToBone = 0.0f;
		if (OwnerMesh && !Expected.IsNone() && OwnerMesh->GetBoneIndex(Expected) != INDEX_NONE)
		{
			DistToBone = FVector::Dist(PartComp->GetComponentLocation(), OwnerMesh->GetBoneLocation(Expected));
		}

		if (bSocketWrong || bParentWrong)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Parts: %s reconcile pass %d FIX '%s' (socket=%s want=%s parentOk=%d dist=%.1f boneLocal=%d)"),
				*GetNameSafe(GetOwner()),
				PostAttachReconcilePass,
				*Def.PartTag.ToString(),
				*PartComp->GetAttachSocketName().ToString(),
				*Expected.ToString(),
				bParentWrong ? 0 : 1,
				DistToBone,
				Def.bAttachTransformIsBoneLocal ? 1 : 0);
			if (ReattachPartToFollowBone(Def, PartComp))
			{
				++FixedCount;
			}
		}
		else
		{
			++OkCount;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Parts: %s reconcile pass %d done (ok=%d fixed=%d)."),
		*GetNameSafe(GetOwner()),
		PostAttachReconcilePass,
		OkCount,
		FixedCount);

	if (PostAttachReconcilePass < MaxPostAttachReconcilePasses)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PostAttachReconcileTimerHandle,
				FTimerDelegate::CreateUObject(this, &UPartsComponent::PostAttachReconcileTick),
				0.15f,
				false);
		}
	}
}

bool UPartsComponent::ReattachPartToFollowBone(const FPartsDefinition& Def, UStaticMeshComponent* PartComp)
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || !PartComp || Def.FollowBoneName.IsNone())
	{
		return false;
	}

	FName BoundSocket = NAME_None;
	FTransform MeshSpaceBind = FTransform::Identity;
	FTransform RelToBone = FTransform::Identity;
	if (!PartsAttachPrivate::AttachPartToFollowBone(OwnerMesh, PartComp, Def, BoundSocket, MeshSpaceBind, RelToBone))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Parts: %s reattach '%s' failed for FollowBone '%s'."),
			*GetNameSafe(GetOwner()),
			*Def.PartTag.ToString(),
			*Def.FollowBoneName.ToString());
		return false;
	}

	if (FPartsRuntimeState* State = PartsRuntime.Find(Def.PartTag))
	{
		State->FollowSocket = BoundSocket;
		State->MeshSpaceBind = MeshSpaceBind;
		State->RelToBone = RelToBone;
		State->bFollowBoneAnim = true;
	}

	PartComp->SetSimulatePhysics(false);
	PartComp->SetEnableGravity(false);
	SetComponentTickEnabled(true);
	StartBoneFollowTimer();
	return true;
}

void UPartsComponent::ApplyAttachBodyHides(const FPartsDefinition& Def)
{
	if (Def.PartMesh.IsNull())
	{
		return;
	}

	if (Def.HideBonesOnAttach.Num() > 0)
	{
		HideOwnerBones(Def.HideBonesOnAttach);
		return;
	}

	// Default: hide the follow bone only (not children — keeps fingers visible for Hand).
	if (!Def.FollowBoneName.IsNone() && !Def.ParentTag.IsValid())
	{
		HideOwnerBones({ Def.FollowBoneName });
	}
}

void UPartsComponent::HideOwnerBones(const TArray<FName>& Bones)
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return;
	}

	for (const FName Bone : Bones)
	{
		if (Bone.IsNone())
		{
			continue;
		}
		OwnerMesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
		HiddenOwnerBones.AddUnique(Bone);
	}

	// HideBone → RebuildVisibilityArray marks all children HiddenByParent (kills fingers).
	RestoreHiddenBoneScalesForDescendants();
	OwnerMesh->MarkRenderStateDirty();
}

void UPartsComponent::RestoreHiddenBoneScalesForDescendants()
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || HiddenOwnerBones.Num() == 0)
	{
		return;
	}

	// UE5.6 HideBone only sets BVS_ExplicitlyHidden, then RebuildVisibilityArray propagates
	// BVS_HiddenByParent to every descendant. Flip those back so fingers stay drawable.
	TArray<uint8>& Visibility = OwnerMesh->GetEditableBoneVisibilityStates();
	bool bChanged = false;
	for (int32 BoneIndex = 0; BoneIndex < Visibility.Num(); ++BoneIndex)
	{
		if (Visibility[BoneIndex] == BVS_HiddenByParent)
		{
			Visibility[BoneIndex] = BVS_Visible;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OwnerMesh->MarkRenderStateDirty();
	}
}

void UPartsComponent::GatherChildBoneNames(FName RootBone, TArray<FName>& OutBones) const
{
	OutBones.Reset();
	if (RootBone.IsNone())
	{
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || !OwnerMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	const FReferenceSkeleton& RefSkel = OwnerMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 RootIndex = RefSkel.FindBoneIndex(RootBone);
	if (RootIndex == INDEX_NONE)
	{
		return;
	}

	OutBones.Add(RootBone);
	const int32 NumBones = RefSkel.GetNum();
	for (int32 BoneIndex = RootIndex + 1; BoneIndex < NumBones; ++BoneIndex)
	{
		int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
		bool bUnderRoot = false;
		while (ParentIndex != INDEX_NONE)
		{
			if (ParentIndex == RootIndex)
			{
				bUnderRoot = true;
				break;
			}
			ParentIndex = RefSkel.GetParentIndex(ParentIndex);
		}
		if (bUnderRoot)
		{
			OutBones.Add(RefSkel.GetBoneName(BoneIndex));
		}
	}
}

void UPartsComponent::HideOwnerBoneTree(FName RootBone)
{
	TArray<FName> Bones;
	GatherChildBoneNames(RootBone, Bones);
	HideOwnerBones(Bones);
	// Do not mutate PhAT BodyInstance here — Chaos crash. Destroyed hits use bDestroyed.
}

void UPartsComponent::RestoreHiddenOwnerBones()
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (OwnerMesh)
	{
		for (const FName Bone : HiddenOwnerBones)
		{
			OwnerMesh->UnHideBoneByName(Bone);
		}
	}
	HiddenOwnerBones.Reset();
}

void UPartsComponent::ResetAllParts()
{
	InitializeParts();
}

const FPartsDefinition* UPartsComponent::FindDefinition(FGameplayTag PartTag) const
{
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (Def.PartTag == PartTag)
		{
			return &Def;
		}
	}
	return nullptr;
}

FPartsRuntimeState* UPartsComponent::FindRuntime(FGameplayTag PartTag)
{
	return PartsRuntime.Find(PartTag);
}

const FPartsRuntimeState* UPartsComponent::FindRuntime(FGameplayTag PartTag) const
{
	return PartsRuntime.Find(PartTag);
}

FGameplayTag UPartsComponent::ResolvePartTagFromBone(FName BoneOrSocketName) const
{
	if (BoneOrSocketName.IsNone())
	{
		return FGameplayTag();
	}

	const FName Key(*BoneOrSocketName.ToString().ToLower());
	if (const FGameplayTag* Found = BoneToPartMap.Find(Key))
	{
		return *Found;
	}

	// PhAT often hits twist/roll helper bones. Walk parents until a mapped part bone is found.
	const USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh || !OwnerMesh->GetSkeletalMeshAsset())
	{
		return FGameplayTag();
	}

	const FReferenceSkeleton& RefSkel = OwnerMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	int32 BoneIndex = RefSkel.FindBoneIndex(BoneOrSocketName);
	while (BoneIndex != INDEX_NONE)
	{
		const int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);
		if (ParentIndex == INDEX_NONE)
		{
			break;
		}

		const FName ParentName = RefSkel.GetBoneName(ParentIndex);
		const FName ParentKey(*ParentName.ToString().ToLower());
		if (const FGameplayTag* FoundParent = BoneToPartMap.Find(ParentKey))
		{
			return *FoundParent;
		}
		BoneIndex = ParentIndex;
	}

	return FGameplayTag();
}

FGameplayTag UPartsComponent::ResolvePartTagForHit(FName HitBoneName, const FVector& ImpactPoint) const
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return ResolvePartTagFromBone(HitBoneName);
	}

	const FGameplayTag MappedFromBone = ResolvePartTagFromBone(HitBoneName);
    // An unmapped torso must not select a descendant arm/head by proximity.
    if (!MappedFromBone.IsValid()) return FGameplayTag();
	FName ChainRootBone = HitBoneName;

	// If PhAT named a parent bone (e.g. upperarm_l for a forearm impact), still score all limb parts under that chain.
	if (const FPartsDefinition* MappedDef = FindDefinition(MappedFromBone))
	{
		if (!MappedDef->FollowBoneName.IsNone())
		{
			ChainRootBone = MappedDef->FollowBoneName;
		}
	}

	FGameplayTag BestTag;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (!Def.PartTag.IsValid() || Def.FollowBoneName.IsNone())
		{
			continue;
		}

		const FPartsRuntimeState* State = FindRuntime(Def.PartTag);
		if (!State)
		{
			continue;
		}

		if (OwnerMesh->GetBoneIndex(Def.FollowBoneName) == INDEX_NONE)
		{
			continue;
		}

		bool bRelated = HitBoneName.IsNone();
		if (!bRelated)
		{
			bRelated =
				Def.FollowBoneName == HitBoneName
				|| Def.FollowBoneName == ChainRootBone
				|| OwnerMesh->BoneIsChildOf(Def.FollowBoneName, ChainRootBone)
				|| OwnerMesh->BoneIsChildOf(HitBoneName, Def.FollowBoneName)
				|| OwnerMesh->BoneIsChildOf(ChainRootBone, Def.FollowBoneName);
		}

		if (!bRelated)
		{
			continue;
		}

		const FVector BoneLoc = OwnerMesh->GetBoneLocation(Def.FollowBoneName);
		const float DistSq = FVector::DistSquared(BoneLoc, ImpactPoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTag = Def.PartTag;
		}
	}

	if (BestTag.IsValid())
	{
		return BestTag;
	}

	return MappedFromBone;
}

FGameplayTag UPartsComponent::ResolvePartTagFromComponent(const UPrimitiveComponent* Component) const
{
	if (!Component)
	{
		return FGameplayTag();
	}

	for (const TPair<FGameplayTag, FPartsRuntimeState>& Pair : PartsRuntime)
	{
		if (Pair.Value.MeshComponent == Component)
		{
			return Pair.Key;
		}
	}
	return FGameplayTag();
}

bool UPartsComponent::IsPartDestroyed(FGameplayTag PartTag) const
{
	if (const FPartsRuntimeState* State = FindRuntime(PartTag))
	{
		return State->bDestroyed;
	}
	return false;
}

float UPartsComponent::GetPartHP(FGameplayTag PartTag) const
{
	if (const FPartsRuntimeState* State = FindRuntime(PartTag))
	{
		return State->CurrentHP;
	}
	return 0.0f;
}

bool UPartsComponent::CanShoot() const
{
	if (bRequireRightArmToShoot)
	{
		return RightArmState != EPartsArmState::Disabled;
	}

	return LeftArmState != EPartsArmState::Disabled
		|| RightArmState != EPartsArmState::Disabled;
}

bool UPartsComponent::ApplyPartDamageAtBone(FName BoneOrSocketName, float Damage)
{
	const FGameplayTag PartTag = ResolvePartTagFromBone(BoneOrSocketName);
	if (!PartTag.IsValid())
	{
		return false;
	}
	return ApplyPartDamage(PartTag, Damage);
}

bool UPartsComponent::ApplyPartDamage(FGameplayTag PartTag, float Damage)
{
	if (!PartTag.IsValid() || Damage <= 0.0f || bDeathPartsExploded)
	{
		return false;
	}

	FPartsRuntimeState* State = FindRuntime(PartTag);
	if (!State || State->bDestroyed)
	{
		return false;
	}

	State->CurrentHP = FMath::Max(0.0f, State->CurrentHP - Damage);
	OnPartDamaged.Broadcast(PartTag, State->CurrentHP);

	UE_LOG(LogTemp, Log, TEXT("PartsDamage: %s part %s HP -> %.1f (dmg %.1f)"),
		*GetNameSafe(GetOwner()), *PartTag.ToString(), State->CurrentHP, Damage);

	if (State->CurrentHP <= 0.0f)
	{
		DestroyPart(PartTag);
	}
	return true;
}

void UPartsComponent::DestroyPart(FGameplayTag PartTag)
{
	DestroyPartInternal(PartTag, false);
}

void UPartsComponent::DestroyChildParts(FGameplayTag DestroyedPartTag, bool bFromDeathExplosion)
{
	const FPartsDefinition* DestroyedDef = FindDefinition(DestroyedPartTag);
	if (!DestroyedDef)
	{
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	const FName DestroyedBone = DestroyedDef->FollowBoneName;
	const bool bUseBoneHierarchy =
		OwnerMesh != nullptr
		&& !DestroyedBone.IsNone()
		&& OwnerMesh->GetBoneIndex(DestroyedBone) != INDEX_NONE;

	// Snapshot tags — DestroyPartInternal mutates runtime while we cascade.
	TArray<FGameplayTag> ToDestroy;
	ToDestroy.Reserve(8);

	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (!Def.PartTag.IsValid() || Def.PartTag == DestroyedPartTag)
		{
			continue;
		}

		const FPartsRuntimeState* ChildState = FindRuntime(Def.PartTag);
		if (!ChildState || ChildState->bDestroyed)
		{
			continue;
		}

		bool bDependent = false;

		if (bUseBoneHierarchy && !Def.FollowBoneName.IsNone()
			&& OwnerMesh->GetBoneIndex(Def.FollowBoneName) != INDEX_NONE)
		{
			// Skeleton wins: only destroy parts on descendant bones.
			// Fixes inverted DA ParentTag (e.g. Upper.Parent=Lower) that wiped the whole arm on forearm hits.
			bDependent = OwnerMesh->BoneIsChildOf(Def.FollowBoneName, DestroyedBone);

			if (!bDependent && Def.ParentTag == DestroyedPartTag)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Parts cascade: ignoring ParentTag child '%s' of '%s' — bone '%s' is not under '%s'."),
					*Def.PartTag.ToString(),
					*DestroyedPartTag.ToString(),
					*Def.FollowBoneName.ToString(),
					*DestroyedBone.ToString());
			}
		}
		else if (Def.ParentTag == DestroyedPartTag)
		{
			bDependent = true;
		}

		if (bDependent)
		{
			ToDestroy.Add(Def.PartTag);
		}
	}

	for (const FGameplayTag& ChildTag : ToDestroy)
	{
		DestroyPartInternal(ChildTag, bFromDeathExplosion);
	}
}

void UPartsComponent::DestroyPartInternal(FGameplayTag PartTag, bool bFromDeathExplosion)
{
	if (!PartTag.IsValid())
	{
		return;
	}

	FPartsRuntimeState* State = FindRuntime(PartTag);
	if (!State || State->bDestroyed)
	{
		return;
	}

	const FPartsDefinition* Def = FindDefinition(PartTag);
	if (!Def)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
    if (CVarDMPartsDiag.GetValueOnGameThread() != 0 && PartTag.GetTagName() == TEXT("Parts.Head"))
        UE_LOG(LogTemp, Display, TEXT("DMPartsDiag Destroy Owner=%s Component=%s Setup=%s Index=%d Separable=%d Cause=%s Mesh=%s"),
            *GetNameSafe(GetOwner()), *GetName(), *GetPathNameSafe(SetupData), int32(Def-ActiveDefinitions.GetData()),
            Def->bSeparable, bFromDeathExplosion ? TEXT("DeathExplosion") : TEXT("PartDamage"), *GetNameSafe(State->MeshComponent));
#endif
	DestroyChildParts(PartTag, bFromDeathExplosion);

	const EPartsDestroyedVisualPolicy VisualPolicy = Def->GetDestroyedVisualPolicy();
	const bool bKeepAttached = VisualPolicy == EPartsDestroyedVisualPolicy::KeepAttachedDisabled;
	State->bDestroyed = true;
	State->bFollowBoneAnim &= bKeepAttached;
	State->CurrentHP = 0.0f;

	const FPartsDefinition DefCopy = *Def;

	// Never touch PhAT FBodyInstance (SetCollision/Response) — Chaos ACCESS_VIOLATION 0xFF..FF.
	// Part HP ignores repeat damage; retained owner PhAT hits still route body damage.
	if (!bKeepAttached)
	{
		if (Def->bHideDescendantBonesOnDestroy)
		{
			const FName TreeRoot = Def->HideTreeRootBone.IsNone() ? Def->FollowBoneName : Def->HideTreeRootBone;
			HideOwnerBoneTree(TreeRoot);
		}
		else if (!Def->FollowBoneName.IsNone())
		{
			HideOwnerBones({ Def->FollowBoneName });
		}
		if (Def->HideBonesOnAttach.Num() > 0)
		{
			HideOwnerBones(Def->HideBonesOnAttach);
		}
	}

	if (VisualPolicy == EPartsDestroyedVisualPolicy::DetachAsDebris)
	{
		if (!State->MeshComponent)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("PartsDamage: destroyed %s but PartMesh was never spawned — assign PartMesh on DA row."),
				*PartTag.ToString());
		}
		DetachPartAsDebris(PartTag, DefCopy, *State, bFromDeathExplosion);
	}
	else if (State->MeshComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("PartsDamage: %s destroyed visual policy=%s (no physics)"),
			*PartTag.ToString(), bKeepAttached ? TEXT("KeepAttachedDisabled") : TEXT("HideInPlace"));
		State->MeshComponent->SetVisibility(bKeepAttached);
		State->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	RefreshCombatDebuffsFromParts();
	OnPartDestroyed.Broadcast(PartTag);

	if (!bFromDeathExplosion)
	{
		MaybeKillOwnerIfCannotShoot();
	}
}

void UPartsComponent::SetOwnerBoneHitEnabled(FName BoneName, bool bEnabled)
{
	// Intentionally no FBodyInstance mutation.
	// SetCollisionEnabled / SetResponseToAllChannels on PhAT bodies crashes Chaos
	// (EXCEPTION_ACCESS_VIOLATION 0xFF..FF) on this project’s meshes.
	// Destroyed parts retain PhAT: the hit router applies body-only damage on owner-mesh re-hits.
	(void)BoneName;
	(void)bEnabled;
}

void UPartsComponent::DisableOwnerHitBodiesForPart(const FPartsDefinition& Def)
{
	(void)Def;
}

void UPartsComponent::RestoreOwnerHitBodiesForPart(const FPartsDefinition& Def)
{
	SetOwnerBoneHitEnabled(Def.FollowBoneName, true);
	for (const FName& Alias : Def.HitBoneAliases)
	{
		SetOwnerBoneHitEnabled(Alias, true);
	}
}

void UPartsComponent::RestoreAllOwnerHitBodies()
{
	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		RestoreOwnerHitBodiesForPart(Def);
	}
}

void UPartsComponent::PlaySeparateJointFX(const FPartsDefinition& Def, const UStaticMeshComponent* PartComp) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector JointLoc = GetJointEffectLocation(Def, PartComp);
	const FRotator JointRot = PartComp ? PartComp->GetComponentRotation() : FRotator::ZeroRotator;

	UParticleSystem* Effect = nullptr;
	if (!Def.SeparateEffect.IsNull())
	{
		Effect = Def.SeparateEffect.LoadSynchronous();
	}
	if (!Effect)
	{
		Effect = DefaultSeparateEffect;
	}

	const float Scale = Def.SeparateEffectScale > 0.0f ? Def.SeparateEffectScale : DefaultSeparateEffectScale;

	if (Effect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			Effect,
			JointLoc,
			JointRot,
			FVector(Scale),
			true
		);
	}

	USoundBase* Sound = nullptr;
	if (!Def.SeparateSound.IsNull())
	{
		Sound = Def.SeparateSound.LoadSynchronous();
	}
	if (!Sound)
	{
		Sound = DefaultSeparateSound;
	}

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, JointLoc);
	}
}

FVector UPartsComponent::GetJointEffectLocation(const FPartsDefinition& Def, const UStaticMeshComponent* PartComp) const
{
	const FName JointSocket = Def.JointEffectSocketName.IsNone()
		? Def.FollowBoneName
		: Def.JointEffectSocketName;

	if (USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		if (!JointSocket.IsNone() && OwnerMesh->DoesSocketExist(JointSocket))
		{
			return OwnerMesh->GetSocketLocation(JointSocket);
		}
		if (!JointSocket.IsNone())
		{
			// Bone without explicit socket — still valid for skeletal meshes.
			const int32 BoneIndex = OwnerMesh->GetBoneIndex(JointSocket);
			if (BoneIndex != INDEX_NONE)
			{
				return OwnerMesh->GetBoneLocation(JointSocket);
			}
		}
	}

	return PartComp ? PartComp->GetComponentLocation() : (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
}

float UPartsComponent::ResolveDebrisLifeSpan(const FPartsDefinition& Def) const
{
	// <0 or 0 → component default. Tiny DA values used to wipe debris instantly.
	float Life = DebrisLifeSpan;
	if (Def.DebrisLifeSpan > KINDA_SMALL_NUMBER)
	{
		Life = Def.DebrisLifeSpan;
	}
	return FMath::Max(Life, 0.5f);
}

void UPartsComponent::ScheduleDebrisDestroy(UStaticMeshComponent* Comp, float LifeSpan)
{
	// Legacy path kept for callers; prefer world debris actors (SetLifeSpan).
	if (!Comp)
	{
		return;
	}

	const float Clamped = FMath::Max(LifeSpan, 0.5f);
	UWorld* World = Comp->GetWorld();
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<UStaticMeshComponent> WeakComp(Comp);
	FTimerHandle DebrisTimer;
	World->GetTimerManager().SetTimer(
		DebrisTimer,
		FTimerDelegate::CreateLambda([WeakComp]()
		{
			if (UStaticMeshComponent* Alive = WeakComp.Get())
			{
				Alive->DestroyComponent();
			}
		}),
		Clamped,
		false
	);
}

void UPartsComponent::DetachPartAsDebris(
	FGameplayTag PartTag,
	const FPartsDefinition& Def,
	FPartsRuntimeState& State,
	bool bExplosiveImpulse)
{
	UStaticMeshComponent* Comp = State.MeshComponent;
	if (!Comp)
	{
		PlaySeparateJointFX(Def, nullptr);
		UE_LOG(LogTemp, Warning,
			TEXT("Parts debris '%s': no MeshComponent — part will only hide (assign PartMesh on DA)."),
			*PartTag.ToString());
		return;
	}

	PlaySeparateJointFX(Def, Comp);

	const float LifeSpan = ResolveDebrisLifeSpan(Def);
	const float ImpulseStrength = bExplosiveImpulse ? DeathExplosionImpulse : SeparateImpulseStrength;
	AActor* OwnerActor = GetOwner();
	const FVector Origin = OwnerActor ? OwnerActor->GetActorLocation() : Comp->GetComponentLocation();

	State.MeshComponent = nullptr;
	State.bFollowBoneAnim = false;

	if (PartsDebrisPrivate::SpawnDebrisCopyFromPartMesh(Comp, OwnerActor, LifeSpan, ImpulseStrength, Origin))
	{
		return;
	}

	UE_LOG(LogTemp, Error,
		TEXT("Parts debris '%s': detach-to-world failed — hiding mesh as last resort."),
		*PartTag.ToString());
	Comp->SetVisibility(false);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->DestroyComponent();
}

void UPartsComponent::ApplyArmEffect(EPartsArmState& ArmState, EPartsDestroyArmEffect Effect, bool bLeftSide)
{
	EPartsArmState Incoming = EPartsArmState::Intact;
	switch (Effect)
	{
	case EPartsDestroyArmEffect::WeakenLeft:
	case EPartsDestroyArmEffect::WeakenRight:
		Incoming = EPartsArmState::Weakened;
		break;
	case EPartsDestroyArmEffect::DisableLeft:
	case EPartsDestroyArmEffect::DisableRight:
		Incoming = EPartsArmState::Disabled;
		break;
	default:
		return;
	}

	const bool bMatchesSide =
		(bLeftSide && (Effect == EPartsDestroyArmEffect::WeakenLeft || Effect == EPartsDestroyArmEffect::DisableLeft))
		|| (!bLeftSide && (Effect == EPartsDestroyArmEffect::WeakenRight || Effect == EPartsDestroyArmEffect::DisableRight));

	if (!bMatchesSide)
	{
		return;
	}

	if (static_cast<uint8>(Incoming) > static_cast<uint8>(ArmState))
	{
		ArmState = Incoming;
	}
}

void UPartsComponent::RefreshCombatDebuffsFromParts()
{
	LeftArmState = EPartsArmState::Intact;
	RightArmState = EPartsArmState::Intact;
	bHeadIntact = true;

	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		const FPartsRuntimeState* State = FindRuntime(Def.PartTag);
		if (!State || !State->bDestroyed)
		{
			continue;
		}

		if (Def.bImpairHeadOnDestroy)
		{
			bHeadIntact = false;
		}

		ApplyArmEffect(LeftArmState, Def.ArmEffectOnDestroy, true);
		ApplyArmEffect(RightArmState, Def.ArmEffectOnDestroy, false);
	}
}

void UPartsComponent::MaybeKillOwnerIfCannotShoot()
{
	if (!bKillOwnerWhenCannotShoot || bDeathPartsExploded || CanShoot())
	{
		return;
	}

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character || Character->IsDead)
	{
		return;
	}

	const float Lethal = FMath::Max(Character->MaxHP, Character->CurHP) + 1.0f;
	UE_LOG(LogTemp, Log,
		TEXT("PartsCombat: %s cannot shoot (right arm required=%d) — forcing body death."),
		*GetNameSafe(Character),
		bRequireRightArmToShoot ? 1 : 0);

	UGameplayStatics::ApplyDamage(
		Character,
		Lethal,
		Character->GetController(),
		Character,
		UDamageType::StaticClass());
}

void UPartsComponent::HandleOwnerDeathPartsExplosion()
{
	if (bDeathPartsExploded)
	{
		return;
	}

	bDeathPartsExploded = true;
#if !UE_BUILD_SHIPPING
    if (CVarDMPartsDiag.GetValueOnGameThread() != 0)
        for (const auto& D : ActiveDefinitions)
            if (D.PartTag.GetTagName() == TEXT("Parts.Head"))
                UE_LOG(LogTemp, Display, TEXT("DMPartsDiag Death Owner=%s Setup=%s HeadSeparable=%d SkipDirectExplosion=%d"),
                    *GetNameSafe(GetOwner()), *GetPathNameSafe(SetupData), D.bSeparable, !D.bSeparable);
#endif


	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (!Def.bSeparable || !Def.PartTag.IsValid())
		{
			continue;
		}

		if (!Def.ParentTag.IsValid())
		{
			DestroyPartInternal(Def.PartTag, true);
		}
	}

	for (const FPartsDefinition& Def : ActiveDefinitions)
	{
		if (!Def.bSeparable)
		{
			continue;
		}

		FPartsRuntimeState* State = FindRuntime(Def.PartTag);
		if (State && !State->bDestroyed)
		{
			DestroyPartInternal(Def.PartTag, true);
		}
	}

	AActor* Owner = GetOwner();
	const FVector Origin = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	OnDeathPartsExplosion.Broadcast(Origin, DeathExplosionImpulse);
}

void UPartsComponent::ValidateWeaponTraceChannel(const UObject* FireAbility, ECollisionChannel FireChannel) const
{
#if !UE_BUILD_SHIPPING
    if (FireChannel == WeaponTraceChannel) return;
    const uint32 Pair = (uint32(FireChannel) << 8) | uint32(WeaponTraceChannel.GetValue());
    if (WarnedTraceChannelPairs.Contains(Pair)) return;
    WarnedTraceChannelPairs.Add(Pair);
    UE_LOG(LogTemp, Warning, TEXT("Parts HitTrace mismatch Owner=%s FireAbility=%s FireTraceChannel=%d PartsWeaponTraceChannel=%d. Fix BP channel settings; values were not overwritten."),
        *GetNameSafe(GetOwner()), *GetPathNameSafe(FireAbility), int32(FireChannel), int32(WeaponTraceChannel));
#endif
}
