#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PartsDamageManager.generated.h"

class UPartsComponent;
class APartsCharacterBase;
class AController;

USTRUCT(BlueprintType)
struct FPartsHitDamageResult
{
	GENERATED_BODY()

    // Native routing outcome; static text, no per-hit allocation.
    const TCHAR* ZeroDamageReason = TEXT("None");

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Damage")
	bool bApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Damage")
	bool bHitPart = false;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Damage")
	FGameplayTag HitPartTag;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Damage")
	float PartDamageApplied = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Parts|Damage")
	float BodyDamageApplied = 0.0f;
};

DECLARE_MULTICAST_DELEGATE_FiveParams(FPartsAppliedDamage, const FPartsHitDamageResult&, AController*, AActor*, const FVector&, FName);

/**
 * Hit damage router: attack traces call ApplyHitDamage.
 * Splits into part HP (PartsComponent) + body HP (owner CharacterBase TakeDamage).
 */
UCLASS(ClassGroup = (Parts), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UPartsDamageManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartsDamageManager();
    FPartsAppliedDamage OnAppliedDamage;
    bool IsRoutingDamage() const { return bRoutingDamage; }

	UFUNCTION(BlueprintCallable, Category = "Parts|Damage")
	FPartsHitDamageResult ApplyHitDamage(
		const FHitResult& Hit,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Parts|Damage")
	FPartsHitDamageResult ApplyDamageToPart(
		FGameplayTag PartTag,
		float Damage,
		AController* InstigatorController,
		AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "Parts|Damage")
	UPartsComponent* GetPartsComponent() const;

    FGameplayTag ResolveHitPartTag(const FHitResult& Hit) const;

protected:
	float ApplyBodyDamage(float BodyDamage, AController* InstigatorController, AActor* DamageCauser);
    bool bRoutingDamage = false;
    const FHitResult* RoutedHit = nullptr;
	APartsCharacterBase* GetPartsOwner() const;

	/** Multiplier applied to part HP when a part is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Damage", meta = (ClampMin = "0.0"))
	float PartDamageScale = 1.0f;

	/** Fraction of incoming damage also applied to body HP when a part is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BodyDamageOnPartHitScale = 0.5f;

	/** Fraction of damage to body when no part was resolved (torso / miss-map). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Damage", meta = (ClampMin = "0.0"))
	float BodyDamageOnNoPartScale = 1.0f;
};
