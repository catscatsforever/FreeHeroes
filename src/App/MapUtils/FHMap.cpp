/*
 * Copyright (C) 2022 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */

#include "FHMap.hpp"

#include "IRandomGenerator.hpp"

#include "Reflection/PropertyTreeReader.hpp"
#include "Reflection/PropertyTreeWriter.hpp"

#include "AdventureReflection.hpp"

namespace FreeHeroes {

namespace Core::Reflection {

// clang-format off
template<>
inline constexpr const auto EnumTraits::s_valueMapping<FHPlayerId> = EnumTraits::make(
    FHPlayerId::Invalid,
    "Invalid"  , FHPlayerId::Invalid,
    "None"     , FHPlayerId::None,
    "Red"      , FHPlayerId::Red,
    "Blue"     , FHPlayerId::Blue,
    "Tan"      , FHPlayerId::Tan,
    "Green"    , FHPlayerId::Green,
    "Orange"   , FHPlayerId::Orange,
    "Purple"   , FHPlayerId::Purple,
    "Teal"     , FHPlayerId::Teal,
    "Pink"     , FHPlayerId::Pink
    );
// clang-format on

// clang-format off
template<>
inline constexpr const auto EnumTraits::s_valueMapping<Core::GameVersion> = EnumTraits::make(
    Core::GameVersion::Invalid,
    "SOD"  , Core::GameVersion::SOD,
    "HOTA" , Core::GameVersion::HOTA
    );
// clang-format on

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHPos>{
    Field("x", &FHPos::m_x),
    Field("y", &FHPos::m_y),
    Field("z", &FHPos::m_z),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHPlayer>{
    Field("ai", &FHPlayer::m_aiPossible),
    Field("human", &FHPlayer::m_humanPossible),
    Field("factions", &FHPlayer::m_startingFactions),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHHeroData>{
    Field("hasExp", &FHHeroData::m_hasExp),
    Field("hasSecSkills", &FHHeroData::m_hasSecSkills),
    Field("hasPrimSkills", &FHHeroData::m_hasPrimSkills),
    Field("hasCustomBio", &FHHeroData::m_hasCustomBio),
    Field("hasSpells", &FHHeroData::m_hasSpells),
    Field("army", &FHHeroData::m_army),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHHero>{
    Field("pos", &FHHero::m_pos),
    Field("player", &FHHero::m_player),
    Field("main", &FHHero::m_isMain),
    Field("id", &FHHero::m_id),
    Field("questId", &FHHero::m_questIdentifier),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHTown>{
    Field("pos", &FHTown::m_pos),
    Field("player", &FHTown::m_player),
    Field("main", &FHTown::m_isMain),
    Field("faction", &FHTown::m_faction),
    Field("hasFort", &FHTown::m_hasFort),
    Field("questId", &FHTown::m_questIdentifier),
    Field("spellResearch", &FHTown::m_spellResearch),
    Field("defFile", &FHTown::m_defFile),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHZone::Rect>{
    Field("pos", &FHZone::Rect::m_pos),
    Field("w", &FHZone::Rect::m_width),
    Field("h", &FHZone::Rect::m_height),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHZone>{
    Field("id", &FHZone::m_id),
    Field("terrain", &FHZone::m_terrain),
    Field("tiles", &FHZone::m_tiles),
    Field("tilesVariants", &FHZone::m_tilesVariants),
    Field("rect", &FHZone::m_rect),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHResource>{
    Field("pos", &FHResource::m_pos),
    Field("amount", &FHResource::m_amount),
    Field("resource", &FHResource::m_resource),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHMap::Objects>{
    Field("resources", &FHMap::Objects::m_resources),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHTileMap>{
    Field("width", &FHTileMap::m_width),
    Field("height", &FHTileMap::m_height),
    Field("depth", &FHTileMap::m_depth),
};

template<>
inline constexpr const std::tuple MetaInfo::s_fields<FHMap>{
    Field("version", &FHMap::m_version),
    Field("seed", &FHMap::m_seed),
    Field("tileMap", &FHMap::m_tileMap),
    Field("name", &FHMap::m_name),
    Field("descr", &FHMap::m_descr),
    Field("difficulty", &FHMap::m_difficulty),
    Field("players", &FHMap::m_players),
    Field("wanderingHeroes", &FHMap::m_wanderingHeroes),
    Field("towns", &FHMap::m_towns),
    Field("zones", &FHMap::m_zones),
    Field("objects", &FHMap::m_objects),
    Field("defaultTerrain", &FHMap::m_defaultTerrain),
    Field("disabledHeroes", &FHMap::m_disabledHeroes),
    Field("disabledArtifacts", &FHMap::m_disabledArtifacts),
    Field("disabledSpells", &FHMap::m_disabledSpells),
    Field("disabledSkills", &FHMap::m_disabledSkills),
    Field("customHeroes", &FHMap::m_customHeroes),
};

template<>
inline constexpr const bool MetaInfo::s_isStringMap<FHMap::PlayersMap>{ true };

}

void FHMap::toJson(PropertyTree& data) const
{
    Core::Reflection::PropertyTreeWriter writer;
    writer.valueToJson(*this, data);
}

void FHMap::fromJson(const PropertyTree& data, const Core::IGameDatabase* database)
{
    Core::Reflection::PropertyTreeReader reader(database);
    *this = {};
    reader.jsonToValue(data, *this);
}

void FHZone::placeOnMap(FHTileMap& map) const
{
    if (m_rect.has_value()) {
        auto& rect = m_rect.value();
        for (uint32_t x = 0; x < rect.m_width; ++x) {
            for (uint32_t y = 0; y < rect.m_height; ++y) {
                map.get(FHPos{ .m_x = x + rect.m_pos.m_x,
                               .m_y = y + rect.m_pos.m_y,
                               .m_z = rect.m_pos.m_z })
                    .m_terrain
                    = m_terrain;
            }
        }
        return;
    }
    if (!m_tiles.empty() && m_tilesVariants.size() == m_tiles.size()) {
        for (size_t i = 0; i < m_tiles.size(); ++i) {
            auto& tile     = map.get(m_tiles[i]);
            tile.m_terrain = m_terrain;
            tile.m_view    = m_tilesVariants[i];
        }
        return;
    }
    if (!m_tiles.empty()) {
        for (auto& pos : m_tiles) {
            map.get(pos).m_terrain = m_terrain;
        }
    }
}

void FHTileMap::correctTerrainTypes(Core::LibraryTerrainConstPtr dirtTerrain,
                                    Core::LibraryTerrainConstPtr sandTerrain,
                                    Core::LibraryTerrainConstPtr waterTerrain)
{
    // true = pattern found
    auto correctTile = [this, sandTerrain, waterTerrain](const FHPos& pos, bool flipHor, bool flipVert, int order) -> bool {
        /* TL  T  TR
         *  L  X   R
         * BL  B  BR
         */
        const auto& TL = getNeighbour(pos, flipHor ? +1 : -1, flipVert ? +1 : -1);
        const auto& T  = getNeighbour(pos, flipHor ? +0 : +0, flipVert ? +1 : -1);
        const auto& TR = getNeighbour(pos, flipHor ? -1 : +1, flipVert ? +1 : -1);
        const auto& L  = getNeighbour(pos, flipHor ? +1 : -1, flipVert ? +0 : +0);
        auto&       X  = get(pos);
        const auto& R  = getNeighbour(pos, flipHor ? -1 : +1, flipVert ? +0 : +0);
        const auto& BL = getNeighbour(pos, flipHor ? +1 : -1, flipVert ? -1 : +1);
        const auto& B  = getNeighbour(pos, flipHor ? +0 : +0, flipVert ? -1 : +1);
        const auto& BR = getNeighbour(pos, flipHor ? -1 : +1, flipVert ? -1 : +1);

        const bool waterX = X.m_terrain == waterTerrain;
        const auto dR     = X.m_terrain != R.m_terrain;
        const auto dL     = X.m_terrain != L.m_terrain;
        const auto dT     = X.m_terrain != T.m_terrain;
        const auto dB     = X.m_terrain != B.m_terrain;

        //const auto dTL = X.m_terrain != TL.m_terrain;
        const auto dTR = X.m_terrain != TR.m_terrain;
        const auto dBL = X.m_terrain != BL.m_terrain;
        const auto dBR = X.m_terrain != BR.m_terrain;

        const auto sandR  = R.m_terrain == sandTerrain || (!waterX && R.m_terrain == waterTerrain);
        const auto sandL  = L.m_terrain == sandTerrain || (!waterX && L.m_terrain == waterTerrain);
        const auto sandT  = T.m_terrain == sandTerrain || (!waterX && T.m_terrain == waterTerrain);
        const auto sandB  = B.m_terrain == sandTerrain || (!waterX && B.m_terrain == waterTerrain);
        const auto sandBR = BR.m_terrain == sandTerrain || (!waterX && BR.m_terrain == waterTerrain);

        if (!waterX) {
            X.m_coastal = false
                          || R.m_terrain == waterTerrain
                          || L.m_terrain == waterTerrain
                          || T.m_terrain == waterTerrain
                          || B.m_terrain == waterTerrain
                          || BR.m_terrain == waterTerrain
                          || TR.m_terrain == waterTerrain
                          || TL.m_terrain == waterTerrain
                          || BL.m_terrain == waterTerrain;
        }

        uint8_t diff = 3;
        if (false) {
        } else if (order == 0 && !waterX) {
            // 40 - D\D
            // 41 - D\S
            // 42 - S\S
            // 43 - right - D, BL - S
            // 44 - bottom - D, TR - S
            // 45 - TR - D, BR - S
            // 46 - BR - S, BL - D
            // 47 - R - S, B - D
            // 48 - B - S, R - D
            if (false) {
            } else if (sandBR && dB && !sandB) {
                X.m_viewMin = 46;
                diff        = 0;
            } else if (sandBR && dR && !sandR) {
                X.m_viewMin = 45;
                diff        = 0;
            } else if (sandR && dB && !sandB) {
                X.m_viewMin = 47;
                diff        = 0;
            } else if (sandB && dR && !sandR) {
                X.m_viewMin = 48;
                diff        = 0;
            }

            else {
                return false;
            }
        } else if (order == 1 && dL && dT) {
            X.m_viewMin = 0 + 20 * (sandL);
            if (!dTR || !dBL) {
                X.m_viewMin = 16 + 20 * (sandL);
                diff        = 1;
            }
        } else if (order == 2 && dL) {
            X.m_viewMin = 4 + 20 * (sandL);
        } else if (order == 2 && dT) {
            X.m_viewMin = 8 + 20 * (sandT);
        } else if (order == 3 && dBR) {
            X.m_viewMin = 12 + 20 * (sandBR);

            const auto& B2  = getNeighbour(pos, flipHor ? +0 : +0, flipVert ? -2 : +2);
            const auto& R2  = getNeighbour(pos, flipHor ? -2 : +2, flipVert ? +0 : +0);
            const auto  dB2 = X.m_terrain != B2.m_terrain;
            const auto  dR2 = X.m_terrain != R2.m_terrain;
            if (dB2 || dR2) {
                X.m_viewMin = 18 + 20 * (sandBR);
                diff        = 1;
            }
        } else {
            return false;
        }
        X.m_flipHor  = flipHor;
        X.m_flipVert = flipVert;
        X.m_viewMax  = X.m_viewMin + diff;
        return true;
    };

    for (uint8_t z = 0; z < m_depth; ++z) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                const FHPos pos{ x, y, z };
                auto&       X = get(pos);
                if (X.m_terrain == sandTerrain || X.m_terrain == dirtTerrain)
                    continue;

                const bool tileCorrected = [&correctTile, &pos]() {
                    for (int order = 0; order <= 3; ++order) {
                        for (int flipHor = 0; flipHor <= 1; ++flipHor) {
                            for (int flipVert = 0; flipVert <= 1; ++flipVert) {
                                if (correctTile(pos, flipHor, flipVert, order))
                                    return true;
                            }
                        }
                    }
                    return false;
                }();
                if (tileCorrected)
                    continue;

                const uint8_t offset     = X.m_terrain->presentationParams.centerTilesOffset;
                const auto    centerSize = X.m_terrain->presentationParams.centerTiles.size();
                X.m_viewMin              = offset;
                X.m_viewMax              = offset + (centerSize == 0 ? 0 : centerSize - 1);
            }
        }
    }
}

void FHTileMap::rngTiles(Core::IRandomGenerator* rng)
{
    auto rngView = [&rng](uint8_t min, uint8_t max) -> uint8_t {
        if (min == max)
            return min;
        uint8_t diff   = max - min;
        uint8_t result = rng->genSmall(diff);
        if (result >= 20)
            result = rng->genSmall(diff);
        return min + result;
    };

    for (uint8_t z = 0; z < m_depth; ++z) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                auto& X = get(x, y, z);
                //if (X.m_view != 0xff)
                //    continue;
                // todo: alternative. what's better?
                if (X.m_view >= X.m_viewMin && X.m_view <= X.m_viewMax)
                    continue;

                X.m_view = rngView(X.m_viewMin, X.m_viewMax);
            }
        }
    }
}

}
