/*
 * Copyright (C) 2020 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "AdventureReplay.hpp"

#include "Reflection/PropertyTreeReader.hpp"
#include "Reflection/PropertyTreeWriter.hpp"

#include "BattleReplayReflection.hpp"

#include "IGameDatabase.hpp"
#include "LibraryTerrain.hpp"
#include "PropertyTree.hpp"
#include "FileFormatJson.hpp"
#include "FileIOUtils.hpp"

namespace FreeHeroes::Core {

bool AdventureReplayData::load(const std_path& filename, IGameDatabase& gameDatabase)
{
    std::string buffer;
    if (!readFileIntoBuffer(filename, buffer))
        return false;
    PropertyTree main;
    if (!readJsonFromBuffer(buffer, main))
        return false;

    const PropertyTree&            jsonBattle  = main["bat"];
    const PropertyTree&            jsonRecords = jsonBattle["records"];
    Reflection::PropertyTreeReader reader(gameDatabase);
    if (jsonRecords.isList()) {
        for (const PropertyTree& jsonRecord : jsonRecords.getList()) {
            m_bat.m_records.push_back({});
            BattleReplayData::EventRecord& event = m_bat.m_records.back();
            reader.jsonToValue(jsonRecord, event);
            if (event.type == BattleReplayData::EventRecord::Type::MoveAttack)
                assert(!event.moveParams.m_movePos.mainPos().isEmpty());
        }
    }
    const PropertyTree& jsonAdventure = main["adv"];
    m_adv.m_seed                      = jsonAdventure["seed"].getScalar().toInt();
    auto terrainId                    = jsonAdventure["terrain"].getScalar().toString();
    m_adv.m_terrain                   = gameDatabase.terrains()->find(terrainId);
    assert(m_adv.m_terrain);
    reader.jsonToValue(jsonAdventure["field"], m_adv.m_field);
    reader.jsonToValue(jsonAdventure["att"], m_adv.m_att);
    reader.jsonToValue(jsonAdventure["def"], m_adv.m_def);

    return true;
}

bool AdventureReplayData::save(const std_path& filename) const
{
    PropertyTree  main;
    PropertyTree& jsonBattle  = main["bat"];
    PropertyTree& jsonRecords = jsonBattle["records"];

    Reflection::PropertyTreeWriter writer;
    for (const auto& record : m_bat.m_records) {
        PropertyTree row = writer.valueToJson(record);
        jsonRecords.append(std::move(row));
    }
    PropertyTree& jsonAdventure = main["adv"];
    jsonAdventure["seed"]       = PropertyTreeScalar(m_adv.m_seed);
    jsonAdventure["terrain"]    = PropertyTreeScalar(m_adv.m_terrain->id);
    jsonAdventure["field"]      = writer.valueToJson(m_adv.m_field);
    jsonAdventure["att"]        = writer.valueToJson(m_adv.m_att);
    jsonAdventure["def"]        = writer.valueToJson(m_adv.m_def);

    std::string buffer;
    return writeJsonToBuffer(buffer, main) && writeFileFromBuffer(filename, buffer);
}

}
