#pragma once

#include <cstdint>
#include <algorithm>
#include <limits>
#include <string>

namespace downstream
{
// Authoritative M1 inventory/stash rules. This header has no Unreal dependency.
// Runtime pickup ownership and durable file commits belong to the engine adapter.
class ExpeditionState
{
public:
    explicit ExpeditionState(std::int32_t capacity = 12) : BaseCapacity(std::max<std::int32_t>(0, capacity)) {}
    std::int32_t GetCarriedSalvage() const { return Carried; }
    std::int32_t GetBankedSalvage() const { return Banked; }
    std::int32_t GetInventoryCapacity() const
    {
        const std::int32_t bonus = Skill == "salvage_handling" ? 4 : 0;
        return BaseCapacity > std::numeric_limits<std::int32_t>::max() - bonus
            ? std::numeric_limits<std::int32_t>::max() : BaseCapacity + bonus;
    }
    double GetRangedDamageMultiplier() const { return Skill == "steady_aim" ? 1.15 : 1.0; }
    const std::string& GetSelectedSkillId() const { return Skill; }
    bool CanCollect(std::int32_t amount) const
    {
        return amount > 0 && amount <= GetInventoryCapacity() - Carried;
    }
    bool Collect(std::int32_t amount)
    {
        if (!CanCollect(amount)) return false;
        Carried += amount;
        return true;
    }
    bool BankCarried()
    {
        if (Carried <= 0 || Banked > std::numeric_limits<std::int32_t>::max() - Carried) return false;
        Banked += Carried;
        Carried = 0;
        return true;
    }
    std::int32_t DropCarriedOnDeath()
    {
        const auto dropped = Carried;
        Carried = 0;
        return dropped;
    }
    bool ChooseStarterSkill(const std::string& skill)
    {
        if (!IsKnownSkill(skill) || skill.empty()) return false;
        if (!Skill.empty()) return Skill == skill;
        Skill = skill;
        return true;
    }
    bool LoadPersisted(std::int64_t version, std::int64_t banked, const std::string& skill)
    {
        if (version != 1 || banked < 0 || banked > std::numeric_limits<std::int32_t>::max() || !IsKnownSkill(skill)) return false;
        Banked = static_cast<std::int32_t>(banked);
        Skill = skill;
        Carried = 0;
        return true;
    }

private:
    static bool IsKnownSkill(const std::string& skill)
    {
        return skill.empty() || skill == "steady_aim" || skill == "salvage_handling";
    }
    std::int32_t BaseCapacity = 12;
    std::int32_t Carried = 0;
    std::int32_t Banked = 0;
    std::string Skill;
};
}
