// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/Character.h"
#include "DeathMatchGameInstane.generated.h"

/**
 * 
 */

class ACharacter;

UCLASS()
class SKYSEOUL_API UDeathMatchGameInstane : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
private:
	UPROPERTY()
	TSubclassOf<ACharacter> PayloadCharacter;
	UPROPERTY()
	TSubclassOf<AController> PayloadController;

public:
	UFUNCTION(BlueprintCallable, Category = "DeathMatchGameInstane")
	void SetPayloadCharacter(TSubclassOf<ACharacter> InCharacter);

	UFUNCTION(BlueprintCallable, Category = "DeathMatchGameInstane")
	void SetPayloadController(TSubclassOf<AController> InCharacter);

	UFUNCTION(BlueprintCallable, Category = "DeathMatchGameInstane")
	TSubclassOf<ACharacter> GetPayloadCharacter() const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatchGameInstane")
	TSubclassOf<AController> GetPayloadController() const;
};
