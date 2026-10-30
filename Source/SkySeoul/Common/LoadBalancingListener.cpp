#include "LoadBalancingListener.h"
#include "../MyGameInstance.h"
#include "Character/SkySeoulCharacter.h"

using namespace ExitGames::Common;
using namespace ExitGames::LoadBalancing;

const ExitGames::Common::JString PeerStatesStr[] = {
	L"Uninitialized",
	L"PeerCreated",
	L"ConnectingToNameserver",
	L"ConnectedToNameserver",
	L"DisconnectingFromNameserver",
	L"Connecting",
	L"Connected",
	L"WaitingForCustomAuthenticationNextStepCall",
	L"Authenticated",
	L"JoinedLobby",
	L"DisconnectingFromMasterserver",
	L"ConnectingToGameserver",
	L"ConnectedToGameserver",
	L"AuthenticatedOnGameServer",
	L"Joining",
	L"Joined",
	L"Leaving",
	L"Left",
	L"DisconnectingFromGameserver",
	L"ConnectingToMasterserver",
	L"ConnectedComingFromGameserver",
	L"AuthenticatedComingFromGameserver",
	L"Disconnecting",
	L"Disconnected"
};

LoadBalancingListener::LoadBalancingListener(UMyGameInstance* pGameInstance) : mpLbc(NULL), mpGameInstance(pGameInstance), mConnected(false)
{
}

LoadBalancingListener::~LoadBalancingListener()
{
}

void LoadBalancingListener::setLBC(ExitGames::LoadBalancing::Client* pLbc)
{
	mpLbc = pLbc;
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%p"), mpLbc));
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%p"), &mpLbc));
}

void LoadBalancingListener::connect(const ExitGames::Common::JString& userName, const ExitGames::Common::JString& serverAddress)
{
	if (mConnected == false)
	{
		ConnectOptions connectOptions = ConnectOptions().setAuthenticationValues(AuthenticationValues().setUserID(userName)).setUsername(userName).setTryUseDatagramEncryption(true);
		if (serverAddress.length())
			connectOptions.setServerAddress(serverAddress);
		mpLbc->connect(connectOptions);
	}
	else
	{
		connectReturn(1, "already connected", "kr", "");
	}
}

void LoadBalancingListener::disconnect(void)
{
	if (mpLbc)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%d"), mpLbc->getIsInLobby()));
		mpLbc->disconnect();
	}
}

void LoadBalancingListener::setUseGroups(bool value)
{
}

bool LoadBalancingListener::getUseGroups(void)
{
	return false;
}

void LoadBalancingListener::updateGroups(void)
{
}

void LoadBalancingListener::setSendGroup(nByte value)
{
}

void LoadBalancingListener::service(void)
{
}

void LoadBalancingListener::createRoom(void)
{
}

void LoadBalancingListener::setAutomove(bool a)
{
}

bool LoadBalancingListener::getAutomove(void)
{
	return false;
}

void LoadBalancingListener::changeRandomColor(void)
{
}

void LoadBalancingListener::nextGridSize(void)
{
}

bool LoadBalancingListener::setLocalPlayerPos(int x, int y)
{
	return false;
}

void LoadBalancingListener::moveLocalPlayer(void)
{
}

void LoadBalancingListener::SendTransform(FTransform transform)
{
	Hashtable data;

	//double location[] = { transform.GetLocation().X, transform.GetLocation().Y, transform.GetLocation().Z };
	//double location[] = { 0.0, 0.0, 0.0 };
	//data.put(1, location, 3);
	data.put(1, transform.GetLocation().XYZ, 3);
	double rotation[] = { transform.GetRotation().X, transform.GetRotation().Y, transform.GetRotation().Z, transform.GetRotation().W };
	data.put(2, rotation, 4);

	mpLbc->opRaiseEvent(false, data, 1);

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("RaiseEvent succeeded"));
}

void LoadBalancingListener::RaiseMove(double rotationYaw, FVector2D movementVector)
{
	Hashtable data;

	double values[] = { rotationYaw, movementVector.X, movementVector.Y };

	data.put(1, values, 3);
	mpLbc->opRaiseEvent(true, data, 2);
}

