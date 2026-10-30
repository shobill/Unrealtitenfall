#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DMCombatHealthEvaluation.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMCombatHealthEvaluationTest, "SkySeoul.DeathMatch.Combat.HealthEvaluation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMCombatHealthEvaluationTest::RunTest(const FString& Parameters)
{
    FDMCombatHealthSettings S;
    const auto Full=DMCombatHealth::Evaluate(100,100,S);
    const auto Half=DMCombatHealth::Evaluate(50,100,S);
    const auto Low=DMCombatHealth::Evaluate(10,100,S);
    TestEqual(TEXT("Full ratio"),Full.HealthRatio,1.f);
    TestEqual(TEXT("Full risk"),Full.HealthRisk,0.f);
    TestEqual(TEXT("Half risk"),Half.HealthRisk,.5f);
    TestEqual(TEXT("Low risk"),Low.HealthRisk,.9f);
    TestEqual(TEXT("Overheal clamped"),DMCombatHealth::Evaluate(200,100,S).HealthRatio,1.f);
    TestEqual(TEXT("Negative health clamped"),DMCombatHealth::Evaluate(-10,100,S).HealthRatio,0.f);
    TestEqual(TEXT("Zero HP does not implement death"),DMCombatHealth::Evaluate(0,100,S).HealthRisk,1.f);
    for (float Maximum : {0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        TestEqual(TEXT("Invalid max safely healthy"),DMCombatHealth::Evaluate(10,Maximum,S).HealthRatio,1.f);
    TestEqual(TEXT("Invalid current safely healthy"),DMCombatHealth::Evaluate(std::numeric_limits<float>::quiet_NaN(),100,S).HealthRatio,1.f);
    TestEqual(TEXT("Full health exposure cost"),DMCombatHealth::ExposureCost(Full,1,1),0.f);
    TestEqual(TEXT("Half health exposure cost"),DMCombatHealth::ExposureCost(Half,1,1),15.f);
    TestEqual(TEXT("Low health exposure cost"),DMCombatHealth::ExposureCost(Low,1,1),27.f);
    TestEqual(TEXT("Safe position has no blanket health cost"),DMCombatHealth::ExposureCost(Low,0,0),0.f);
    TestFalse(TEXT("Low health alone not emergency"),DMCombatHealth::Emergency(Low,true,.3f,0));
    TestFalse(TEXT("Remembered-only exposure not sensed emergency"),DMCombatHealth::Emergency(Low,true,.3f,0));
    TestTrue(TEXT("Low health and sensed exposure emergency"),DMCombatHealth::Emergency(Low,true,.3f,1));
    TestFalse(TEXT("Toggle respected"),DMCombatHealth::Emergency(Low,false,.3f,1));
    TestFalse(TEXT("Healthy exposure not low health emergency"),DMCombatHealth::Emergency(Full,true,.3f,1));
    TestEqual(TEXT("Full micro scale"),Full.MicroMoveScale,1.f);
    TestEqual(TEXT("Half micro scale"),Half.MicroMoveScale,.75f);
    TestTrue(TEXT("Low micro scale"),FMath::IsNearlyEqual(Low.MicroMoveScale,.55f));
    Swap(S.CautionHealthRatio,S.CriticalHealthRatio);
    TestEqual(TEXT("Reversed bands preserve preference"),DMCombatHealth::Evaluate(50,100,S).CoverReward,Half.CoverReward);
    S.CautionHealthRatio=S.CriticalHealthRatio=.3f;
    TestTrue(TEXT("Equal bands remain finite"),FMath::IsFinite(DMCombatHealth::Evaluate(10,100,S).AggressionPenalty));
    S.LowHealthMicroMoveScale=1;
    TestEqual(TEXT("Scale one preserves radius"),DMCombatHealth::Evaluate(10,100,S).MicroMoveScale,1.f);
    S.LowHealthMicroMoveScale=0;
    TestEqual(TEXT("Scale zero at full risk suppresses radius"),DMCombatHealth::Evaluate(0,100,S).MicroMoveScale,0.f);
    S=FDMCombatHealthSettings(); S.bEnableHealthEvaluation=false;
    auto Disabled=DMCombatHealth::Evaluate(10,100,S);
    TestEqual(TEXT("Disabled exposure"),DMCombatHealth::ExposureCost(Disabled,1,1),0.f);
    TestEqual(TEXT("Disabled micro"),Disabled.MicroMoveScale,1.f);
    TestEqual(TEXT("Disabled preferences"),Disabled.CoverReward+Disabled.RetreatReward+Disabled.AggressionPenalty,0.f);
    TestTrue(TEXT("Legacy emergency independent of disabled evaluation"),DMCombatHealth::Emergency(Disabled,true,.3f,1));
    S.bEnableHealthEvaluation=true; S.HealthEvaluationWeight=0;
    Disabled=DMCombatHealth::Evaluate(10,100,S);
    TestEqual(TEXT("Zero weight exposure"),DMCombatHealth::ExposureCost(Disabled,1,1),0.f);
    TestEqual(TEXT("Zero weight micro"),Disabled.MicroMoveScale,1.f);
    S.HealthEvaluationWeight=-1;
    TestEqual(TEXT("Negative weight disabled"),DMCombatHealth::Evaluate(10,100,S).AggressionPenalty,0.f);
    S.HealthEvaluationWeight=1; S.LowHealthExposurePenalty=S.LowHealthPathExposurePenalty=0;
    TestEqual(TEXT("Zero costs disable individual contributions"),DMCombatHealth::ExposureCost(DMCombatHealth::Evaluate(10,100,S),1,1),0.f);
    return true;
}
#endif
