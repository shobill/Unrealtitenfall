// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "SkySeoulInputConfig.h"
#include "EnhancedInputComponent.h"
#include "PlayerControllerBase.generated.h"

/**
 * 
 */ 
UCLASS()
class SKYSEOUL_API APlayerControllerBase : public APlayerController
{
	GENERATED_BODY()
public:
	class ASkySeoulCharacter* PlayerCharacter;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;


protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	void SetupInputComponent() override;

	void RefreshPlayerCharacter();
	bool CanAcceptMoveLookInput() const;
	class ACharacter* GetControlledCharacter() const;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Jump */
	void ControllerJump();
	void ControllerStopJump();

	void LocationTrace();

	void DefaultInitialization();

	virtual void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	virtual void Input_AbilityInputTagReleased(FGameplayTag InputTag);

public:

	UFUNCTION(BlueprintCallable, Category = "PlayerController")
	UAbilitySystemComponent* GetCharacterAbilitySystemComponent() const;

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const USkySeoulInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles);

protected:
	class UMyGameInstance* mpGameInstance;
};

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
inline void APlayerControllerBase::BindAbilityActions(const USkySeoulInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles)
{
	check(InputConfig);
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	for (const FSkySeoulInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindHandles.Add(EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, PressedFunc, Action.InputTag).GetHandle());
			}

			if (ReleasedFunc)
			{
				BindHandles.Add(EnhancedInputComponent->BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			}
		}
	}
}
