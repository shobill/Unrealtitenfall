#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TitanDefence/Grapple/Tests/TDGrappleTestAbility.h"
#include "TitanDefence/Grapple/GAS/TDGrappleTags.h"
#include "TitanDefence/Grapple/GAS/TDGrappleAbilitySet.h"
#include "TitanDefence/Grapple/TDGrappleComponent.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "CableComponent.h"
#include "TitanDefence/TDPlayerCharacte.h"
#include "Controller/PlayerControllerBase.h"
#include "ProjectPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDGrappleGASLifecycleTest, "SkySeoul.TitanDefense.Grapple.GASLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDGrappleGASLifecycleTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues IV = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TDGrappleGASTest"), nullptr,
		true, ERHIFeatureLevel::Num, &IV);
	AActor* Owner = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	UTDGrappleComponent* Grapple = NewObject<UTDGrappleComponent>(Owner);
	Grapple->RegisterComponent();
	UCharacterAbilitySystemComponent* ASC = NewObject<UCharacterAbilitySystemComponent>(Owner);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Owner, Owner);
	Owner->DispatchBeginPlay();
	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(50));
	Box->SetMobility(EComponentMobility::Static);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetWorldLocation(FVector(1000,0,0));
	Box->RegisterComponent();
	FGameplayAbilitySpec Spec(UTDGrappleTestAbility::StaticClass());
	Spec.GetDynamicSpecSourceTags().AddTag(TDGrappleTags::Input_Grapple);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	auto Press = [ASC]()
	{
		ASC->AbilityInputTagPressed(TDGrappleTags::Input_Grapple);
		ASC->ProcessAbilityInput(1.f/60.f, false);
	};
	auto Release = [ASC]()
	{
		ASC->AbilityInputTagReleased(TDGrappleTags::Input_Grapple);
		ASC->ProcessAbilityInput(1.f/60.f, false);
	};
	auto Active = [ASC, Handle]() { return ASC->FindAbilitySpecFromHandle(Handle)->IsActive(); };
	Press();
	TestTrue(TEXT("Input tag activates GA and attaches terrain"), Active() && Grapple->IsGrappling());
	TestTrue(TEXT("GAS owns active grapple state tag"), ASC->HasMatchingGameplayTag(TDGrappleTags::State_Grappling));
	Release();
	TestFalse(TEXT("WaitInputRelease ends GA"), Active());
	TestFalse(TEXT("Release removes active tag"), ASC->HasMatchingGameplayTag(TDGrappleTags::State_Grappling));
	TestFalse(TEXT("Release hides cable"), Grapple->Cable->IsVisible());
	const FGameplayTag Dead = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
	const FGameplayTag Stun = FGameplayTag::RequestGameplayTag(TEXT("State.Debuff.Stun"));
	ASC->AddLooseGameplayTag(Dead);
	Press();
	TestFalse(TEXT("Dead blocks activation"), Active() || Grapple->IsGrappling());
	Release();
	ASC->RemoveLooseGameplayTag(Dead);
	ASC->AddLooseGameplayTag(Stun);
	Press();
	TestFalse(TEXT("Stun blocks activation"), Active() || Grapple->IsGrappling());
	Release();
	ASC->RemoveLooseGameplayTag(Stun);
	Press();
	ASC->AddLooseGameplayTag(Stun);
	TestFalse(TEXT("Stun task cancels active GA and cable"), Active() || Grapple->IsGrappling());
	TestFalse(TEXT("Canceled active state tag removed"), ASC->HasMatchingGameplayTag(TDGrappleTags::State_Grappling));
	ASC->RemoveLooseGameplayTag(Stun);
	ASC->ProcessAbilityInput(1.f/60.f, false);
	TestFalse(TEXT("Held input does not reactivate after stun clears"), Active());
	Release();
	Press();
	ASC->AddLooseGameplayTag(Dead);
	TestFalse(TEXT("Death task cancels active GA"), Active() || Grapple->IsGrappling());
	Release();
	ASC->RemoveLooseGameplayTag(Dead);
	Press();
	ASC->CancelAbilityHandle(Handle);
	TestFalse(TEXT("External GAS cancel releases cable"), Grapple->IsGrappling());
	Release();
	Press();
	Grapple->ReleaseGrapple();
	TestFalse(TEXT("External connection loss ends GA without recursion"), Active());
	Release();
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Press();
	TestFalse(TEXT("Miss ends ability and leaves no active state"), Active() || ASC->HasMatchingGameplayTag(TDGrappleTags::State_Grappling));
	Release();
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Press();
	TestTrue(TEXT("Can reconnect after failed activation"), Active() && Grapple->IsGrappling());
	ASC->ClearAbility(Handle);
	TestFalse(TEXT("Removing active ability clears cable and tag"), Grapple->IsGrappling() || ASC->HasMatchingGameplayTag(TDGrappleTags::State_Grappling));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDGrappleAbilitySetTest, "SkySeoul.TitanDefense.Grapple.AbilitySet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDGrappleAbilitySetTest::RunTest(const FString& Parameters)
{
	// GiveAbility는 권한이 있는 소유자를 요구합니다.
	const UWorld::InitializationValues IV = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TDGrappleSetTest"), nullptr,
		true, ERHIFeatureLevel::Num, &IV);
	AActor* Owner = World->SpawnActor<AActor>();
	UCharacterAbilitySystemComponent* ASC = NewObject<UCharacterAbilitySystemComponent>(Owner);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Owner, Owner);
	FAbilitySetData_GrantedHandles Granted;
	GetDefault<UTDGrappleAbilitySet>()->GiveToAbilitySystem(ASC, &Granted, Owner);
	TestEqual(TEXT("AbilitySet grants one ability"), ASC->GetActivatableAbilities().Num(), 1);
	if (ASC->GetActivatableAbilities().Num())
		TestTrue(TEXT("Granted spec carries grapple input tag"), ASC->GetActivatableAbilities()[0].GetDynamicSpecSourceTags().HasTagExact(TDGrappleTags::Input_Grapple));
	Granted.TakeFromAbilitySystem(ASC);
	TestEqual(TEXT("Grant handles remove owned ability"), ASC->GetActivatableAbilities().Num(), 0);
	Granted.TakeFromAbilitySystem(ASC);
	TestEqual(TEXT("Repeated cleanup is safe"), ASC->GetActivatableAbilities().Num(), 0);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTDGrapplePossessionTest, "SkySeoul.TitanDefense.Grapple.PlayerStateLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTDGrapplePossessionTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues IV = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TDGrapplePossessionTest"), nullptr,
		true, ERHIFeatureLevel::Num, &IV);
	APlayerControllerBase* PC = World->SpawnActor<APlayerControllerBase>();
	AProjectPlayerState* PS = World->SpawnActor<AProjectPlayerState>();
	PS->SetOwner(PC);
	PC->SetPlayerState(PS);
	// 최소 테스트 월드는 게임 시작 초기화를 하지 않으므로 속성 서브오브젝트를 명시적으로 등록합니다.
	UCharacterAbilitySystemComponent* ASC = CastChecked<UCharacterAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	ASC->AddAttributeSetSubobject(PS->GatAttributeSetBase());
	ATDPlayerCharacte* Player = World->SpawnActor<ATDPlayerCharacte>();
	PC->Possess(Player);
	TestTrue(TEXT("Uses PlayerState ASC rather than another component"), Player->GetAbilitySystemComponent() == ASC);
	auto CountGrapple = [ASC]()
	{
		int32 Count = 0;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(TDGrappleTags::Input_Grapple)) ++Count;
		return Count;
	};
	TestEqual(TEXT("Possession grants one grapple"), CountGrapple(), 1);
	Player->PossessedBy(PC);
	TestEqual(TEXT("Repeated initialization does not duplicate grant"), CountGrapple(), 1);
	const FGameplayAbilitySpecHandle Other = ASC->GiveAbility(FGameplayAbilitySpec(UCharacterGameplayAbility::StaticClass()));
	PC->UnPossess();
	TestEqual(TEXT("Unpossess removes grapple from persistent PlayerState"), CountGrapple(), 0);
	TestNotNull(TEXT("Other abilities are preserved"), ASC->FindAbilitySpecFromHandle(Other));
	PC->Possess(Player);
	TestEqual(TEXT("Repossess grants one fresh grapple"), CountGrapple(), 1);
	Player->Destroy();
	TestEqual(TEXT("Pawn destruction cleans persistent ASC"), CountGrapple(), 0);
	World->DestroyWorld(false);
	return true;
}
#endif
