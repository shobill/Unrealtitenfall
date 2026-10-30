// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "SkySeoulCharacter.h"
#include "InteractionInterface.h"
#include "RobotCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHackingUpdate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerSelectUpdate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRobotStateUpdate);

UENUM(BlueprintType)
enum class ERobotState : uint8
{
	E_Peace UMETA(DisplayName = "Peace"),
	E_PreemptiveAttack UMETA(DisplayName = "PreemptiveAttack"),
	E_UnderAttack UMETA(DisplayName = "UnderAttack"),
};

UENUM(BlueprintType)
enum class ERobotActionState : uint8
{
	E_Idle UMETA(DisplayName = "Idle"),
	E_FollowCharacter UMETA(DisplayName = "FollowCharacter"),
	E_MoveLocation UMETA(DisplayName = "MoveLocation"),
	E_Explosion UMETA(DisplayName = "Explosion"),
};


UCLASS()
class SKYSEOUL_API ARobotCharacter : public ACharacterBase, public IInteractionInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ARobotCharacter(const class FObjectInitializer& ObjectInitializer);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class ACharacter* OwnerCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UWidgetComponent* InteractionWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UWidgetComponent* ButtonWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERobotState RobotState;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERobotActionState ActionState;

	UPROPERTY(BlueprintAssignable)
	FHackingUpdate HackingUpdate;
	UPROPERTY(BlueprintAssignable)
	FPlayerSelectUpdate PlayerSelectUpdate;

	UPROPERTY(BlueprintAssignable)
	FRobotStateUpdate RobotStateUpdate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite , Category = "HackingData")
	float HackingPercent = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	float CurrentHackingPercent = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	float MaxHackingPercent = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	bool IsSelect = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotData")
	int32 RobotCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RobotData")
	bool IsHacking = false;

	FTimerHandle HackingAbortTimer;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	void HackingStart(AActor* Caller);
	void HackingAbort();
	void HackingEnd(AActor* Caller);

	UFUNCTION(BlueprintCallable)
	void MoveAction(FVector GoalLocation);

	UFUNCTION(BlueprintCallable)
	void SetRobotActionState(ERobotActionState Value);
	UFUNCTION(BlueprintCallable)
	void RobotExplosion(TArray<FHitResult>& HitResult);


	UFUNCTION(BlueprintCallable)
	void RobotDie();


	UFUNCTION(BlueprintCallable)
	void RobotControllerDestroy();

	UFUNCTION(BlueprintCallable)
	void RobotIsFree();
	UFUNCTION(BlueprintCallable)
	void SetSelect(bool Value);
	UFUNCTION(BlueprintCallable)
	void StateChange(ERobotState State);


	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnInteract(AActor* Caller);
	virtual void OnInteract_Implementation(AActor* Caller);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void StartFocus();
	virtual void StartFocus_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void EndFocus();
	virtual void EndFocus_Implementation();
};
