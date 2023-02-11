/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#pragma once

#include "FHPos.hpp"

#include "LibraryObjectDef.hpp"
#include "AdventureStack.hpp"

#include <map>

#include "MapUtilExport.hpp"

namespace FreeHeroes {

enum class FHScoreAttr
{
    Invalid,

    Army,
    ArtStat,
    ArtSupport,
    Gold,
    Resource,
    ResourceGen,
    Experience,
    SpellOffensive,
    SpellCommon,
    SpellAll,
    Misc,
};
using FHScore = std::map<FHScoreAttr, int64_t>;

struct FHCommonObject {
    FHPos                m_pos{ g_invalidPos };
    int                  m_order = 0;
    Core::ObjectDefIndex m_defIndex;
    int64_t              m_guard = 0;
    FHScore              m_score;

    bool operator==(const FHCommonObject&) const noexcept = default;
};

struct FHCommonVisitable : public FHCommonObject {
    Core::LibraryMapVisitableConstPtr m_visitableId = nullptr;

    bool operator==(const FHCommonVisitable&) const noexcept = default;
};

struct FHPlayerControlledObject : public FHCommonObject {
    Core::LibraryPlayerConstPtr m_player = nullptr;

    bool operator==(const FHPlayerControlledObject&) const noexcept = default;
};

struct FHTown : public FHPlayerControlledObject {
    bool                         m_isMain{ false };
    Core::LibraryFactionConstPtr m_factionId = nullptr;
    bool                         m_hasFort{ false };
    uint32_t                     m_questIdentifier = 0;
    bool                         m_spellResearch{ false };
    bool                         m_hasCustomBuildings{ false };
    bool                         m_hasGarison{ false };

    std::vector<Core::LibraryBuildingConstPtr> m_buildings;

    std::vector<Core::AdventureStack> m_garison;

    struct RmgStack {
        int m_level = 0;
        int m_value = 0;

        bool operator==(const RmgStack&) const noexcept = default;
    };

    std::vector<RmgStack> m_garisonRmg;

    bool operator==(const FHTown&) const noexcept = default;
};

MAPUTIL_EXPORT std::ostream& operator<<(std::ostream& stream, const FreeHeroes::FHScore& score);

MAPUTIL_EXPORT FreeHeroes::FHScore operator+(const FreeHeroes::FHScore& l, const FreeHeroes::FHScore& r);
MAPUTIL_EXPORT FreeHeroes::FHScore operator-(const FreeHeroes::FHScore& l, const FreeHeroes::FHScore& r);

}
