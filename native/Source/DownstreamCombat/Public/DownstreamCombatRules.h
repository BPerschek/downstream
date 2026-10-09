#pragma once

#include <cmath>

namespace downstream
{
// Shared by native attacks; failed attempts never extend their next-ready time.
class CombatCooldown
{
public:
    bool TryConsume(double Now, double Interval)
    {
        if (!std::isfinite(Now) || !std::isfinite(Interval) || Interval <= 0.0 || Now < NextReady)
            return false;
        NextReady = Now + Interval;
        return true;
    }
    double SecondsRemaining(double Now) const { return Now < NextReady ? NextReady - Now : 0.0; }

private:
    double NextReady = 0.0;
};

inline bool CanDamage(bool bSourcePlayer, bool bTargetPlayer, bool bTargetDead, bool bTargetProtected)
{
    return bSourcePlayer != bTargetPlayer && !bTargetDead && !bTargetProtected;
}
}
