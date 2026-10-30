#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "GameplayTagContainer.h"
#include "PartsCharacter/PartsTypes.h"
#include "PartsCharacter/PartsDamageManager.h"
#include "PartsCharacterBase.generated.h"

class UPartsComponent;
class UPartsSetupData;
class UPartsDamageManager;
class UParticleSystem;
class USoundBase;

/**
 * CharacterBase -> PartsCharacterBase -> DeathMatchRobotCharacter
 * Parts logic in UPartsComponent. BP: SetupData meshes + death FX event/assets.
 */
UCLASS(Abstract)
class SKYSEOUL_API APartsCharacterBase : public ACharacterBase
{
	GENERATED_BODY()

public:
	APartsCharacterBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Parts")
	bool ApplyPartDamage(FGameplayTag PartTag, float Damage);

	UFUNCTION(BlueprintCallable, Category = "Parts")
	bool ApplyPartDamageAtBone(FName BoneOrSocketName, float Damage);

	/** Attack traces call this — routes to PartsDamageManager (part + body). */
	UFUNCTION(BlueprintCallable, Category = "Parts|Damage")
	FPartsHitDamageResult ApplyHitDamage(
		const FHitResult& Hit,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Parts|Damage")
	UPartsDamageManager* GetPartsDamageManager() const { return PartsDamageManager; }

	UFUNCTION(BlueprintCallable, Category = "Parts")
	void DestroyPart(FGameplayTag PartTag);

	/** Detach remaining parts + impulse, then fire BP FX hooks. */
	UFUNCTION(BlueprintCallable, Category = "Parts|Death")
	void HandleOwnerDeathPartsExplosion();

	/** Optional. Only needed if the same pawn instance is reused. New-pawn respawn already reinits parts. */
	UFUNCTION(BlueprintCallable, Category = "Parts")
	void ResetAllParts();

	UFUNCTION(BlueprintPure, Category = "Parts")
	FGameplayTag ResolvePartTagFromBone(FName BoneOrSocketName) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool IsPartDestroyed(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	float GetPartHP(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	EPartsArmState GetLeftArmState() const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	EPartsArmState GetRightArmState() const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool IsHeadIntact() const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	bool CanShoot() const;

	UFUNCTION(BlueprintPure, Category = "Parts")
	UPartsComponent* GetPartsComponent() const { return PartsComponent; }

	UPROPERTY(BlueprintAssignable, Category = "Parts")
	FOnPartDamaged OnPartDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Parts")
	FOnPartDestroyed OnPartDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "Parts|Death")
	FOnDeathPartsExplosion OnDeathPartsExplosion;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePartDamaged(FGameplayTag PartTag, float RemainingHP);

	UFUNCTION()
	void HandlePartDestroyed(FGameplayTag PartTag);

	UFUNCTION()
	void HandleDeathPartsExplosion(FVector Origin, float Impulse);

	void PlayDeathExplosionBuiltinFX(FVector Origin) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Parts", meta = (DisplayName = "On Part Destroyed FX"))
	void BP_OnPartDestroyedFX(FGameplayTag PartTag);

	/** Override in BP to spawn Niagara/Cascade/camera shake. Origin = actor location at death. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Parts|Death", meta = (DisplayName = "On Death Parts Explosion FX"))
	void BP_OnDeathPartsExplosionFX(FVector Origin, float ImpulseStrength);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parts")
	TObjectPtr<UPartsComponent> PartsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parts|Damage")
	TObjectPtr<UPartsDamageManager> PartsDamageManager;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TObjectPtr<UPartsSetupData> PartsSetupData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death")
	float DeathExplosionImpulse = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death", meta = (ClampMin = "0.0"))
	float DebrisLifeSpan = 8.0f;

	/** Fallback if DA SeparateEffect is empty — applied to PartsComponent on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX")
	TObjectPtr<UParticleSystem> DefaultSeparateEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX")
	TObjectPtr<USoundBase> DefaultSeparateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Separate|FX", meta = (ClampMin = "0.01"))
	float DefaultSeparateEffectScale = 1.0f;

	/** Optional; C++ auto-spawns if set. Prefer BP event for custom Niagara. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death|FX")
	TObjectPtr<UParticleSystem> DeathExplosionEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death|FX")
	TObjectPtr<USoundBase> DeathExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parts|Death|FX", meta = (ClampMin = "0.01"))
	float DeathExplosionFXScale = 1.0f;
};
