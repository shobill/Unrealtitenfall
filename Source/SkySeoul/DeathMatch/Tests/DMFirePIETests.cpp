#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "DeathMatch/Core/DeathMatchLog.h"

class FDMObserveFirePIE : public IAutomationLatentCommand
{
public:
    FDMObserveFirePIE(FAutomationTestBase* InTest, int32 InPreviousDiag) : Test(InTest), PreviousDiag(InPreviousDiag) {}
    virtual bool Update() override;
private:
    FAutomationTestBase* Test;
    bool bObservedShot = false;
    int32 MaxRobots = 0;
    int32 PreviousDiag;
};
bool FDMObserveFirePIE::Update()
{
    int32 Robots = 0, Resolved = 0;
    if (UWorld* World = GEditor->PlayWorld)
    {
        for (TActorIterator<ADeathMatchRobotCharacter> It(World); It; ++It)
        {
            ++Robots;
            if (auto* ASC = It->GetAbilitySystemComponent())
                for (const auto& Spec : ASC->GetActivatableAbilities())
                    if (const auto* Fire = Cast<UDMGameplayAbility_Fire>(Spec.GetPrimaryInstance()))
                        Resolved += Fire->GetLastShotResult().bShotExecuted ? 1 : 0;
        }
    }
    MaxRobots = FMath::Max(MaxRobots, Robots);
    bObservedShot |= Resolved > 0;
    if ((!GEditor->PlayWorld || GEditor->PlayWorld->GetTimeSeconds() < 25.) && FPlatformTime::Seconds() - StartTime < 90.) return false;
    Test->AddInfo(FString::Printf(TEXT("PIE real map: max robots=%d observed real shot=%d; inspect DM FireAttempt events for the complete history"), MaxRobots, bObservedShot));
    Test->TestTrue(TEXT("Actual map spawned AI robots"), MaxRobots > 0);
    Test->TestTrue(TEXT("PIE AI executed real shots"), bObservedShot);
    CVarDMCombatDiag->Set(PreviousDiag, ECVF_SetByCode);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMFirePIETest, "SkySeoul.DeathMatch.Combat.Fire.PIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMFirePIETest::RunTest(const FString& Parameters)
{
    // Existing project viewport configuration emits this unrelated error when PIE creates UI.
    AddExpectedError(TEXT("Using CommonUI without a CommonGameViewportClient"), EAutomationExpectedErrorFlags::Contains, 1);
    const int32 PreviousDiag = CVarDMCombatDiag.GetValueOnGameThread();
    CVarDMCombatDiag->Set(1, ECVF_SetByCode);
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/DeathMatch/Levels/Lv_DawnBreaker_WareHouse_Original"), false, true))
    {
        AddError(TEXT("Cannot load actual DeathMatch map"));
        CVarDMCombatDiag->Set(PreviousDiag, ECVF_SetByCode);
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FDMObserveFirePIE(this, PreviousDiag));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif


