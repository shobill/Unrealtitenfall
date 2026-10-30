// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/PlayerControllerBase.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

#include "MyGameInstance.h"
#include "Character/SkySeoulCharacter.h"
#include "Character/CharacterBase.h"
#include "ProjectPlayerState.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"



void APlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	RefreshPlayerCharacter();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerControllerBase: DefaultMappingContext is not set on %s"), *GetName());
		}
	}
	DefaultInitialization();

	mpGameInstance = Cast<UMyGameInstance>(GetGameInstance());
}

void APlayerControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	RefreshPlayerCharacter();
	DefaultInitialization();

	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerControllerBase: Possessed %s but it is not SkySeoulCharacter — move uses ACharacter fallback. Prefer SkySeoulCharacter parent."),
			*GetNameSafe(InPawn));
	}

	AProjectPlayerState* PS = GetPlayerState<AProjectPlayerState>();
	if (PS && PS->GetAbilitySystemComponent())
	{
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, InPawn);
	}
}

void APlayerControllerBase::RefreshPlayerCharacter()
{
	PlayerCharacter = Cast<ASkySeoulCharacter>(GetPawn());
}

ACharacter* APlayerControllerBase::GetControlledCharacter() const
{
	if (PlayerCharacter)
	{
		return PlayerCharacter;
	}
	return Cast<ACharacter>(GetPawn());
}

bool APlayerControllerBase::CanAcceptMoveLookInput() const
{
	ACharacter* Char = GetControlledCharacter();
	if (!Char)
	{
		return false;
	}

	if (const ACharacterBase* CharacterBase = Cast<ACharacterBase>(Char))
	{
		// AttributeSet not ready (DM / early possess): GetHealth()==0 would block all move via IsAlive().
		if (CharacterBase->GetMaxHealth() <= 0.0f)
		{
			return true;
		}
		return CharacterBase->IsAlive();
	}

	return true;
}

void APlayerControllerBase::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetCharacterAbilitySystemComponent());
	if (ASC)
	{
		ASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void APlayerControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		if (!MoveAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerControllerBase: MoveAction is null on %s — assign IA_Move on the PC BP (IMC alone is not enough)."), *GetName());
		}
		if (!LookAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerControllerBase: LookAction is null on %s"), *GetName());
		}

		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &APlayerControllerBase::ControllerJump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &APlayerControllerBase::ControllerStopJump);
		}

		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerControllerBase::Move);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerControllerBase::Look);
		}
	}
}

void APlayerControllerBase::Move(const FInputActionValue& Value)
{
	if (!PlayerCharacter)
	{
		RefreshPlayerCharacter();
	}
	if (!CanAcceptMoveLookInput())
	{
		return;
	}

	ACharacter* Char = GetControlledCharacter();
	if (!Char)
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	Char->AddMovementInput(ForwardDirection, MovementVector.Y);
	Char->AddMovementInput(RightDirection, MovementVector.X);

	if (mpGameInstance)
	{
		mpGameInstance->SendMove(Rotation.Yaw, MovementVector);
	}
}

void APlayerControllerBase::Look(const FInputActionValue& Value)
{
	if (!PlayerCharacter)
	{
		RefreshPlayerCharacter();
	}
	if (!CanAcceptMoveLookInput())
	{
		return;
	}

	ACharacter* Char = GetControlledCharacter();
	if (!Char)
	{
		return;
	}

	if (ACharacterBase* CharacterBase = Cast<ACharacterBase>(Char))
	{
		if (UAbilitySystemComponent* ASC = CharacterBase->GetAbilitySystemComponent())
		{
			const FGameplayTagContainer& Container = ASC->GetOwnedGameplayTags();
			if (Container.HasTag(FGameplayTag::RequestGameplayTag(FName("State.Debuff.CameraLock"), false)))
			{
				return;
			}
		}
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	Char->AddControllerYawInput(LookAxisVector.X);
	Char->AddControllerPitchInput(LookAxisVector.Y);
}

void APlayerControllerBase::ControllerJump()
{
	if (!PlayerCharacter)
	{
		RefreshPlayerCharacter();
	}
	if (!CanAcceptMoveLookInput())
	{
		return;
	}

	if (ACharacter* Char = GetControlledCharacter())
	{
		Char->Jump();
	}

	if (mpGameInstance)
	{
		mpGameInstance->SendJump(true);
	}
}

void APlayerControllerBase::ControllerStopJump()
{
	if (!PlayerCharacter)
	{
		RefreshPlayerCharacter();
	}

	if (ACharacter* Char = GetControlledCharacter())
	{
		Char->StopJumping();
	}

	if (mpGameInstance)
	{
		mpGameInstance->SendJump(false);
	}
}

void APlayerControllerBase::LocationTrace()
{
}

void APlayerControllerBase::DefaultInitialization()
{
	if (!PlayerCharacter)
	{
		RefreshPlayerCharacter();
	}
	if (!PlayerCharacter)
	{
		return;
	}
	if (const USkySeoulInputConfig* InputConfig = PlayerCharacter->InputConfig)
	{
		TArray<uint32> BindHandles;
		BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
	}
}

void APlayerControllerBase::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetCharacterAbilitySystemComponent()))
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void APlayerControllerBase::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetCharacterAbilitySystemComponent()))
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

UAbilitySystemComponent* APlayerControllerBase::GetCharacterAbilitySystemComponent() const
{
	const AProjectPlayerState* PPS = CastChecked<AProjectPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
	if (PPS)
		return PPS->GetAbilitySystemComponent();
	else
		return nullptr;
}
