#include "DownstreamSettlementState.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
int Failures = 0;
void Check(bool condition, const char* name)
{
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++Failures; }
}
}

int main()
{
    using downstream::StructureType;
    using downstream::SettlementState;
    using downstream::StructureRecord;
    downstream::ExpeditionState bank(12);
    Check(bank.LoadPersisted(1, 30, "salvage_handling"), "bank fixture restores");
    Check(!bank.SpendBanked(0) && !bank.SpendBanked(-2), "invalid spending cannot create salvage");
    Check(!bank.SpendBanked(31) && bank.GetBankedSalvage() == 30, "unaffordable spending preserves bank");
    Check(bank.SpendBanked(1) && bank.GetBankedSalvage() == 29, "spending removes exact cost");
    SettlementState settlement;
    Check(!settlement.CanConstructHauler(bank), "robot requires all three structures");
    Check(!settlement.PlaceStructure(bank, StructureType::Storage, {-5, 0}), "left out-of-grid placement rejects");
    Check(!settlement.PlaceStructure(bank, StructureType::Storage, {0, 4}), "top out-of-grid placement rejects");
    Check(!settlement.PlaceStructure(bank, static_cast<StructureType>(99), {0, 0}), "unknown structure rejects");
    Check(bank.GetBankedSalvage() == 29 && settlement.GetStructures().empty(), "failed placements preserve state and bank");
    Check(settlement.PlaceStructure(bank, StructureType::Storage, {-4, -3}), "inclusive boundary placement succeeds");
    Check(bank.GetBankedSalvage() == 25 && bank.GetInventoryCapacity() == 24, "storage spends four and stacks eight slots with skill");
    Check(!settlement.PlaceStructure(bank, StructureType::Storage, {0, 0}), "duplicate structure type rejects");
    Check(!settlement.PlaceStructure(bank, StructureType::Workshop, {-4, -3}), "occupied cell rejects");
    Check(bank.GetBankedSalvage() == 25 && settlement.GetStructures().size() == 1, "duplicate and overlap preserve economy");
    Check(settlement.PlaceStructure(bank, StructureType::Workshop, {4, 3}), "workshop accepts other boundary");
    Check(settlement.PlaceStructure(bank, StructureType::Charger, {0, 0}), "charger is placed");
    Check(bank.GetBankedSalvage() == 15 && settlement.CanConstructHauler(bank), "all prerequisites unlock robot");
    Check(settlement.ConstructHauler(bank) && bank.GetBankedSalvage() == 7, "robot spends eight once");
    Check(!settlement.ConstructHauler(bank) && bank.GetBankedSalvage() == 7, "robot cannot duplicate");
    Check(settlement.IsHaulerConstructed() && settlement.GetHaulerBattery() == 100.0, "new robot starts fully charged");
    Check(settlement.SetHaulingEnabled(true) && settlement.IsHaulingEnabled(), "hauling preference enables built robot");
    Check(!settlement.LoadHaulerCargo(0) && !settlement.LoadHaulerCargo(-1), "invalid cargo rejects");
    Check(settlement.LoadHaulerCargo(3), "whole cargo loads");
    Check(!settlement.LoadHaulerCargo(2) && settlement.GetHaulerCargo() == 3, "oversized cargo preserves inventory");
    Check(settlement.LoadHaulerCargo(1) && !settlement.LoadHaulerCargo(1), "last cargo slot fills exactly");
    Check(settlement.UnloadHaulerCargo(bank) && settlement.GetHaulerCargo() == 0 && bank.GetBankedSalvage() == 11, "cargo unload transfers exact saved value");
    Check(!settlement.UnloadHaulerCargo(bank) && bank.GetBankedSalvage() == 11, "empty unload cannot duplicate salvage");
    Check(settlement.SetHaulingEnabled(false) && !settlement.IsHaulingEnabled(), "disabled hauling preference is retained");
    settlement.UpdateHaulerBattery(-108.0);
    Check(settlement.GetHaulerBattery() == 0.0, "battery drain clamps at zero");
    settlement.UpdateHaulerBattery(150.0);
    Check(settlement.GetHaulerBattery() == 100.0, "charging clamps at one hundred");
    settlement.UpdateHaulerBattery(std::numeric_limits<double>::quiet_NaN());
    settlement.UpdateHaulerBattery(std::numeric_limits<double>::infinity());
    Check(settlement.GetHaulerBattery() == 100.0, "nonfinite battery changes reject");

    downstream::ExpeditionState poor;
    Check(poor.LoadPersisted(1, 3, ""), "poor fixture restores");
    SettlementState poorSettlement;
    Check(!poorSettlement.PlaceStructure(poor, StructureType::Storage, {0, 0}) && poor.GetBankedSalvage() == 3, "unaffordable structure leaves bank unchanged");
    Check(!poorSettlement.LoadHaulerCargo(1) && !poorSettlement.SetHaulingEnabled(true), "unbuilt robot cannot work");
    poorSettlement.UpdateHaulerBattery(-20.0);
    Check(poorSettlement.GetHaulerBattery() == 100.0, "unbuilt robot battery does not tick");

    const std::vector<StructureRecord> all = {{StructureType::Storage, {-1, 0}}, {StructureType::Workshop, {0, 0}}, {StructureType::Charger, {1, 0}}};
    SettlementState restored;
    Check(restored.LoadPersisted(all, true, 26.5, 4, true), "complete saved settlement restores");
    Check(restored.GetStructures().size() == 3 && restored.GetHaulerCargo() == 4 && restored.GetHaulerBattery() == 26.5 && restored.IsHaulingEnabled(), "restored robot cargo battery and preference survive");
    auto duplicateType = all; duplicateType[1].Type = StructureType::Storage;
    auto duplicateCell = all; duplicateCell[1].Cell = duplicateCell[0].Cell;
    auto invalidCell = all; invalidCell[0].Cell.X = 100;
    auto invalidType = all; invalidType[0].Type = static_cast<StructureType>(99);
    Check(!restored.LoadPersisted(duplicateType, true, 0.0, 0, false), "duplicate saved type rejects");
    Check(!restored.LoadPersisted(duplicateCell, true, 0.0, 0, false), "duplicate saved cell rejects");
    Check(!restored.LoadPersisted(invalidCell, true, 0.0, 0, false), "out-of-grid save rejects");
    Check(!restored.LoadPersisted(invalidType, true, 0.0, 0, false), "unknown saved type rejects");
    Check(!restored.LoadPersisted({}, true, 0.0, 0, false), "saved robot without structures rejects");
    Check(!restored.LoadPersisted({}, false, 100.0, 1, false), "cargo without robot rejects");
    Check(!restored.LoadPersisted({}, false, 100.0, 0, true), "job without robot rejects");
    Check(!restored.LoadPersisted(all, true, -1.0, 0, false) && !restored.LoadPersisted(all, true, 101.0, 0, false), "saved battery outside range rejects");
    Check(!restored.LoadPersisted(all, true, std::numeric_limits<double>::quiet_NaN(), 0, false), "nonfinite saved battery rejects");
    Check(!restored.LoadPersisted(all, true, 30.0, -1, false) && !restored.LoadPersisted(all, true, 30.0, 5, false), "invalid saved cargo rejects");
    Check(restored.GetHaulerCargo() == 4 && restored.GetHaulerBattery() == 26.5 && restored.IsHaulingEnabled(), "failed saves preserve prior robot state");
    downstream::ExpeditionState fullBank;
    const auto max = std::numeric_limits<std::int32_t>::max();
    Check(fullBank.LoadPersisted(1, max, ""), "full bank restores");
    Check(!restored.UnloadHaulerCargo(fullBank) && restored.GetHaulerCargo() == 4 && fullBank.GetBankedSalvage() == max, "unload overflow preserves both values");
    Check(restored.LoadPersisted({}, false, 100.0, 0, false), "empty legacy settlement restores");
    Check(restored.GetStructures().empty() && !restored.IsHaulerConstructed(), "legacy restore clears settlement");
    downstream::ExpeditionState huge(max);
    Check(huge.SetSettlementCapacityBonus(8) && huge.GetInventoryCapacity() == max, "storage capacity saturates safely");
    Check(!huge.SetSettlementCapacityBonus(-1) && huge.GetInventoryCapacity() == max, "negative capacity bonus rejects");
    downstream::ExpeditionState legacyCapacity;
    Check(legacyCapacity.SetSettlementCapacityBonus(8) && legacyCapacity.ChooseStarterSkill("salvage_handling"), "capacity restore fixture has both bonuses");
    Check(legacyCapacity.LoadPersisted(1, 9, "") && legacyCapacity.GetInventoryCapacity() == 12, "legacy expedition restore clears unsaved storage bonus");
    downstream::SettlementRules invalidRules;
    invalidRules.StorageCost = -1;
    SettlementState invalidPricing(invalidRules);
    Check(!invalidPricing.PlaceStructure(fullBank, StructureType::Storage, {0, 0}) && fullBank.GetBankedSalvage() == max, "invalid price cannot create salvage");
    if (Failures != 0) return EXIT_FAILURE;
    std::cout << "PASS: settlement spending, placement, prerequisites, cargo, battery, restore validation, and overflow\n";
    return EXIT_SUCCESS;
}
