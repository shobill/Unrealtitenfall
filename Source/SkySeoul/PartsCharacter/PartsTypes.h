#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/StaticMesh.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "PartsTypes.generated.h"

/** Destruction presentation, independent of HP and functional damage. */
UENUM(BlueprintType)
enum class EPartsDestroyedVisualPolicy : uint8
{
	DetachAsDebris,
	HideInPlace,
	KeepAttachedDisabled
};

/** Per-arm combat capability after part breaks. */
UENUM(BlueprintType)
enum class EPartsArmState : uint8
{
	Intact   UMETA(DisplayName = "Intact"),
	Weakened UMETA(DisplayName = "Weakened"),
	Disabled UMETA(DisplayName = "Disabled"),
};

/** Applied when this part is destroyed (data-driven debuff). */
UENUM(BlueprintType)
enum class EPartsDestroyArmEffect : uint8
{
	None         UMETA(DisplayName = "None"),
	WeakenLeft   UMETA(DisplayName = "Weaken Left Arm"),
	DisableLeft  UMETA(DisplayName = "Disable Left Arm"),
	WeakenRight  UMETA(DisplayName = "Weaken Right Arm"),
	DisableRight UMETA(DisplayName = "Disable Right Arm"),
};

/**
 * One row in a Parts Setup DataAsset.
 * Attach: always to owner skeletal mesh on FollowBoneName (mesh-space AttachRelativeTransform).
 * ParentTag: cascade destroy only — NOT used for attach parenting.
 */
USTRUCT(BlueprintType)
struct FPartsDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FGameplayTag PartTag;

	/**
	 * Parent part for cascade destroy only (child parts still attach to owner FollowBoneName).
	 * Empty = no cascade parent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FGameplayTag ParentTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts", meta = (ClampMin = "1.0"))
	float MaxHP = 50.0f;

	/**
	 * Root only: bone on owner skeletal mesh to follow (animation).
	 * Bone name only — no Skeleton Socket asset required.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FName FollowBoneName;

	/**
	 * Mesh-space pose used in step 1 (SK→Create Static Mesh under Mesh = Identity).
	 * After KeepWorld onto FollowBone, runtime Rel becomes bone-local automatically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FTransform AttachRelativeTransform = FTransform::Identity;

	/**
	 * false (default): AttachRelativeTransform is character-Mesh relative → place then KeepWorld to bone.
	 * true: already bone-local → attach to bone then SetRelative (manual DA offsets).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	bool bAttachTransformIsBoneLocal = false;

	/**
	 * PhAT bone names that map to this part (hits).
	 * Hand: can list hand + finger bones for hit grouping — optional if you only care about hand body.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TArray<FName> HitBoneAliases;

	/**
	 * When this part has a PartMesh, hide these bones on the owner skeletal mesh (anti-overlap).
	 * Example Upper: upperarm_l. Do NOT list finger bones here — keep them visible on the body.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|BodyMesh")
	TArray<FName> HideBonesOnAttach;

	/**
	 * On destroy: hide FollowBoneName (or HideTreeRootBone) and ALL child bones on the body SK.
	 * Use on Hand so fingers (children of hand_l) vanish when the hand part breaks — no per-finger DA.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|BodyMesh")
	bool bHideDescendantBonesOnDestroy = false;

	/** Root of the hide-tree. None = FollowBoneName. Hand: hand_l */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|BodyMesh", meta = (EditCondition = "bHideDescendantBonesOnDestroy"))
	FName HideTreeRootBone;

	/** BP assigns the part mesh asset here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UStaticMesh> PartMesh;

	/** Optional separate debris mesh; if null, PartMesh is used when detaching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UStaticMesh> DebrisMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	EPartsDestroyArmEffect ArmEffectOnDestroy = EPartsDestroyArmEffect::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	bool bImpairHeadOnDestroy = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	bool bSeparable = true;

	/** Detach requires bSeparable; otherwise it safely resolves to KeepAttachedDisabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Destroyed")
	EPartsDestroyedVisualPolicy DestroyedVisualPolicy = EPartsDestroyedVisualPolicy::DetachAsDebris;

	EPartsDestroyedVisualPolicy GetDestroyedVisualPolicy() const
	{
		return !bSeparable && DestroyedVisualPolicy == EPartsDestroyedVisualPolicy::DetachAsDebris
			? EPartsDestroyedVisualPolicy::KeepAttachedDisabled : DestroyedVisualPolicy;
	}

	/**
	 * Bone on owner skeletal where break FX spawns (joint).
	 * None = FollowBoneName, then part component location.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Separate")
	FName JointEffectSocketName;

	/** Cascade break FX at joint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Separate")
	TSoftObjectPtr<UParticleSystem> SeparateEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Separate")
	TSoftObjectPtr<USoundBase> SeparateSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Separate", meta = (ClampMin = "0.01"))
	float SeparateEffectScale = 1.0f;

	/**
	 * Seconds until detached debris mesh is destroyed.
	 * < 0 = use PartsComponent DebrisLifeSpan.
	 * 0 = destroy immediately.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Separate")
	float DebrisLifeSpan = -1.0f;
	// NOTE: 0 is treated as "use PartsComponent default" (instant destroy looked like vanishing).
};

USTRUCT(BlueprintType)
struct FPartsRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	float CurrentHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	bool bDestroyed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	/** Bone we follow for animation (HideBone zeroes socket scale — driven in Tick). */
	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	FName FollowSocket = NAME_None;

	/**
	 * Part pose in owner-mesh space at SK bind/ref (usually Identity for SK→SM).
	 * Each tick: PartCS = MeshSpaceBind * Inv(RefBoneCS) * AnimBoneCS.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	FTransform MeshSpaceBind = FTransform::Identity;

	/** Cached RelToBone = MeshSpaceBind * Inv(RefBoneCS); PartWorld = RelToBone * UnscaledBoneWorld */
	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	FTransform RelToBone = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	bool bFollowBoneAnim = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartDamaged, FGameplayTag, PartTag, float, RemainingHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartDestroyed, FGameplayTag, PartTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathPartsExplosion, FVector, Origin, float, Impulse);
