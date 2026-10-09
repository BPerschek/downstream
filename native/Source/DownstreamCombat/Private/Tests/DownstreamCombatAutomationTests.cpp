#include "Misc/AutomationTest.h"
#include "DownstreamCombatRules.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamCooldownTest,
    "Downstream.Combat.CooldownBoundary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamCooldownTest::RunTest(const FString&)
{
    downstream::CombatCooldown Cooldown;
    TestTrue(TEXT("An initial attack is immediately available"), Cooldown.TryConsume(0.0, 0.22));
    TestFalse(TEXT("Holding fire cannot bypass its cooldown"), Cooldown.TryConsume(0.1, 0.22));
    TestFalse(TEXT("Still cooling down just before boundary"), Cooldown.TryConsume(0.219, 0.22));
    TestTrue(TEXT("Ready at exact boundary"), Cooldown.TryConsume(0.22, 0.22));
    TestFalse(TEXT("Repeated attack in same frame is rejected"), Cooldown.TryConsume(0.22, 0.22));
    TestFalse(TEXT("Invalid interval does not consume or grant attack"), Cooldown.TryConsume(1.0, -1.0));
    TestTrue(TEXT("Invalid input leaves prior cooldown unchanged"), Cooldown.TryConsume(1.0, 0.22));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamDamageRulesTest,
    "Downstream.Combat.FactionAndProtection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamDamageRulesTest::RunTest(const FString&)
{
    TestTrue(TEXT("Player projectile can damage living enemy"), downstream::CanDamage(true, false, false, false));
    TestTrue(TEXT("Enemy projectile can damage living player"), downstream::CanDamage(false, true, false, false));
    TestFalse(TEXT("Player projectiles never damage player faction"), downstream::CanDamage(true, true, false, false));
    TestFalse(TEXT("Enemy projectiles never damage other enemies"), downstream::CanDamage(false, false, false, false));
    TestFalse(TEXT("Dead actors cannot receive another death"), downstream::CanDamage(true, false, true, false));
    TestFalse(TEXT("Home and dodge protection block enemy damage"), downstream::CanDamage(false, true, false, true));
    return true;
}
#endif
