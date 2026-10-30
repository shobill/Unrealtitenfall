#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TitanDefence/Robot/Damage/TDDamageTypes.h"
#include "TDRobotPartDamageComponent.generated.h"

class UDamageType;
class UTDRobotPartDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDRobotPartDamaged,
	const FTDDamageResult&, Result, float, AppliedPartDamage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDRobotPartHealthChanged,
	FGameplayTag, PartTag, float, PreviousHealth, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDRobotPartHealthDepleted,
	FGameplayTag, PartTag, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDRobotBodyDamageResolved, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDRobotPartResolutionFailed, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDRobotDamageRejected, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDRobotBodyDamageReceiverChanged,
	UObject*, PreviousReceiver, UObject*, NewReceiver);

/** Resolves hit bones into robot parts, owns only part health, and forwards body damage to State. */
UCLASS(ClassGroup = (TitanDefence), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UTDRobotPartDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDRobotPartDamageComponent();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool SetBodyDamageReceiver(UObject* InBodyDamageReceiver);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	void ClearBodyDamageReceiver();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool InitializePartStates();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool ResetPartStates();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool ProcessGenericDamage(float Damage, AController* InstigatorController, AActor* DamageCauser,
		TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool ProcessPointDamage(float Damage, const FHitResult& HitResult, AController* InstigatorController,
		AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	bool ProcessRadialDamage(float Damage, FVector DamageOrigin, const FHitResult& HitInfo,
		AController* InstigatorController, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Damage")
	float GetPartCurrentHealth(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Damage")
	float GetPartMaxHealth(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Damage")
	float GetPartHealthRatio(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Damage")
	bool IsPartHealthDepleted(FGameplayTag PartTag) const;

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Damage")
	void SetDamageEnabled(bool bEnabled) { bDamageEnabled = bEnabled; }

	/** Policy for damage with no resolvable part. The default applies body damage only. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Titan Defence|Robot|Damage")
	bool ShouldApplyUnresolvedDamageToBody(ETDDamageEventType DamageEventType, FName HitBoneName) const;
	virtual bool ShouldApplyUnresolvedDamageToBody_Implementation(
		ETDDamageEventType DamageEventType, FName HitBoneName) const;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotPartDamaged OnPartDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotPartHealthChanged OnPartHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotPartHealthDepleted OnPartHealthDepleted;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotBodyDamageResolved OnBodyDamageResolved;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotPartResolutionFailed OnPartResolutionFailed;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotDamageRejected OnDamageRejected;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Damage")
	FTDRobotBodyDamageReceiverChanged OnBodyDamageReceiverChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Damage")
	bool bDamageEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Damage")
	bool bAutoBindOwnerDamageEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Damage")
	TObjectPtr<UTDRobotPartDataAsset> PartDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Damage", meta = (ClampMin = "0.0"))
	float DefaultPartDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Damage", meta = (ClampMin = "0.0"))
	float DefaultBodyDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Damage")
	TArray<FTDRobotPartDamageRule> PerPartDamageRules;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleTakePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy,
		FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection,
		const UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void HandleTakeRadialDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		FVector Origin, const FHitResult& HitInfo, AController* InstigatedBy, AActor* DamageCauser);

	bool ResolveDamage(FTDDamageResult& Result);
	bool ResolveBodyOnlyDamage(FTDDamageResult& Result);
	bool ApplyBodyDamageToReceiver(FTDDamageResult& Result);
	void RejectDamage(FTDDamageResult& Result, ETDDamageRejectionReason Reason);
	void BroadcastPartResolutionFailed(const FTDDamageResult& Result);
	FTDRobotPartRuntimeState* FindRuntimeState(FGameplayTag PartTag);
	const FTDRobotPartRuntimeState* FindRuntimeState(FGameplayTag PartTag) const;
	FGameplayTag ResolvePartTag(FName HitBoneName) const;
	const FTDRobotPartDamageRule* FindDamageRule(FGameplayTag PartTag) const;
	bool ValidateCommonRequest(FTDDamageResult& Result);
	void MarkSpecializedDamageForAnySuppression(float Damage, AController* InstigatorController,
		AActor* DamageCauser, const UDamageType* DamageType);
	bool ConsumeMatchingAnyDamage(float Damage, AController* InstigatorController,
		AActor* DamageCauser, const UDamageType* DamageType);

	UPROPERTY(Transient)
	TArray<FTDRobotPartRuntimeState> PartRuntimeStates;

	TWeakObjectPtr<UObject> BodyDamageReceiver;
	bool bPartStatesInitialized = false;
	bool bOwnerEventsBound = false;
	bool bHasPendingAnySuppression = false;
	uint64 PendingSuppressionFrame = 0;
	float PendingSuppressionDamage = 0.0f;
	TWeakObjectPtr<AController> PendingSuppressionInstigator;
	TWeakObjectPtr<AActor> PendingSuppressionCauser;
	TWeakObjectPtr<const UDamageType> PendingSuppressionDamageType;
};

