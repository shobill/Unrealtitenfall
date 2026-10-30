// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "Logging/LogMacros.h"
#include "Character/Abilities/AbilitySetData.h"
#include "SkySeoulCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAbilitySetData;
class UEquipmentComponent;
class ARobotCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRobotUpdated);

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);


UENUM(BlueprintType)
enum class EPlayerActionState : uint8
{
	E_None UMETA(DisplayName = "None"),
	E_Weapon1 UMETA(DisplayName = "Weapon1"),
	E_Weapon2 UMETA(DisplayName = "Weapon2"),
	E_ConsumeItem UMETA(DisplayName = "ConsumeItem"),
	E_ThrowItem UMETA(DisplayName = "ThrowItem"),
	E_ZoomAction UMETA(DisplayName = "ZoomIn"),
	E_TargetAction UMETA(DisplayName = "TargetAction"),
	E_Phone UMETA(DisplayName = "Phone"),
	E_LapTop UMETA(DisplayName = "LapTop"),
	E_BeGrabbed UMETA(DisplayName = "BeGrabbed"),
	E_Down UMETA(DisplayName = "Down"),
	E_Stun UMETA(DisplayName = "Stun"),
	E_RobotHacking UMETA(DisplayName = "RobotHacking"),
	E_PeanutJump UMETA(DisplayName = "PeanutJump")
};


UCLASS(config=Game)
class ASkySeoulCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASkySeoulCharacter(const class FObjectInitializer& ObjectInitializer);

protected:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Camera")
	float StartingCameraBoomArmLength;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Camera")
	FVector StartingCameraBoomLocation;

	bool ASCInputBound = false;

	FGameplayTag DeadTag;

	virtual void OnRep_PlayerState() override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Input")
	TObjectPtr<class USkySeoulInputConfig> InputConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|State")
	EPlayerActionState PlayerActionState = EPlayerActionState::E_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interection")
	AActor* FocusedActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Robot")
	int32 SelectRobotNumber = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Select")
	int32 SelectNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot")
	FVector RobotLocation;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot")
	TArray<ARobotCharacter*> RobotArray;

	UPROPERTY(BlueprintAssignable)
	FOnRobotUpdated OnRobotUpdated;

	FTimerHandle RobotLocationTimer;
	FTimerHandle InterectionTimer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	UEquipmentComponent*	EquipComponent;
	
protected:	
	// To add mapping context
	virtual void BeginPlay();

	// Called every frame
	virtual void Tick(float DeltaTime) override;
public:
	UInventoryComponent* GetInventoryComponent() {
		return InventoryComponent;
	};

	virtual void PossessedBy(AController* NewController) override;

	void ForwardTrace();

	UFUNCTION(BlueprintCallable)
	void LocationTrace();

	void Interaction();
	void StartFocus(AActor* TraceActor);
	void EndFocus(AActor* TraceActor);

	bool RobotCostCheck(int32 RobotCost);
	bool AddRobot(ARobotCharacter* RobotCharacter);

	UFUNCTION(BlueprintCallable)
	void RemoveRobot(int32 Value);

	void RemoveRobot(ARobotCharacter* Robot);


	UFUNCTION(BlueprintCallable)
	void ChangeRobotLeft();
	UFUNCTION(BlueprintCallable)
	void ChangeRobotRight();
	void ChangeRobot(const int32 Value);

	void SelectNumberReset();
	void RequestSelectAction(const int32 Num);

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	UFUNCTION(BlueprintCallable, Category = "Character|Camera")
	float GetStartingCameraBoomArmLength();

	UFUNCTION(BlueprintCallable, Category = "Character|Camera")
	FVector GetStartingCameraBoomLocation();

	void SetPlayerActionState(FGameplayTag StateTag);


	//-------------------------------------------------------------------------------------------------------------------------


	// 블루 프린트로 이용중이라 테스트 종료 후 필요 없을 시 삭제 무방
public:
	// 점프키는 설정에 없어 굳이 넣지 않음
	UFUNCTION(BlueprintCallable)
	void Attack();
	UFUNCTION(BlueprintCallable)
	void MeleeAttack();

protected:


private:


	// ----------------------------------------------------------------------- //
			// Value

public:
	UPROPERTY(EditAnywhere, Category = "Option Setting")
	float MouseSensitivity = 15;

	UPROPERTY(EditAnywhere, Category = "Combat Stat")
	bool MaxRange;

protected:

};