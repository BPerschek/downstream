#include "DownstreamExpeditionState.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace
{
int Failures = 0;
void Check(bool condition, const char* name)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << name << '\n';
        ++Failures;
    }
}
}

int main()
{
    // Catch accepting negative/zero quantities or allowing carry beyond capacity.
    downstream::ExpeditionState carry(12);
    Check(!carry.Collect(0) && !carry.Collect(-1), "invalid pickup quantities are rejected");
    Check(carry.Collect(11), "positive pickup is collected");
    Check(!carry.Collect(2) && carry.GetCarriedSalvage() == 11, "oversized pickup leaves carry untouched");
    Check(carry.Collect(1) && carry.GetCarriedSalvage() == 12, "last capacity slot is usable");
    Check(!carry.Collect(1), "full inventory rejects another pickup");

    // Catch extraction duplication, loss, or non-transactional overflow.
    Check(carry.BankCarried(), "extraction accepts carried salvage");
    Check(carry.GetCarriedSalvage() == 0 && carry.GetBankedSalvage() == 12, "extraction transfers all carried salvage");
    Check(!carry.BankCarried() && carry.GetBankedSalvage() == 12, "empty extraction cannot duplicate salvage");
    Check(carry.Collect(5) && carry.BankCarried() && carry.GetBankedSalvage() == 17, "later extraction accumulates stash");

    // Catch death erasing banked salvage or dropping the same carried salvage twice.
    Check(carry.Collect(7), "death fixture collects salvage");
    Check(carry.DropCarriedOnDeath() == 7, "death returns exact recovery bag quantity");
    Check(carry.GetCarriedSalvage() == 0 && carry.GetBankedSalvage() == 17, "death clears carry and preserves stash");
    Check(carry.DropCarriedOnDeath() == 0, "second death cannot duplicate a recovery bag");
    Check(carry.Collect(7), "recovery bag can be collected through ordinary capacity rules");

    // Catch invalid starter IDs, granting both nodes, or missing skill effects.
    downstream::ExpeditionState combat(12);
    Check(!combat.ChooseStarterSkill("unknown") && combat.GetSelectedSkillId().empty(), "unknown starter skill is rejected");
    Check(combat.ChooseStarterSkill("steady_aim"), "combat starter is selected");
    Check(std::abs(combat.GetRangedDamageMultiplier() - 1.15) < 0.000001, "steady aim grants fifteen percent ranged damage");
    Check(combat.GetInventoryCapacity() == 12, "steady aim leaves carrying capacity alone");
    Check(!combat.ChooseStarterSkill("salvage_handling"), "only one free starter node is granted");
    Check(combat.ChooseStarterSkill("steady_aim"), "same starter request is idempotent");
    downstream::ExpeditionState builder(12);
    Check(builder.ChooseStarterSkill("salvage_handling") && builder.Collect(16), "salvage handling grants four carrying slots");
    Check(!builder.Collect(1) && builder.GetRangedDamageMultiplier() == 1.0, "builder skill has bounded capacity and no combat bonus");

    // Catch unsafe save values, losing state on a failed load, or persisting field carry.
    downstream::ExpeditionState restored(12);
    Check(restored.LoadPersisted(1, 27, "salvage_handling"), "version one stash and skill restore");
    Check(restored.GetBankedSalvage() == 27 && restored.GetInventoryCapacity() == 16, "loaded save restores stash and selected node");
    Check(restored.Collect(3), "restore fixture has in-run carry");
    Check(!restored.LoadPersisted(2, 90, "") && !restored.LoadPersisted(1, -1, ""), "unsupported version and negative bank are rejected");
    Check(!restored.LoadPersisted(1, 20, "surprise_skill"), "unsupported saved skill is rejected");
    Check(restored.GetBankedSalvage() == 27 && restored.GetCarriedSalvage() == 3 && restored.GetSelectedSkillId() == "salvage_handling", "failed loads leave state unchanged");
    Check(restored.LoadPersisted(1, 8, "") && restored.GetCarriedSalvage() == 0, "successful load starts a fresh field run");

    const auto max = std::numeric_limits<std::int32_t>::max();
    downstream::ExpeditionState overflow(max);
    Check(overflow.ChooseStarterSkill("salvage_handling") && overflow.GetInventoryCapacity() == max, "skill capacity saturates without integer overflow");
    Check(overflow.LoadPersisted(1, static_cast<std::int64_t>(max) - 1, ""), "near-limit stash restores");
    Check(overflow.Collect(2) && !overflow.BankCarried(), "bank overflow is rejected");
    Check(overflow.GetCarriedSalvage() == 2 && overflow.GetBankedSalvage() == max - 1, "rejected overflow preserves carry and stash");
    Check(!overflow.LoadPersisted(1, static_cast<std::int64_t>(max) + 1, ""), "oversized saved quantity is rejected");
    downstream::ExpeditionState invalidCapacity(-12);
    Check(!invalidCapacity.Collect(1) && invalidCapacity.GetInventoryCapacity() == 0, "negative base capacity cannot create salvage");

    if (Failures != 0) return EXIT_FAILURE;
    std::cout << "PASS: expedition capacity, banking, death recovery, skills, save validation, and overflow\n";
    return EXIT_SUCCESS;
}
