#include "MyGameInstance.h"
#include "Character/SkySeoulCharacter.h"

UMyGameInstance::UMyGameInstance()
    : Super(),
    serverAddress(TEXT("ns.photonengine.io")),
    appID(TEXT("33712f15-76be-4e8c-83f4-d85827acc2f7")),
    appVersion(TEXT("1.0")),
    mpListener(new LoadBalancingListener(this)),
    mpClient(NULL)
{
}

void UMyGameInstance::Init()
{
	Super::Init();
}

void UMyGameInstance::Shutdown()
{
    if (mpClient != nullptr)
    {
        mpListener->disconnect();
        mpListener->setLBC(NULL);

        delete mpClient;
    }

	Super::Shutdown();
}

void UMyGameInstance::Connect(FString userName)
{
    mpClient = new ExitGames::LoadBalancing::Client(*mpListener, TCHAR_TO_UTF8(*appID), TCHAR_TO_UTF8(*appVersion));

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%p"), &mpListener));

    mpListener->setLBC(mpClient);
    mpClient->setDebugOutputLevel(DEBUG_RELEASE(ExitGames::Common::DebugLevel::INFO, ExitGames::Common::DebugLevel::WARNINGS));
    ExitGames::Common::Base::setListener(mpListener);
    ExitGames::Common::Base::setDebugOutputLevel(DEBUG_RELEASE(ExitGames::Common::DebugLevel::INFO, ExitGames::Common::DebugLevel::WARNINGS));

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Connecting..."));

    if (userName.IsEmpty())
    {
        mpListener->connect(ExitGames::Common::JString(L"UR") + GETTIMEMS(), TCHAR_TO_UTF8(*serverAddress));
    }
    else
    {
        mpListener->connect(TCHAR_TO_UTF8(*userName), TCHAR_TO_UTF8(*serverAddress));
    }
}

bool UMyGameInstance::Service()
{
    if (mpClient)
    {
        mpClient->service();
        return true;
    }
    return false;
}

void UMyGameInstance::CreateRoom(FString roomName)
{
    ExitGames::LoadBalancing::RoomOptions roomOptions = ExitGames::LoadBalancing::RoomOptions();
    roomOptions.setMaxPlayers((nByte)2);
    
    mpClient->opCreateRoom(TCHAR_TO_UTF8(*roomName), roomOptions);
}

void UMyGameInstance::JoinRoom(FString roomName)
{
    mpClient->opJoinRoom(TCHAR_TO_UTF8(*roomName));
}

void UMyGameInstance::SendTransform()
{
    if (!mpClient)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Not Connected to Server"));
        return;
    }

    if (localPlayerActor)
    {
        mpListener->SendTransform(localPlayerActor->GetActorTransform());
    }
}

void UMyGameInstance::SetOtherPlayerActor(int playerNr, ACharacter* player)
{
    otherPlayerCharacters.Add(playerNr, player);
}

void UMyGameInstance::SetLocalPlayerActor(AActor* player)
{
    localPlayerActor = player;
}

void UMyGameInstance::SendMove(double rotationYaw, FVector2D movementVector)
{
    if (!mpClient)
        return;

    mpListener->RaiseMove(rotationYaw, movementVector);
}

void UMyGameInstance::SendLook(FVector2D lookAxisVector)
{
    if (!mpClient)
        return;

    mpListener->RaiseLook(lookAxisVector);
}

void UMyGameInstance::SendJump(bool jump)
{
    if (!mpClient)
        return;

    mpListener->RaiseJump(jump);
}
