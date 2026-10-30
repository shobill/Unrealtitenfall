#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TitanDefence/Robot/Damage/TDDamageTypes.h"
#include "TDSimpleDamageComponent.generated.h"

class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDBodyDamageResolved, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDDamageRejected, const FTDDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDBodyDamageReceiverChanged,
	UObject*, PreviousReceiver, UObject*, NewReceiver);

/** Resolves official Unreal damage events and forwards body damage to an explicitly supplied State. */
UCLASS(ClassGroup = (TitanDefence), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UTDSimpleDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDSimpleDamageComponent();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	bool SetBodyDamageReceiver(UObject* InBodyDamageReceiver);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	void ClearBodyDamageReceiver();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	bool ProcessGenericDamage(float Damage, AController* InstigatorController, AActor* DamageCauser,
		TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	bool ProcessPointDamage(float Damage, const FHitResult& HitResult, AController* InstigatorController,
		AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	bool ProcessRadialDamage(float Damage, FVector DamageOrigin, const FHitResult& HitInfo,
		AController* InstigatorController, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Damage")
	void SetDamageEnabled(bool bEnabled) { bDamageEnabled = bEnabled; }

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Damage")
	FTDBodyDamageResolved OnBodyDamageResolved;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Damage")
	FTDDamageRejected OnDamageRejected;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Damage")
	FTDBodyDamageReceiverChanged OnBodyDamageReceiverChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Damage")
	bool bDamageEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Damage")
	bool bAutoBindOwnerDamageEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Damage", meta = (ClampMin = "0.0"))
	float IncomingDamageMultiplier = 1.0f;

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
	void RejectDamage(FTDDamageResult& Result, ETDDamageRejectionReason Reason);
	void MarkSpecializedDamageForAnySuppression(float Damage, AController* InstigatorController,
		AActor* DamageCauser, const UDamageType* DamageType);
	bool ConsumeMatchingAnyDamage(float Damage, AController* InstigatorController,
		AActor* DamageCauser, const UDamageType* DamageType);

	TWeakObjectPtr<UObject> BodyDamageReceiver;
	bool bOwnerEventsBound = false;
	bool bHasPendingAnySuppression = false;
	uint64 PendingSuppressionFrame = 0;
	float PendingSuppressionDamage = 0.0f;
	TWeakObjectPtr<AController> PendingSuppressionInstigator;
	TWeakObjectPtr<AActor> PendingSuppressionCauser;
	TWeakObjectPtr<const UDamageType> PendingSuppressionDamageType;
};

