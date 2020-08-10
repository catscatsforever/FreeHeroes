/*
 * Copyright (C) 2020 Smirnov Valdimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#pragma once

#include "BattleFieldPosition.hpp"
#include "BattleStack.hpp"

namespace FreeHeroes::Core {

struct BattlePlanMoveParams {
    BattlePositionExtended m_movePos;

    BattlePositionExtended m_moveFrom; // debug.

    bool m_calculateUnlimitedPath = false; // used by AI.
    bool m_noMoveCalculation = false; // used by AI.
    void clear() noexcept { m_movePos = {}; }
    bool isActive() const noexcept { return !m_movePos.isEmpty(); }
};

struct BattlePlanAttackParams {
    enum class Alteration { None, ForceMelee, FreeAttack };
    BattlePositionExtended m_attackTargetPos;
    BattlePositionExtended::Side m_attackSide = BattlePositionExtended::Side::Right;
    BattleAttackDirection m_attackDirection = BattleAttackDirection::None;
    Alteration m_alteration = Alteration::None;

    void clear() noexcept { m_attackTargetPos = {}; }
    bool isActive() const noexcept { return !m_attackTargetPos.isEmpty(); }
};

struct BattlePlanMove {
    enum class Attack { No, Melee, Ranged };

    bool m_isValid = false;
    Attack m_attackMode = Attack::No;
    BattlePositionExtended m_attackTarget;
    BattlePositionExtended m_moveFrom;
    BattlePositionExtended m_moveTo;
    BattlePositionPath m_walkPath;
    BattleAttackDirection m_attackDirection = BattleAttackDirection::None;
    int64_t m_rangedAttackDenominator = 1; // 1, 2, 4

    BattleStackConstPtr m_attacker = nullptr;
    BattleStackConstPtr m_defender = nullptr;

    DamageEstimate m_mainDamage;
    DamageEstimate m_retaliationDamage;

    struct Target{
        BattleStackConstPtr unit = nullptr;
        DamageEstimate damage;
    };
    std::vector<Target> m_extraAffectedTargets;

    void clear() { *this = BattlePlanMove{}; }
    bool isValid() const noexcept { return m_isValid; }
};


}