void LoadBalancingListener::RaiseLook(FVector2D lookAxisVector)
{
	Hashtable data;

	data.put(1, lookAxisVector.XY, 2);
	mpLbc->opRaiseEvent(true, data, 3);
}

void LoadBalancingListener::RaiseJump(bool jump)
{
	Hashtable data;

	data.put(1, jump);
	mpLbc->opRaiseEvent(true, data, 4);
}

// 콜백함수들

void LoadBalancingListener::debugReturn(int debugLevel, const ExitGames::Common::JString& string)
{
}

void LoadBalancingListener::connectionErrorReturn(int errorCode)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("connection failed with error"));
}

void LoadBalancingListener::clientErrorReturn(int errorCode)
{
}

void LoadBalancingListener::warningReturn(int warningCode)
{
}

void LoadBalancingListener::serverErrorReturn(int errorCode)
{
}

void LoadBalancingListener::joinRoomEventAction(int playerNr, const ExitGames::Common::JVector<int>& playernrs, const ExitGames::LoadBalancing::Player& player)
{
}

void LoadBalancingListener::leaveRoomEventAction(int playerNr, bool isInactive)
{
	if (mpGameInstance->otherPlayerCharacters.Contains(playerNr))
	{
		AActor* otherPlayerActor = *(mpGameInstance->otherPlayerCharacters.Find(playerNr));
		mpGameInstance->otherPlayerCharacters.Remove(playerNr);
		otherPlayerActor->Destroy();
	}
}

void LoadBalancingListener::customEventAction(int playerNr, nByte eventCode, const ExitGames::Common::Object& eventContentObj)
{
	Hashtable eventContent = ValueObject<Hashtable>(eventContentObj).getDataCopy();

	switch (eventCode)
	{
	case 1:
	{
		// 1 : player location, rotation

		ACharacter* otherPlayerCharacter;
		FTransform targetTransform;

		const Object* obj = eventContent.getValue(1);
		if (obj)
		{
			double* data = ((ValueObject<double*>*)obj)->getDataCopy();
			targetTransform.SetLocation(FVector(data[0], data[1], data[2]));
		}
		obj = eventContent.getValue(2);
		if (obj)
		{
			double* data = ((ValueObject<double*>*)obj)->getDataCopy();
			targetTransform.SetRotation(FQuat(data[0], data[1], data[2], data[3]));
		}

		if (!mpGameInstance->otherPlayerCharacters.Contains(playerNr))
		{
			mpGameInstance->CreateOtherPlayer(playerNr, targetTransform);
		}

		otherPlayerCharacter = *(mpGameInstance->otherPlayerCharacters.Find(playerNr));

		//otherPlayerCharacter->SetActorTransform(targetTransform);
	}
	break;
	case 2:
	{
		const Object* obj = eventContent.getValue(1);
		if (obj)
		{
			double* data = ((ValueObject<double*>*)obj)->getDataCopy();

			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%f, %f, %f"), data[0], data[1], data[2]));

			const FRotator YawRotation(0, data[0], 0);
			const FVector2D movementVector(data[1], data[2]);

			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			ACharacter* otherPlayerCharacter = *(mpGameInstance->otherPlayerCharacters.Find(playerNr));

			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%d"), playerNr));


			otherPlayerCharacter->AddMovementInput(ForwardDirection, data[2]);
			otherPlayerCharacter->AddMovementInput(RightDirection, data[1]);
		}
	}
	break;
	case 3:
	{
		const Object* obj = eventContent.getValue(1);
		if (obj)
		{
			double* data = ((ValueObject<double*>*)obj)->getDataCopy();

			FVector2D lookAxisVector = FVector2D(data[0], data[1]);
		}
	}
	break;
	case 4:
	{
		const Object* obj = eventContent.getValue(1);
		if (obj)
		{
			bool jump = ((ValueObject<bool>*)obj)->getDataCopy();

			ACharacter* otherPlayerCharacter = *(mpGameInstance->otherPlayerCharacters.Find(playerNr));

			if (jump)
			{
				otherPlayerCharacter->Jump();
			}
			else
			{
				otherPlayerCharacter->StopJumping();
			}
		}
	}
	break;
	default:
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("undefine event"));
		break;
	}
}

