/*
 * Copyright (C) 2022 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "FH2H3M.hpp"

#include "FHMap.hpp"
#include "H3MMap.hpp"

#include "IGameDatabase.hpp"
#include "IRandomGenerator.hpp"

#include "LibraryArtifact.hpp"
#include "LibraryDwelling.hpp"
#include "LibraryFaction.hpp"
#include "LibraryHero.hpp"
#include "LibraryMapBank.hpp"
#include "LibraryMapObstacle.hpp"
#include "LibraryMapVisitable.hpp"
#include "LibraryObjectDef.hpp"
#include "LibrarySecondarySkill.hpp"
#include "LibraryTerrain.hpp"

#define assume(cond) \
    if (!(cond)) \
        throw std::runtime_error(#cond " - violated, invalid FH map provided.");

namespace FreeHeroes {

namespace {

H3Pos int3fromPos(FHPos pos, int xoffset = 0)
{
    return { (uint8_t) (pos.m_x + xoffset), (uint8_t) pos.m_y, (uint8_t) pos.m_z };
}

std::vector<uint8_t> reverseArray(std::vector<uint8_t> arr)
{
    std::reverse(arr.begin(), arr.end());
    return arr;
}

class ObjectTemplateCache {
public:
    ObjectTemplateCache(const Core::IGameDatabase* database)
        : m_database(database)
    {}

    uint32_t add(ObjectTemplate def)
    {
        auto index = static_cast<uint32_t>(m_objectDefs.size());
        m_objectDefs.push_back(std::move(def));
        return index;
    }

    uint32_t add(Core::LibraryObjectDefConstPtr record)
    {
        assert(record);
        if (m_index.contains(record))
            return m_index.at(record);
        ObjectTemplate res{
            .m_animationFile = record->defFile,
            .m_blockMask     = reverseArray(record->blockMap),
            .m_visitMask     = reverseArray(record->visitMap),
            .m_terrainsHard  = reverseArray(record->terrainsHard),
            .m_terrainsSoft  = reverseArray(record->terrainsSoft),

            .m_id           = static_cast<uint32_t>(record->objId),
            .m_subid        = static_cast<uint32_t>(record->subId),
            .m_type         = static_cast<ObjectTemplate::Type>(record->type),
            .m_drawPriority = static_cast<uint8_t>(record->priority),
        };
        if (res.m_animationFile.find('.') == std::string::npos)
            res.m_animationFile += ".def";
        const size_t terrainsCount = m_database->terrains()->legacyOrderedIds().size();
        res.m_terrainsHard.resize(terrainsCount);
        res.m_terrainsSoft.resize(terrainsCount);

        const auto index = add(res);
        m_index[record]  = index;
        return index;
    }

    uint32_t addId(std::string defId)
    {
        auto* defsContainer = m_database->objectDefs();
        if (defId.ends_with(".def"))
            defId = defId.substr(0, defId.size() - 4);
        std::transform(defId.begin(), defId.end(), defId.begin(), [](unsigned char c) { return std::tolower(c); });
        return add(defsContainer->find(defId));
    }

    uint32_t addHero(Core::LibraryHeroConstPtr hero)
    {
        std::string id = hero->getAdventureSprite() + "e";
        return addId(std::move(id));
    }

    std::map<Core::LibraryObjectDefConstPtr, uint32_t> m_index;
    std::vector<ObjectTemplate>                        m_objectDefs;
    const Core::IGameDatabase* const                   m_database;
};

}

void convertFH2H3M(const FHMap& src, H3Map& dest, const Core::IGameDatabase* database, Core::IRandomGenerator* rng)
{
    dest              = {};
    dest.m_mapName    = src.m_name;
    dest.m_mapDescr   = src.m_descr;
    dest.m_difficulty = src.m_difficulty;
    auto tileMap      = src.m_tileMap;
    tileMap.updateSize();
    assume(tileMap.m_width == tileMap.m_height && tileMap.m_width > 0);

    dest.m_format                 = src.m_version == Core::GameVersion::SOD ? MapFormat::SOD : MapFormat::HOTA3;
    dest.m_anyPlayers             = true;
    dest.m_tiles.m_size           = tileMap.m_width;
    dest.m_tiles.m_hasUnderground = tileMap.m_depth > 1;
    dest.m_tiles.updateSize();
    dest.prepareArrays();

    dest.m_hotaVer.m_allowSpecialWeeks = src.m_config.m_allowSpecialWeeks;
    dest.m_hotaVer.m_roundLimit        = src.m_config.m_hasRoundLimit ? src.m_config.m_roundLimit : 0xffffffffU;

    auto fillZoneTerrain = [&tileMap](const FHZone& zone) {
        if (!zone.m_terrainId)
            return;

        zone.placeOnMap(tileMap);
    };

    fillZoneTerrain(FHZone{ .m_terrainId = src.m_defaultTerrain, .m_rect{ FHZone::Rect{ .m_pos{ 0, 0, 0 }, .m_width = tileMap.m_width, .m_height = tileMap.m_height } } });
    for (auto& zone : src.m_zones)
        fillZoneTerrain(zone);
    for (auto& river : src.m_rivers)
        river.placeOnMap(tileMap);
    for (auto& road : src.m_roads)
        road.placeOnMap(tileMap);

    const auto*  dirtTerrain   = database->terrains()->find(std::string(Core::LibraryTerrain::s_terrainDirt));
    const auto*  sandTerrain   = database->terrains()->find(std::string(Core::LibraryTerrain::s_terrainSand));
    const auto*  waterTerrain  = database->terrains()->find(std::string(Core::LibraryTerrain::s_terrainWater));
    const size_t terrainsCount = database->terrains()->legacyOrderedIds().size();

    tileMap.correctTerrainTypes(dirtTerrain, sandTerrain, waterTerrain);
    tileMap.correctRoads();
    tileMap.correctRivers();
    tileMap.rngTiles(rng);

    for (uint8_t z = 0; z < tileMap.m_depth; ++z) {
        for (uint32_t y = 0; y < tileMap.m_height; ++y) {
            for (uint32_t x = 0; x < tileMap.m_width; ++x) {
                auto& tile              = tileMap.get(x, y, z);
                auto& destTile          = dest.m_tiles.get(x, y, z);
                destTile.m_terType      = static_cast<uint8_t>(tile.m_terrain->legacyId);
                destTile.m_terView      = tile.m_view;
                destTile.m_riverType    = static_cast<uint8_t>(tile.m_riverType);
                destTile.m_riverDir     = tile.m_riverView;
                destTile.m_roadType     = static_cast<uint8_t>(tile.m_roadType);
                destTile.m_roadDir      = tile.m_roadView;
                destTile.m_extTileFlags = 0;
                if (tile.m_flipHor)
                    destTile.m_extTileFlags |= MapTile::TerrainFlipHor;
                if (tile.m_flipVert)
                    destTile.m_extTileFlags |= MapTile::TerrainFlipVert;
                if (tile.m_roadFlipHor)
                    destTile.m_extTileFlags |= MapTile::RoadFlipHor;
                if (tile.m_roadFlipVert)
                    destTile.m_extTileFlags |= MapTile::RoadFlipVert;
                if (tile.m_riverFlipHor)
                    destTile.m_extTileFlags |= MapTile::RiverFlipHor;
                if (tile.m_riverFlipVert)
                    destTile.m_extTileFlags |= MapTile::RiverFlipVert;
                if (tile.m_coastal)
                    destTile.m_extTileFlags |= MapTile::Coastal;
            }
        }
    }

    for (auto& [playerId, fhPlayer] : src.m_players) {
        auto  index    = static_cast<int>(playerId);
        auto& h3player = dest.m_players[index];

        h3player.m_canHumanPlay    = fhPlayer.m_humanPossible;
        h3player.m_canComputerPlay = fhPlayer.m_aiPossible;

        uint16_t factionsBitmask = 0;
        for (Core::LibraryFactionConstPtr faction : fhPlayer.m_startingFactions) {
            assume(faction != nullptr);
            factionsBitmask |= 1U << uint32_t(faction->legacyId);
        }
        h3player.m_allowedFactionsBitmask = factionsBitmask;
    }
    for (auto& bit : dest.m_allowedHeroes)
        bit = 1;

    ObjectTemplateCache tmplCache(database);

    for (auto* def : src.m_initialObjectDefs)
        tmplCache.add(def);

    tmplCache.addId("avwmrnd0");
    tmplCache.addId("avlholg0");

    for (auto& fhTown : src.m_towns) {
        auto playerIndex = static_cast<int>(fhTown.m_player);
        if (playerIndex >= 0) {
            auto& h3player = dest.m_players[playerIndex];
            if (fhTown.m_isMain) {
                const int townGateOffset = 2;
                h3player.m_hasMainTown   = true;
                h3player.m_posOfMainTown = int3fromPos(fhTown.m_pos, -townGateOffset);
                h3player.m_generateHero  = true;
            }
        }
        auto cas1               = std::make_unique<MapTown>(dest.m_features);
        cas1->m_playerOwner     = playerIndex;
        cas1->m_hasFort         = fhTown.m_hasFort;
        cas1->m_questIdentifier = fhTown.m_questIdentifier;
        cas1->m_spellResearch   = fhTown.m_spellResearch;
        //cas1->m_formation       = 0xCC;
        cas1->prepareArrays();
        auto* libraryFaction = fhTown.m_factionId;
        assert(libraryFaction);
        uint32_t defnum;
        if (!fhTown.m_defFile.empty()) {
            defnum = tmplCache.addId(fhTown.m_defFile);
        } else {
            defnum = tmplCache.add(libraryFaction->mapObjectDef);
        }

        dest.m_objects.push_back(Object{ .m_order = fhTown.m_order, .m_pos = int3fromPos(fhTown.m_pos), .m_defnum = defnum, .m_impl = std::move(cas1) });
    }

    for (auto& fhHero : src.m_wanderingHeroes) {
        auto  playerIndex = static_cast<int>(fhHero.m_player);
        auto* libraryHero = fhHero.m_data.m_army.hero.library;
        assert(libraryHero);

        const uint8_t heroId = libraryHero->legacyId;

        if (playerIndex >= 0) {
            auto& h3player = dest.m_players[playerIndex];
            if (fhHero.m_isMain) {
                h3player.m_mainCustomHeroId = heroId;
                h3player.m_generateHero     = false;
            }
            h3player.m_heroesNames.push_back(SHeroName{ .m_heroId = heroId, .m_heroName = "" });
        }

        auto hero               = std::make_unique<MapHero>(dest.m_features);
        hero->m_playerOwner     = playerIndex;
        hero->m_subID           = heroId;
        hero->m_questIdentifier = fhHero.m_questIdentifier;
        hero->prepareArrays();
        {
            hero->m_hasExp                             = fhHero.m_data.m_hasExp;
            hero->m_hasSecSkills                       = fhHero.m_data.m_hasSecSkills;
            hero->m_primSkillSet.m_hasCustomPrimSkills = fhHero.m_data.m_hasPrimSkills;
            hero->m_spellSet.m_hasCustomSpells         = fhHero.m_data.m_hasSpells;
            if (hero->m_hasExp)
                hero->m_exp = static_cast<int32_t>(fhHero.m_data.m_army.hero.experience);
        }
        dest.m_allowedHeroes[heroId] = 0;
        if (playerIndex < 0) {
            dest.m_objects.push_back(Object{ .m_order = fhHero.m_order, .m_pos = int3fromPos(fhHero.m_pos), .m_defnum = tmplCache.addId("avxprsn0"), .m_impl = std::move(hero) });
        } else {
            dest.m_objects.push_back(Object{ .m_order = fhHero.m_order, .m_pos = int3fromPos(fhHero.m_pos, +1), .m_defnum = tmplCache.addHero(libraryHero), .m_impl = std::move(hero) });
        }
    }

    for (auto& allowed : dest.m_allowedHeroes)
        allowed = 1;
    for (auto& allowed : dest.m_allowedArtifacts)
        allowed = 1;
    for (auto& allowed : dest.m_allowedSpells)
        allowed = 1;
    for (auto& allowed : dest.m_allowedSecSkills)
        allowed = 1;

    for (const auto& heroId : src.m_disabledHeroes) {
        const auto legacyId            = (heroId)->legacyId;
        dest.m_allowedHeroes[legacyId] = 0;
    }

    if (src.m_version == Core::GameVersion::HOTA) {
        // these artifact ids are just unexistent.
        dest.m_allowedArtifacts[145] = 0;
        dest.m_allowedArtifacts[144] = 0;
    }
    for (const auto& artId : src.m_disabledArtifacts) {
        const auto legacyId               = (artId)->legacyId;
        dest.m_allowedArtifacts[legacyId] = 0;
    }

    for (const auto& spellId : src.m_disabledSpells) {
        const auto legacyId            = (spellId)->legacyId;
        dest.m_allowedSpells[legacyId] = 0;
    }
    for (const auto& secSkillId : src.m_disabledSkills) {
        const auto legacyId               = (secSkillId)->legacyId;
        dest.m_allowedSecSkills[legacyId] = 0;
    }
    for (auto& customHero : src.m_customHeroes) {
        const auto legacyId = customHero.m_army.hero.library->legacyId;
        auto&      destHero = dest.m_customHeroData[legacyId];
        destHero.prepareArrays();
        destHero.m_enabled                            = true;
        destHero.m_hasExp                             = customHero.m_hasExp;
        destHero.m_hasCustomBio                       = customHero.m_hasCustomBio;
        destHero.m_hasSkills                          = customHero.m_hasSecSkills;
        destHero.m_primSkillSet.m_hasCustomPrimSkills = customHero.m_hasPrimSkills;
        destHero.m_spellSet.m_hasCustomSpells         = customHero.m_hasSpells;
        if (customHero.m_hasPrimSkills) {
            auto& prim = destHero.m_primSkillSet.m_primSkills;
            prim.resize(4);
            std::tie(prim[0], prim[1]) = customHero.m_army.hero.currentBasePrimary.ad.asTuple();
            std::tie(prim[2], prim[3]) = customHero.m_army.hero.currentBasePrimary.magic.asTuple();
        }
        if (customHero.m_hasSecSkills) {
            for (auto& sk : customHero.m_army.hero.secondarySkills) {
                destHero.m_skills.push_back({ static_cast<uint8_t>(sk.skill->legacyId), static_cast<uint8_t>(sk.level) });
            }
        }
        if (destHero.m_spellSet.m_hasCustomSpells) {
            for (auto* spell : customHero.m_army.hero.spellbook)
                destHero.m_spellSet.m_spells[spell->legacyId] = 1;
        }
    }

    for (auto& fhRes : src.m_objects.m_resources) {
        if (fhRes.m_type == FHResource::Type::Resource) {
            auto res      = std::make_unique<MapResource>(dest.m_features);
            res->m_amount = fhRes.m_amount;

            dest.m_objects.push_back(Object{ .m_order = fhRes.m_order, .m_pos = int3fromPos(fhRes.m_pos), .m_defnum = tmplCache.add(fhRes.m_id->mapObjectDef), .m_impl = std::move(res) });
        } else {
            auto* def = fhRes.m_visitableId->mapObjectDefs[fhRes.m_defVariant];
            dest.m_objects.push_back(Object{ .m_order = fhRes.m_order, .m_pos = int3fromPos(fhRes.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::make_unique<MapObjectSimple>(dest.m_features) });
        }
    }

    for (auto& fhRes : src.m_objects.m_resourcesRandom) {
        auto res      = std::make_unique<MapResource>(dest.m_features);
        res->m_amount = fhRes.m_amount;
        dest.m_objects.push_back(Object{ .m_order = fhRes.m_order, .m_pos = int3fromPos(fhRes.m_pos), .m_defnum = tmplCache.addId("avtrndm0"), .m_impl = std::move(res) });
    }

    for (auto& fhArt : src.m_objects.m_artifacts) {
        auto art = std::make_unique<MapArtifact>(dest.m_features, false);
        if (fhArt.m_id->scrollSpell) {
            art->m_spellId = fhArt.m_id->scrollSpell->legacyId;
            art->m_isSpell = true;
        }
        dest.m_objects.push_back(Object{ .m_order = fhArt.m_order, .m_pos = int3fromPos(fhArt.m_pos), .m_defnum = tmplCache.add(fhArt.m_id->mapObjectDef), .m_impl = std::move(art) });
    }
    for (auto& fhArt : src.m_objects.m_artifactsRandom) {
        auto        art = std::make_unique<MapArtifact>(dest.m_features, false);
        std::string id  = "";
        switch (fhArt.m_type) {
            case FHRandomArtifact::Type::Any:
                id = "avarand";
                break;
            case FHRandomArtifact::Type::Treasure:
                id = "avarnd1";
                break;
            case FHRandomArtifact::Type::Minor:
                id = "avarnd2";
                break;
            case FHRandomArtifact::Type::Major:
                id = "avarnd3";
                break;
            case FHRandomArtifact::Type::Relic:
                id = "avarnd4";
                break;
            case FHRandomArtifact::Type::Invalid:
                break;
        }

        dest.m_objects.push_back(Object{ .m_order = fhArt.m_order, .m_pos = int3fromPos(fhArt.m_pos), .m_defnum = tmplCache.addId(id), .m_impl = std::move(art) });
    }

    for (auto& fhMon : src.m_objects.m_monsters) {
        auto monster               = std::make_unique<MapMonster>(dest.m_features);
        monster->m_count           = static_cast<uint16_t>(fhMon.m_count);
        monster->m_questIdentifier = fhMon.m_questIdentifier;
        if (fhMon.m_agressionMax == fhMon.m_agressionMin) {
            if (fhMon.m_agressionMax == 0)
                monster->m_joinAppeal = 0;
            else if (fhMon.m_agressionMax == 10)
                monster->m_joinAppeal = 4;
            else {
                monster->m_joinAppeal     = 5;
                monster->m_agressionExact = fhMon.m_agressionMax;
            }
        } else if (fhMon.m_agressionMin == 1 && fhMon.m_agressionMax == 7) {
            monster->m_joinAppeal = 1;
        } else if (fhMon.m_agressionMin == 1 && fhMon.m_agressionMax == 10) {
            monster->m_joinAppeal = 2;
        } else if (fhMon.m_agressionMin == 4 && fhMon.m_agressionMax == 10) {
            monster->m_joinAppeal = 3;
        } else {
            throw std::runtime_error("unsupported monster appeal");
        }
        monster->m_joinOnlyForMoney = fhMon.m_joinOnlyForMoney;
        monster->m_joinPercent      = fhMon.m_joinPercent;

        dest.m_objects.push_back(Object{ .m_order = fhMon.m_order, .m_pos = int3fromPos(fhMon.m_pos, dest.m_features->m_monstersMapXOffset), .m_defnum = tmplCache.add(fhMon.m_id->mapObjectDef), .m_impl = std::move(monster) });
    }

    for (auto& fhDwelling : src.m_objects.m_dwellings) {
        auto dwell     = std::make_unique<MapObjectWithOwner>(dest.m_features);
        dwell->m_owner = static_cast<uint8_t>(fhDwelling.m_player);

        auto* def = fhDwelling.m_id->mapObjectDefs[fhDwelling.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhDwelling.m_order, .m_pos = int3fromPos(fhDwelling.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(dwell) });
    }
    for (auto& fhMine : src.m_objects.m_mines) {
        auto mine     = std::make_unique<MapObjectWithOwner>(dest.m_features);
        mine->m_owner = static_cast<uint8_t>(fhMine.m_player);

        auto* def = fhMine.m_id->minesDefs[fhMine.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhMine.m_order, .m_pos = int3fromPos(fhMine.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(mine) });
    }

    for (auto& fhBank : src.m_objects.m_banks) {
        auto bank = std::make_unique<MapObjectCreatureBank>(dest.m_features);

        if (!fhBank.m_guardsVariants.empty()) {
            const int variantsCount = fhBank.m_id->variants.size();
            if (variantsCount > 4) {
                bank->m_content = fhBank.m_guardsVariants[0];
                if (bank->m_content > 4)
                    bank->m_content -= variantsCount / 2;
                if (fhBank.m_guardsVariants.size() == 2)
                    bank->m_upgraded = 0xffu;
                else if (fhBank.m_guardsVariants[0] < variantsCount / 2)
                    bank->m_upgraded = 0;
                else
                    bank->m_upgraded = 1;
            } else {
                bank->m_content = fhBank.m_guardsVariants[0];
            }
        }

        auto* def = fhBank.m_id->mapObjectDefs[fhBank.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhBank.m_order, .m_pos = int3fromPos(fhBank.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(bank) });
    }
    for (auto& fhObstacle : src.m_objects.m_obstacles) {
        auto obj = std::make_unique<MapObjectSimple>(dest.m_features);

        auto* def = fhObstacle.m_id->mapObjectDef;
        dest.m_objects.push_back(Object{ .m_order = fhObstacle.m_order, .m_pos = int3fromPos(fhObstacle.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(obj) });
    }
    for (auto& fhVisitable : src.m_objects.m_visitables) {
        auto obj = std::make_unique<MapObjectSimple>(dest.m_features);

        auto* def = fhVisitable.m_visitableId->mapObjectDefs[fhVisitable.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhVisitable.m_order, .m_pos = int3fromPos(fhVisitable.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(obj) });
    }
    for (auto& fhShrine : src.m_objects.m_shrines) {
        auto obj = std::make_unique<MapShrine>(dest.m_features);
        if (fhShrine.m_spellId)
            obj->m_spell = static_cast<uint8_t>(fhShrine.m_spellId->legacyId);
        else
            obj->m_spell = 0xffU;

        auto* def = fhShrine.m_visitableId->mapObjectDefs[fhShrine.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhShrine.m_order, .m_pos = int3fromPos(fhShrine.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(obj) });
    }
    for (auto& fhSkillHut : src.m_objects.m_skillHuts) {
        auto obj = std::make_unique<MapWitchHut>(dest.m_features);
        for (auto* allowedSkill : fhSkillHut.m_skillIds)
            obj->m_allowedSkills[allowedSkill->legacyId] = 1;

        auto* def = fhSkillHut.m_visitableId->mapObjectDefs[fhSkillHut.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhSkillHut.m_order, .m_pos = int3fromPos(fhSkillHut.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(obj) });
    }
    for (auto& fhScholar : src.m_objects.m_scholars) {
        auto obj         = std::make_unique<MapScholar>(dest.m_features);
        obj->m_bonusType = fhScholar.m_type == FHScholar::Type::Random ? 0xffU : static_cast<uint8_t>(fhScholar.m_type);
        if (fhScholar.m_type == FHScholar::Type::Primary)
            obj->m_bonusId = static_cast<uint8_t>(fhScholar.m_primaryType);
        else if (fhScholar.m_type == FHScholar::Type::Secondary)
            obj->m_bonusId = static_cast<uint8_t>(fhScholar.m_skillId->legacyId);
        else if (fhScholar.m_type == FHScholar::Type::Spell)
            obj->m_bonusId = static_cast<uint8_t>(fhScholar.m_spellId->legacyId);

        auto* def = fhScholar.m_visitableId->mapObjectDefs[fhScholar.m_defVariant];
        dest.m_objects.push_back(Object{ .m_order = fhScholar.m_order, .m_pos = int3fromPos(fhScholar.m_pos), .m_defnum = tmplCache.add(def), .m_impl = std::move(obj) });
    }
    auto convertStacks = [](const std::vector<Core::UnitWithCount>& stacks) -> std::vector<StackBasicDescriptor> {
        std::vector<StackBasicDescriptor> result;
        for (auto& stack : stacks)
            result.push_back({ static_cast<uint16_t>(stack.unit->legacyId), static_cast<uint16_t>(stack.count) });
        return result;
    };

    auto convertReward = [&convertStacks](const Core::Reward& fhReward, MapReward& reward) {
        reward.m_gainedExp  = fhReward.gainedExp;
        reward.m_manaDiff   = fhReward.manaDiff;
        reward.m_luckDiff   = fhReward.rngBonus.luck;
        reward.m_moraleDiff = fhReward.rngBonus.morale;

        reward.m_resourceSet.m_resourceAmount[0] = fhReward.resources.wood;
        reward.m_resourceSet.m_resourceAmount[1] = fhReward.resources.mercury;
        reward.m_resourceSet.m_resourceAmount[2] = fhReward.resources.ore;
        reward.m_resourceSet.m_resourceAmount[3] = fhReward.resources.sulfur;
        reward.m_resourceSet.m_resourceAmount[4] = fhReward.resources.crystal;
        reward.m_resourceSet.m_resourceAmount[5] = fhReward.resources.gems;
        reward.m_resourceSet.m_resourceAmount[6] = fhReward.resources.gold;

        // @todo: m_primSkillSet
        // @todo: m_secSkills
        // @todo: m_artifacts
        for (auto* spell : fhReward.spells.onlySpells)
            reward.m_spells.push_back(static_cast<uint8_t>(spell->legacyId));

        reward.m_creatures.m_stacks = convertStacks(fhReward.units);
    };

    for (auto& fhPandora : src.m_objects.m_pandoras) {
        auto obj = std::make_unique<MapPandora>(dest.m_features);
        convertReward(fhPandora.m_reward, obj->m_reward);

        dest.m_objects.push_back(Object{ .m_order = fhPandora.m_order, .m_pos = int3fromPos(fhPandora.m_pos), .m_defnum = tmplCache.addId("ava0128"), .m_impl = std::move(obj) });
    }

    dest.m_objectDefs = std::move(tmplCache.m_objectDefs);

    std::sort(dest.m_objects.begin(), dest.m_objects.end(), [](const auto& lh, const auto& rh) { return lh.m_order < rh.m_order; });
}

}
