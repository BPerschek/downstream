#pragma once
#include "DownstreamExpeditionState.h"
#include <cmath>
#include <vector>

namespace downstream
{
enum class StructureType { Storage, Workshop, Charger };
struct GridCell { std::int32_t X = 0; std::int32_t Y = 0; };
struct StructureRecord { StructureType Type = StructureType::Storage; GridCell Cell; };
struct SettlementRules
{
    std::int32_t StorageCost = 4, WorkshopCost = 6, ChargerCost = 4, HaulerCost = 8;
    std::int32_t StorageCapacityBonus = 8, HaulerCargoCapacity = 4;
    std::int32_t MinX = -4, MaxX = 4, MinY = -3, MaxY = 3;
};
class SettlementState
{
public:
    explicit SettlementState(SettlementRules rules = {}) : Rules(rules) {}
    const std::vector<StructureRecord>& GetStructures() const { return Structures; }
    std::int32_t GetStructureCost(StructureType type) const
    {
        switch (type)
        {
        case StructureType::Storage: return Rules.StorageCost;
        case StructureType::Workshop: return Rules.WorkshopCost;
        case StructureType::Charger: return Rules.ChargerCost;
        default: return 0;
        }
    }
    std::int32_t GetHaulerCost() const { return Rules.HaulerCost; }
    std::int32_t GetHaulerCargoCapacity() const { return std::max<std::int32_t>(0, Rules.HaulerCargoCapacity); }
    bool HasStructure(StructureType type) const
    {
        return std::any_of(Structures.begin(), Structures.end(), [type](const auto& record) { return record.Type == type; });
    }
    bool IsValidCell(GridCell cell) const
    {
        return cell.X >= Rules.MinX && cell.X <= Rules.MaxX && cell.Y >= Rules.MinY && cell.Y <= Rules.MaxY;
    }
    bool CanPlaceStructure(const ExpeditionState& bank, StructureType type, GridCell cell) const
    {
        return GetStructureCost(type) > 0 && bank.GetBankedSalvage() >= GetStructureCost(type)
            && Rules.StorageCapacityBonus >= 0 && IsValidCell(cell) && !HasStructure(type)
            && std::none_of(Structures.begin(), Structures.end(), [cell](const auto& record)
                { return record.Cell.X == cell.X && record.Cell.Y == cell.Y; });
    }
    bool PlaceStructure(ExpeditionState& bank, StructureType type, GridCell cell)
    {
        if (!CanPlaceStructure(bank, type, cell)) return false;
        auto candidate = bank;
        if (!candidate.SpendBanked(GetStructureCost(type))) return false;
        if (type == StructureType::Storage && !candidate.SetSettlementCapacityBonus(Rules.StorageCapacityBonus)) return false;
        Structures.push_back({type, cell});
        bank = candidate;
        return true;
    }
    bool CanConstructHauler(const ExpeditionState& bank) const
    {
        return !HaulerBuilt && Rules.HaulerCost > 0 && Rules.HaulerCargoCapacity > 0
            && bank.GetBankedSalvage() >= Rules.HaulerCost && HasStructure(StructureType::Storage)
            && HasStructure(StructureType::Workshop) && HasStructure(StructureType::Charger);
    }
    bool ConstructHauler(ExpeditionState& bank)
    {
        if (!CanConstructHauler(bank) || !bank.SpendBanked(Rules.HaulerCost)) return false;
        HaulerBuilt = true;
        Battery = 100.0;
        return true;
    }
    bool IsHaulerConstructed() const { return HaulerBuilt; }
    double GetHaulerBattery() const { return Battery; }
    std::int32_t GetHaulerCargo() const { return Cargo; }
    bool IsHaulingEnabled() const { return HaulingEnabled; }
    bool SetHaulingEnabled(bool enabled)
    {
        if (!HaulerBuilt) return false;
        HaulingEnabled = enabled;
        return true;
    }
    bool LoadHaulerCargo(std::int32_t amount)
    {
        if (!HaulerBuilt || amount <= 0 || amount > GetHaulerCargoCapacity() - Cargo) return false;
        Cargo += amount;
        return true;
    }
    bool UnloadHaulerCargo(ExpeditionState& bank)
    {
        if (!HaulerBuilt || Cargo <= 0 || !bank.AddBanked(Cargo)) return false;
        Cargo = 0;
        return true;
    }
    void UpdateHaulerBattery(double signedDelta)
    {
        if (!HaulerBuilt || !std::isfinite(signedDelta)) return;
        Battery = std::clamp(Battery + signedDelta, 0.0, 100.0);
    }
    bool LoadPersisted(const std::vector<StructureRecord>& records, bool built, double battery, std::int64_t cargo, bool enabled)
    {
        if (!std::isfinite(battery) || battery < 0.0 || battery > 100.0 || cargo < 0
            || cargo > GetHaulerCargoCapacity() || (!built && (cargo != 0 || enabled))) return false;
        std::vector<StructureRecord> validated;
        for (const auto& record : records)
        {
            if (GetStructureCost(record.Type) <= 0 || !IsValidCell(record.Cell)) return false;
            for (const auto& previous : validated)
                if (previous.Type == record.Type || (previous.Cell.X == record.Cell.X && previous.Cell.Y == record.Cell.Y)) return false;
            validated.push_back(record);
        }
        if (built && (validated.size() != 3 || Rules.HaulerCargoCapacity <= 0)) return false;
        Structures = std::move(validated);
        HaulerBuilt = built;
        Battery = battery;
        Cargo = static_cast<std::int32_t>(cargo);
        HaulingEnabled = enabled;
        return true;
    }
private:
    SettlementRules Rules;
    std::vector<StructureRecord> Structures;
    bool HaulerBuilt = false;
    double Battery = 100.0;
    std::int32_t Cargo = 0;
    bool HaulingEnabled = false;
};
}
