#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "Common/LoadBalancingListener.h"
#include "Common-cpp/inc/Common.h"

#include "MyGameInstance.generated.h"

UCLASS()
class SKYSEOUL_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UMyGameInstance();
	virtual void Init() override;
	virtual void Shutdown() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString serverAddress;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString appID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString appVersion;

	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void Connect(FString userName);
	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	bool Service();
	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void CreateRoom(FString roomName);
	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void JoinRoom(FString roomName);

	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void SendTransform();
	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void SetOtherPlayerActor(int playerNr, ACharacter* player);

	UFUNCTION(BlueprintCallable, Category = "MyPhoton")
	void SetLocalPlayerActor(AActor* player);

	UFUNCTION(BlueprintImplementableEvent, Category = "MyPhoton")
	void ConnectReturn();
	UFUNCTION(BlueprintImplementableEvent, Category = "MyPhoton")
	void UpdateRoomList(const TArray<FString>& roomNames);
	UFUNCTION(BlueprintImplementableEvent, Category = "MyPhoton")
	void JoinRoomReturn();
	UFUNCTION(BlueprintImplementableEvent, Category = "MyPhoton")
	void CreateOtherPlayer(int playerNr, FTransform transform);


	void SendMove(double rotationYaw, FVector2D movementVector);
	void SendLook(FVector2D lookAxisVector);
	void SendJump(bool jump);


	TMap<int, ACharacter*> otherPlayerCharacters;

private:
	LoadBalancingListener* mpListener;
	ExitGames::LoadBalancing::Client* mpClient;

	AActor* localPlayerActor;
};
