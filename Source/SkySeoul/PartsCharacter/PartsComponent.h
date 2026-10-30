#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PartsCharacter/PartsTypes.h"
#include "PartsCharacter/PartsHitCollision.h"
#include "PartsComponent.generated.h"

class UPartsSetupData;
class UPartsComponent;
class UPrimitiveComponent;
class UParticleSystem;
class USoundBase;
class UStaticMeshComponent;

/**
 * Data-driven parts engine.
 * Reads SetupData (or built-in DM defaults), spawns mesh comps, parent hierarchy attach,
 * damage / cascade destroy / death explosion / reset. BP only fills meshes on the DataAsset.
 */
UCLASS(ClassGroup = (Parts), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartsComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Parts")
	void InitializeParts();

	UFUNCTION(BlueprintCallable, Category = "Parts")
	bool ApplyPartDamage(FGameplayTag PartTag, float Damage);

	UFUNCTION(BlueprintCallable, Category = "Parts")
	bool ApplyPartDamageAtBone(FName BoneOrSocketName, float Damage);

	UFUNCTION(BlueprintCallable, Category = "Parts")
	void DestroyPart(FGameplayTag PartTag);

	UFUNCTION(BlueprintCallable, Category = "Parts")
	void HandleOwnerDeathPartsExplosion();

	UFUNCTION(BlueprintCallable, Category = "Parts")
	void ResetAllParts();

	UFUNCTION(BlueprintPure, Category = "Parts")
	FGameplayTag ResolvePartTagFromBone(FName BoneOrSocketName) const;

	/**
	 * Prefer the part whose FollowBone is closest to ImpactPoint among bones related to HitBone.
	 * Fixes coarse PhAT capsules (forearm hit reporting upperarm_l → whole arm cascade).
	 */
	UFUNCTION(BlueprintPure, Category = "Parts")
	FGameplayTag ResolvePartTagForHit(FName HitBoneName, const FVector& ImpactPoint) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	FGameplayTag ResolvePartTagFromComponent(const UPrimitiveComponent* Component) const;

	/** Enumerates visual components for tooling; production weapon traces need no ignore list. */
	UFUNCTION(BlueprintCallable, Category = "Parts")
	void CollectAttachedPartComponents(TArray<UPrimitiveComponent*>& OutComponents) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	const TMap<FName, FGameplayTag>& GetBoneToPartMap() const { return BoneToPartMap; }

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool IsPartDestroyed(FGameplayTag PartTag) const;

    bool HasPart(FGameplayTag PartTag) const { return FindRuntime(PartTag) != nullptr; }
    void ValidateWeaponTraceChannel(const UObject* FireAbility, ECollisionChannel FireChannel) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	float GetPartHP(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	EPartsArmState GetLeftArmState() const { return LeftArmState; }

	UFUNCTION(BlueprintPure, Category = "Parts")
	EPartsArmState GetRightArmState() const { return RightArmState; }

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool IsHeadIntact() const { return bHeadIntact; }

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool CanShoot() const;

	/**
	 * DM weapons mount on the right arm by default.
	 * true: RightArm Disabled → cannot shoot (left-only intact is not enough).
	 * false: either arm Intact/Weakened allows shooting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Combat")
	bool bRequireRightArmToShoot = true;

	/**
	 * When CanShoot becomes false from part loss, force body death so the pawn
	 * does not stand forever as a valid target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Combat")
	bool bKillOwnerWhenCannotShoot = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TObjectPtr<UPartsSetupData> SetupData;

	/** Used when SetupData is null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	bool bUseDefaultDeathMatchPartsIfNoSetupData = true;

	/** Serialized compatibility only; PartsComponent owns the PhAT-only policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Parts|Hit", meta = (DeprecatedProperty, DeprecationMessage = "Ignored: attached parts are visual-only NoCollision; owner PhAT blocks WeaponTraceChannel.", ToolTip = "Deprecated. Stored value has no effect. Attached parts are NoCollision; owner PhAT owns weapon hits."))
	bool bAttachedPartsBlockTraces = false;

	/** Serialized compatibility only; owner mesh configuration is now unconditional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Parts|Hit", meta = (DeprecatedProperty, DeprecationMessage = "Ignored: PartsComponent always configures owner PhAT and capsule.", ToolTip = "Deprecated. Stored value has no effect. Owner PhAT blocks WeaponTraceChannel; capsule ignores it."))
	bool bConfigureOwnerMeshForWeaponTrace = true;

	/**
	 * Must match Fire TraceChannel.
	 * Project mesh profile NormalCollision blocks HitTrace and ignores Pawn — use HitTrace (ECC_GameTraceChannel1).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Hit")
	TEnumAsByte<ECollisionChannel> WeaponTraceChannel = PartsHitCollision::DefaultWeaponTraceChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death")
	float DeathExplosionImpulse = 800.0f;

	/** Impulse when a single part breaks off (non-death). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate", meta = (ClampMin = "0.0"))
	float SeparateImpulseStrength = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death", meta = (ClampMin = "0.0"))
	float DebrisLifeSpan = 8.0f;

	/** Fallback joint FX when DA row has no SeparateEffect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX")
	TObjectPtr<UParticleSystem> DefaultSeparateEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX")
	TObjectPtr<USoundBase> DefaultSeparateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX", meta = (ClampMin = "0.01"))
	float DefaultSeparateEffectScale = 1.0f;

	UPROPERTY(BlueprintAssignable, Category = "Parts")
	FOnPartDamaged OnPartDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Parts")
	FOnPartDestroyed OnPartDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "Parts")
	FOnDeathPartsExplosion OnDeathPartsExplosion;

protected:
	void BuildDefinitionList();
	void DestroySpawnedMeshes();
	void CreateAndAttachMeshes();
	void RebuildBoneToPartMap();
	void ConfigureOwnerMeshForWeaponTrace();
	void RegisterBoneAlias(FName BoneOrSocketName, FGameplayTag PartTag);
	TArray<int32> BuildAttachOrder() const;

	/** Skeletal mesh asset + bones available (cold PIE / post-rebuild often lags one or more frames). */
	bool IsOwnerMeshReadyForAttach() const;
	void ScheduleDeferredInitializeParts();
	void DeferredInitializePartsTick();
	void BindOwnerAnimInitialized();
	UFUNCTION()
	void HandleOwnerAnimInitialized();
	bool AreAttachedPartsOnExpectedBones() const;
	/** Cold start: first attach can report OK then sit on mesh root — re-apply bone attach after settle. */
	void SchedulePostAttachReconcile();
	void PostAttachReconcileTick();
	bool ReattachPartToFollowBone(const FPartsDefinition& Def, UStaticMeshComponent* PartComp);

	void ApplyBoneFollowTransforms();
	void StartBoneFollowTimer();
	void StopBoneFollowTimer();

	const FPartsDefinition* FindDefinition(FGameplayTag PartTag) const;
	FPartsRuntimeState* FindRuntime(FGameplayTag PartTag);
	const FPartsRuntimeState* FindRuntime(FGameplayTag PartTag) const;

	void DestroyPartInternal(FGameplayTag PartTag, bool bFromDeathExplosion);
	void DestroyChildParts(FGameplayTag ParentTag, bool bFromDeathExplosion);
	void DetachPartAsDebris(FGameplayTag PartTag, const FPartsDefinition& Def, FPartsRuntimeState& State, bool bExplosiveImpulse);
	void PlaySeparateJointFX(const FPartsDefinition& Def, const UStaticMeshComponent* PartComp) const;
	FVector GetJointEffectLocation(const FPartsDefinition& Def, const UStaticMeshComponent* PartComp) const;
	float ResolveDebrisLifeSpan(const FPartsDefinition& Def) const;
	void ScheduleDebrisDestroy(UStaticMeshComponent* Comp, float LifeSpan);
	void DisableOwnerHitBodiesForPart(const FPartsDefinition& Def);
	void RestoreOwnerHitBodiesForPart(const FPartsDefinition& Def);
	void RestoreAllOwnerHitBodies();
	void SetOwnerBoneHitEnabled(FName BoneName, bool bEnabled);
	void HideOwnerBones(const TArray<FName>& Bones);
	void HideOwnerBoneTree(FName RootBone);
	void GatherChildBoneNames(FName RootBone, TArray<FName>& OutBones) const;
	void RestoreHiddenOwnerBones();
	/** HideBone marks descendants HiddenByParent — clear that so fingers stay visible. */
	void RestoreHiddenBoneScalesForDescendants();
	void ApplyAttachBodyHides(const FPartsDefinition& Def);
	void RefreshCombatDebuffsFromParts();
	void MaybeKillOwnerIfCannotShoot();
	static void ApplyArmEffect(EPartsArmState& ArmState, EPartsDestroyArmEffect Effect, bool bLeftSide);

	/**
	 * Legacy flag — PhAT BodyInstance mutation is permanently disabled (Chaos crash).
	 * Kept so BP serialization does not break; has no effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Hit", meta = (DeprecatedProperty))
	bool bDisablePhatBodyOnPartDestroy = false;

	UPROPERTY()
	TArray<FName> HiddenOwnerBones;

	USkeletalMeshComponent* GetOwnerMesh() const;

	UPROPERTY()
	TArray<FPartsDefinition> ActiveDefinitions;

	UPROPERTY()
	TMap<FGameplayTag, FPartsRuntimeState> PartsRuntime;

	UPROPERTY()
	TMap<FName, FGameplayTag> BoneToPartMap;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Debuff")
	EPartsArmState LeftArmState = EPartsArmState::Intact;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Debuff")
	EPartsArmState RightArmState = EPartsArmState::Intact;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Debuff")
	bool bHeadIntact = true;

	UPROPERTY(BlueprintReadOnly, Category = "Parts")
	bool bDeathPartsExploded = false;

	bool bInitialized = false;
    bool bLoggedHitCollision = false;
    mutable TSet<uint32> WarnedTraceChannelPairs;

	FTimerHandle DeferredInitTimerHandle;
	FTimerHandle PostAttachReconcileTimerHandle;
	FTimerHandle BoneFollowTimerHandle;
	int32 DeferredInitAttempts = 0;
	int32 PostAttachReconcilePass = 0;
	bool bAttachSettleFramePending = false;
	bool bBoundOwnerAnimInitialized = false;
	static constexpr int32 MaxDeferredInitAttempts = 60;
	static constexpr int32 MaxPostAttachReconcilePasses = 3;
};
