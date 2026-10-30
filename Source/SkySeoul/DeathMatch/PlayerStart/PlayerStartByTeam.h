// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"
#include "PlayerStartByTeam.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API APlayerStartByTeam : public APlayerStart
{
	GENERATED_BODY()

public:
    // 이 스폰 포인트가 속한 팀 ID (GenericTeamId::GetId()와 매칭)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
    uint8 TeamId = 255; // 255 = NoTeam 느낌으로 사용
};