void LoadBalancingListener::connectReturn(int errorCode, const ExitGames::Common::JString& errorString, const ExitGames::Common::JString& region, const ExitGames::Common::JString& cluster)
{
	updateState();
	if (errorCode == ErrorCode::OK)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("ConnectReturn : Connection succeeded!"));

		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, mpLbc->getUserID().UTF8Representation().cstr());

		mConnected = true;

		if (mpGameInstance == nullptr)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("mpGameInstance is empty"));
		}
		else
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("mpGameInstance is live"));
		}

		mpGameInstance->ConnectReturn();
	}
	else if (errorCode == 1)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("ConnectReturn : already connected"));

		mConnected = true;

		mpGameInstance->ConnectReturn();
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("ConnectReturn : Connection fail"));
	}
}

void LoadBalancingListener::disconnectReturn(void)
{
	updateState();
	mConnected = false;
}

void LoadBalancingListener::createRoomReturn(int localPlayerNr, const ExitGames::Common::Hashtable& gameProperties, const ExitGames::Common::Hashtable& playerProperties, int errorCode, const ExitGames::Common::JString& errorString)
{
	updateState();
	if (errorCode == ErrorCode::OK)
	{
		mpGameInstance->JoinRoomReturn();
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Err : create room fail %d"), errorCode));
	}
}

void LoadBalancingListener::joinOrCreateRoomReturn(int localPlayerNr, const ExitGames::Common::Hashtable& gameProperties, const ExitGames::Common::Hashtable& playerProperties, int errorCode, const ExitGames::Common::JString& errorString)
{
}

void LoadBalancingListener::joinRoomReturn(int localPlayerNr, const ExitGames::Common::Hashtable& gameProperties, const ExitGames::Common::Hashtable& playerProperties, int errorCode, const ExitGames::Common::JString& errorString)
{
	if (errorCode == ErrorCode::OK)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("JoinRoomReturn"));
		mpGameInstance->JoinRoomReturn();
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("CantJoinRoom"));
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%d"), errorCode));
	}
}

void LoadBalancingListener::joinRandomRoomReturn(int localPlayerNr, const ExitGames::Common::Hashtable& gameProperties, const ExitGames::Common::Hashtable& playerProperties, int errorCode, const ExitGames::Common::JString& errorString)
{
}

void LoadBalancingListener::leaveRoomReturn(int errorCode, const ExitGames::Common::JString& errorString)
{
}

void LoadBalancingListener::gotQueuedReturn(void)
{
}

void LoadBalancingListener::joinLobbyReturn(void)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("joinLobbyReturn"));
}

void LoadBalancingListener::leaveLobbyReturn(void)
{
}

void LoadBalancingListener::onRoomListUpdate(void)
{
	const JVector<Room*>& rooms = mpLbc->getRoomList();
	JVector<JString> jnames(rooms.getSize());
	for (unsigned int i = 0; i < rooms.getSize(); ++i)
		jnames.addElement(rooms[i]->getName());

	TArray<FString> fnames;
	for (unsigned int i = 0; i < jnames.getSize(); i++)
		fnames.Push(FString(jnames[i].UTF8Representation().cstr()));

	mpGameInstance->UpdateRoomList(fnames);
}

void LoadBalancingListener::onLobbyStatsUpdate(const ExitGames::Common::JVector<ExitGames::LoadBalancing::LobbyStatsResponse>& lobbyStats)
{
}

void LoadBalancingListener::onLobbyStatsResponse(const ExitGames::Common::JVector<ExitGames::LoadBalancing::LobbyStatsResponse>& lobbyStats)
{
}

void LoadBalancingListener::onRoomPropertiesChange(const ExitGames::Common::Hashtable& changes)
{
}

void LoadBalancingListener::updateState(void)
{
}

void LoadBalancingListener::afterRoomJoined(int localPlayerNr)
{
}

bool LoadBalancingListener::updateGridSize(const ExitGames::Common::Hashtable& props)
{
	return false;
}

void LoadBalancingListener::raiseColorEvent(void)
{
}

int LoadBalancingListener::getGroupByPos(void)
{
	return 0;
}

