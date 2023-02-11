/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "IGameDatabase.hpp"
#include "IRandomGenerator.hpp"

#include "LibraryMapBank.hpp"

#include "ObjectGenerator.hpp"

#include <iostream>
#include <functional>

namespace FreeHeroes {

template<class Child>
struct CommonRecord {
    bool     m_enabled   = true;
    uint64_t m_frequency = 0;
    uint64_t m_attempts  = 1;

    Child& setFreq(int freq)
    {
        m_frequency = freq;
        return static_cast<Child&>(*this);
    }
};

template<class Record>
struct CommonRecordList {
    std::vector<Record>        m_records;
    size_t                     m_active    = 0;
    uint64_t                   m_frequency = 0;
    std::map<uint64_t, size_t> m_index;

    void updateFrequency()
    {
        m_frequency = 0;
        m_active    = 0;
        m_index.clear();
        for (size_t i = 0; const Record& rec : m_records) {
            if (rec.m_enabled) {
                m_index[m_frequency] = i;
                m_frequency += rec.m_frequency;
                m_active++;
            }
            i++;
        }
    }

    size_t getFreqIndex(uint64_t rngFreq) const
    {
        if (rngFreq == 0)
            return m_index.at(0);
        auto it = m_index.lower_bound(rngFreq);
        it--;
        const size_t lower = it->second;
        return lower;
    }

    void onDisable(Record& record)
    {
        record.m_attempts--;
        if (record.m_attempts == 0) {
            record.m_enabled = false;
            updateFrequency();
        }
    }
};

class ArtifactPool {
public:
    using ArtifactSet = std::set<Core::LibraryArtifactConstPtr>;
    using ArtList     = std::vector<Core::LibraryArtifactConstPtr>;

    static bool okFilter(Core::LibraryArtifactConstPtr art, bool enableFilter, const FHScoreSettings& scoreSettings)
    {
        if (!enableFilter)
            return true;

        const bool isStat = art->statBonus.nonEmptyAmount() > 0;
        auto       attr   = isStat ? FHScoreAttr::ArtStat : FHScoreAttr::ArtSupport;

        bool isValid = scoreSettings.isValidValue(attr, art->value);
        return isValid;
    }

    ArtifactPool(FHMap& map, const Core::IGameDatabase* database, Core::IRandomGenerator* const rng)
        : m_rng(rng)
    {
        for (auto* art : database->artifacts()->records()) {
            if (map.m_disabledArtifacts.isDisabled(map.m_isWaterMap, art))
                continue;

            m_artifacts.push_back(art);
        }
    }

    Core::LibraryArtifactConstPtr make(const Core::ArtifactFilter& filter, bool enableFilter, const FHScoreSettings& scoreSettings)
    {
        ArtList artList = filter.filterPossible(m_artifacts);
        if (artList.empty())
            return nullptr;

        ArtifactSet artSet(artList.cbegin(), artList.cend());
        m_pools[artSet].m_artList = artList;
        return m_pools[artSet].make(m_rng, enableFilter, scoreSettings);
    }
    bool isEmpty(const Core::ArtifactFilter& filter, bool enableFilter, const FHScoreSettings& scoreSettings) const
    {
        ArtList artList = filter.filterPossible(m_artifacts);
        if (artList.empty())
            return true;

        ArtList artListFiltered;
        for (auto art : artList) {
            if (okFilter(art, enableFilter, scoreSettings))
                artListFiltered.push_back(art);
        }

        return artListFiltered.empty();
    }

private:
    struct SubPool {
        ArtList m_artList;
        ArtList m_current;

        Core::LibraryArtifactConstPtr make(Core::IRandomGenerator* rng, bool enableFilter, const FHScoreSettings& scoreSettings)
        {
            bool hasReset = false;
            if (m_current.empty()) {
                m_current = m_artList;
                hasReset  = true;
            }
            auto art = makeOne(rng, enableFilter, scoreSettings);

            while (!art) {
                if (m_current.empty()) {
                    if (hasReset)
                        return nullptr;
                    m_current = m_artList;
                    hasReset  = true;
                }
                art = makeOne(rng, enableFilter, scoreSettings);
            }

            return art;
        }

        Core::LibraryArtifactConstPtr makeOne(Core::IRandomGenerator* rng, bool enableFilter, const FHScoreSettings& scoreSettings)
        {
            auto index = rng->gen(m_current.size() - 1);
            auto art   = m_current[index];
            m_current.erase(m_current.begin() + index);
            if (!okFilter(art, enableFilter, scoreSettings))
                return nullptr;

            return art;
        }
    };
    std::map<ArtifactSet, SubPool> m_pools;
    Core::IRandomGenerator* const  m_rng;

    ArtList m_artifacts;
};

template<class T>
struct ObjectGenerator::AbstractObject : public IObject {
    void        setPos(FHPos pos) override { m_obj.m_pos = pos; }
    void        place() const override { m_map->m_objects.container<T>().push_back(m_obj); }
    FHScore     getScore() const override { return m_obj.m_score; }
    void        disable() override { m_onDisable(); }
    std::string getId() const override { return m_obj.m_id->id; }
    int64_t     getGuard() const override { return m_obj.m_guard; }

    T                     m_obj;
    FHMap*                m_map = nullptr;
    std::function<void()> m_onDisable;
};

template<class Record>
struct ObjectGenerator::AbstractFactory : public IObjectFactory {
    AbstractFactory(FHMap& map, const Core::IGameDatabase* database, Core::IRandomGenerator* const rng)
        : m_rng(rng)
        , m_database(database)
        , m_map(map)
    {}

    uint64_t totalFreq() const override
    {
        return m_records.m_frequency;
    }
    size_t totalActiveRecords() const override
    {
        return m_records.m_active;
    }

    CommonRecordList<Record> m_records;

    Core::IRandomGenerator* const m_rng;
    const Core::IGameDatabase*    m_database;
    FHMap&                        m_map;
};

// ---------------------------------------------------------------------------------------

struct RecordBank : public CommonRecord<RecordBank> {
    Core::LibraryMapBankConstPtr m_id            = nullptr;
    int                          m_guardsVariant = 0;
};

struct ObjectGenerator::ObjectFactoryBank : public AbstractFactory<RecordBank> {
    struct ObjectBank : public AbstractObject<FHBank> {
        std::string getId() const override { return m_obj.m_id->id + " [" + std::to_string(m_obj.m_guardsVariant + 1) + "]"; }
    };

    ObjectFactoryBank(FHMap& map, const FHRngZone::GeneratorBank& genSettings, const FHScoreSettings& scoreSettings, const Core::IGameDatabase* database, Core::IRandomGenerator* const rng, ArtifactPool* artifactPool)
        : AbstractFactory<RecordBank>(map, database, rng)
        , m_artifactPool(artifactPool)
        , m_scoreSettings(scoreSettings)
    {
        if (!genSettings.m_isEnabled)
            return;

        for (auto* bank : database->mapBanks()->records()) {
            if (m_map.m_disabledBanks.isDisabled(m_map.m_isWaterMap, bank))
                continue;

            for (int i = 0, sz = (int) bank->variants.size(); i < sz; i++) {
                RecordBank record;
                record.m_guardsVariant = i;
                record.m_id            = bank;
                record.m_frequency     = bank->variants[i].frequency;

                {
                    const Core::Reward&        reward                = record.m_id->rewards[record.m_id->variants[record.m_guardsVariant].rewardIndex];
                    bool                       artifactRewardIsValid = true;
                    const Core::ArtifactFilter firstFilter           = reward.artifacts.empty() ? Core::ArtifactFilter{} : reward.artifacts[0];
                    for (const auto& filter : reward.artifacts) {
                        if (m_artifactPool->isEmpty(filter, firstFilter == filter, scoreSettings))
                            artifactRewardIsValid = false;
                    }
                    if (!artifactRewardIsValid) {
                        std::cerr << "---- art - skipping " << bank->id << " [" << i << "]\n";
                        continue;
                    }

                    {
                        FHScore score;
                        {
                            for (const auto& [id, count] : reward.resources.data) {
                                const int amount = count / id->pileSize;
                                const int value  = amount * id->value;
                                auto      attr   = (id->rarity == Core::LibraryResource::Rarity::Gold) ? FHScoreAttr::Gold : FHScoreAttr::Resource;
                                score[attr] += value;
                            }
                        }
                        std::list<bool> validChecks;
                        for (FHScoreAttr attr : { FHScoreAttr::Gold, FHScoreAttr::Resource }) {
                            if (score[attr]) {
                                bool isValid = scoreSettings.isValidValue(attr, score[attr]);
                                validChecks.push_back(isValid);
                            }
                        }
                        bool resourceRewardIsValid = validChecks.empty();
                        for (bool isValid : validChecks)
                            resourceRewardIsValid = resourceRewardIsValid || isValid;

                        if (!resourceRewardIsValid) {
                            std::cerr << "---- res - skipping " << bank->id << " [" << i << "]\n";
                            continue;
                        }
                    }

                    const size_t artRewards = reward.artifacts.size();
                    if (artRewards == 0)
                        record.m_attempts = 1;
                    else if (artRewards == 1)
                        record.m_attempts = 3;
                    else
                        record.m_attempts = 5;
                }

                m_records.m_records.push_back(record);
            }
        }
        m_records.updateFrequency();
    }

    IObjectPtr make(uint64_t rngFreq) override
    {
        const size_t index  = m_records.getFreqIndex(rngFreq);
        auto&        record = m_records.m_records[index];

        const bool upgraded = record.m_id->upgradedStackIndex == -1 ? false : m_rng->genSmall(3) == 0;
        ObjectBank obj;
        obj.m_onDisable = [this, &record] {
            m_records.onDisable(record);
        };
        obj.m_obj.m_id            = record.m_id;
        obj.m_obj.m_guardsVariant = record.m_guardsVariant;
        obj.m_obj.m_upgradedStack = upgraded ? FHBank::UpgradedStack::Yes : FHBank::UpgradedStack::No;
        obj.m_map                 = &m_map;

        const Core::Reward& reward = record.m_id->rewards[record.m_id->variants[record.m_guardsVariant].rewardIndex];
        FHScore             score;
        {
            for (const auto& [id, count] : reward.resources.data) {
                const int amount = count / id->pileSize;
                const int value  = amount * id->value;
                auto      attr   = (id->rarity == Core::LibraryResource::Rarity::Gold) ? FHScoreAttr::Gold : FHScoreAttr::Resource;
                score[attr] += value;
            }
        }

        {
            const Core::ArtifactFilter firstFilter = reward.artifacts.empty() ? Core::ArtifactFilter{} : reward.artifacts[0];
            for (const auto& filter : reward.artifacts) {
                auto art = m_artifactPool->make(filter, firstFilter == filter, m_scoreSettings);
                assert(art);
                obj.m_obj.m_artifacts.push_back(art);
                const bool isStat = art->statBonus.nonEmptyAmount() > 0;
                auto       attr   = isStat ? FHScoreAttr::ArtStat : FHScoreAttr::ArtSupport;
                score[attr] += art->value;
            }
        }
        {
            for (const auto& stack : reward.units) {
                auto      attr  = FHScoreAttr::Army;
                const int value = stack.unit->value * stack.count;
                score[attr] += value;
            }
        }
        obj.m_obj.m_score = score;
        obj.m_obj.m_guard = obj.m_obj.m_id->guardValue;

        return std::make_shared<ObjectBank>(std::move(obj));
    }

    ArtifactPool* const   m_artifactPool;
    const FHScoreSettings m_scoreSettings;
};

// ---------------------------------------------------------------------------------------

struct RecordArtifact : public CommonRecord<RecordArtifact> {
    Core::ArtifactFilter m_filter;
};

struct ObjectGenerator::ObjectFactoryArtifact : public AbstractFactory<RecordArtifact> {
    struct ObjectArtifact : public AbstractObject<FHArtifact> {
    };

    ObjectFactoryArtifact(FHMap& map, const FHRngZone::GeneratorArtifact& genSettings, const FHScoreSettings& scoreSettings, const Core::IGameDatabase* database, Core::IRandomGenerator* const rng, ArtifactPool* artifactPool)
        : AbstractFactory<RecordArtifact>(map, database, rng)
        , m_artifactPool(artifactPool)
        , m_scoreSettings(scoreSettings)
    {
        if (!genSettings.m_isEnabled)
            return;

        for (const auto& [_, value] : genSettings.m_records)
            m_records.m_records.push_back(RecordArtifact{
                .m_filter = value.m_filter }
                                              .setFreq(value.m_frequency));

        for (auto& record : m_records.m_records)
            record.m_enabled = !m_artifactPool->isEmpty(record.m_filter, true, scoreSettings);

        m_records.updateFrequency();
    }

    IObjectPtr make(uint64_t rngFreq) override
    {
        const size_t index  = m_records.getFreqIndex(rngFreq);
        auto&        record = m_records.m_records[index];

        ObjectArtifact obj;
        obj.m_onDisable = [this, &record] {
            m_records.onDisable(record);
        };
        obj.m_map = &m_map;

        FHScore score;
        obj.m_obj.m_id = m_artifactPool->make(record.m_filter, true, m_scoreSettings);
        assert(obj.m_obj.m_id);

        const bool isStat = obj.m_obj.m_id->statBonus.nonEmptyAmount() > 0;
        auto       attr   = isStat ? FHScoreAttr::ArtStat : FHScoreAttr::ArtSupport;
        score[attr]       = obj.m_obj.m_id->value;

        obj.m_obj.m_score = score;
        obj.m_obj.m_guard = obj.m_obj.m_id->guard;

        return std::make_shared<ObjectArtifact>(std::move(obj));
    }

    ArtifactPool* const   m_artifactPool;
    const FHScoreSettings m_scoreSettings;
};

// ---------------------------------------------------------------------------------------

struct ObjectResourcePile : public ObjectGenerator::AbstractObject<FHResource> {
};

struct RecordResourcePile : public CommonRecord<RecordResourcePile> {
    ObjectResourcePile m_obj;
};

struct ObjectGenerator::ObjectFactoryResourcePile : public AbstractFactory<RecordResourcePile> {
    ObjectFactoryResourcePile(FHMap& map, const FHRngZone::GeneratorResourcePile& genSettings, const FHScoreSettings& scoreSettings, const Core::IGameDatabase* database, Core::IRandomGenerator* const rng)
        : AbstractFactory<RecordResourcePile>(map, database, rng)
    {
        if (!genSettings.m_isEnabled)
            return;

        for (const auto& [_, value] : genSettings.m_records) {
            const auto attr = value.m_resource->rarity == Core::LibraryResource::Rarity::Gold ? FHScoreAttr::Gold : FHScoreAttr::Resource;
            for (int amount : value.m_amounts) {
                ObjectResourcePile obj;
                obj.m_obj.m_id          = value.m_resource;
                obj.m_obj.m_amount      = amount;
                obj.m_obj.m_guard       = value.m_guard;
                obj.m_obj.m_score[attr] = value.m_resource->value * (amount / value.m_resource->pileSize);
                if (scoreSettings.isValidValue(attr, obj.m_obj.m_score[attr]))
                    m_records.m_records.push_back(RecordResourcePile{ .m_obj = std::move(obj) }.setFreq(value.m_frequency));
            }
        }

        m_records.updateFrequency();
    }

    IObjectPtr make(uint64_t rngFreq) override
    {
        const size_t index  = m_records.getFreqIndex(rngFreq);
        auto&        record = m_records.m_records[index];

        ObjectResourcePile obj = record.m_obj;
        obj.m_onDisable        = [this, &record] {
            m_records.onDisable(record);
        };
        obj.m_map = &m_map;

        return std::make_shared<ObjectResourcePile>(std::move(obj));
    }
};

// ---------------------------------------------------------------------------------------

void ObjectGenerator::generate(const FHRngZone& zoneSettings)
{
    static const std::string indentBase("      ");

    auto tryGen = [this](const FHScore& targetScore, FHScore& currentScore, std::vector<IObjectFactoryPtr>& objectFactories) -> bool {
        static const std::string indent("        ");
        uint64_t                 totalWeight = 0;
        for (IObjectFactoryPtr& fac : objectFactories) {
            totalWeight += fac->totalFreq();
        }
        if (!totalWeight)
            return false;

        const uint64_t rngFreq = m_rng->gen(totalWeight - 1);

        auto isScoreOverflow = [&targetScore](const FHScore& current) {
            for (const auto& [key, val] : current) {
                if (!targetScore.contains(key))
                    return true;
                const auto targetVal = targetScore.at(key);
                if (val > targetVal)
                    return true;
            }
            return false;
        };

        uint64_t indexWeight = 0, baseWeight = 0;
        for (IObjectFactoryPtr& fac : objectFactories) {
            indexWeight += fac->totalFreq();
            if (indexWeight > rngFreq && fac->totalFreq()) {
                const uint64_t rngFreqForFactory = rngFreq - baseWeight;
                auto           obj               = fac->make(rngFreqForFactory);
                if (!obj)
                    throw std::runtime_error("Object factory failed to make an object!");
                if (obj->getScore().empty())
                    throw std::runtime_error("Object '" + obj->getId() + "' has no score!");

                FHScore currentScoreTmp = currentScore + obj->getScore();
                if (isScoreOverflow(currentScoreTmp)) {
                    obj->disable();
                    // m_logOutput << "try disable '" << obj->getId() << "'\n";
                    return true;
                }
                currentScore = currentScoreTmp;

                m_logOutput << indent << "add '" << obj->getId() << "' score=" << obj->getScore() << " guard=" << obj->getGuard() << "; current factory freq=" << fac->totalFreq() << ", active=" << fac->totalActiveRecords() << "\n";
                //m_logOutput << "Updated score=" << m_currentScore << "\n";

                m_objects.push_back(obj);
                return true;
            }
            baseWeight = indexWeight;
        }

        assert(false);
        return false;
    };

    ArtifactPool pool(m_map, m_database, m_rng);

    for (const auto& [scoreId, scoreSettings] : zoneSettings.m_scoreTargets) {
        if (!scoreSettings.m_isEnabled) {
            m_logOutput << indentBase << scoreId << " is disabled;\n";
            continue;
        }
        FHScore currentScore;
        FHScore targetScore;

        for (const auto& [key, val] : scoreSettings.m_score)
            targetScore[key] = val.m_target;

        std::vector<IObjectFactoryPtr> objectFactories;
        objectFactories.push_back(std::make_shared<ObjectFactoryBank>(m_map, zoneSettings.m_generators.m_banks, scoreSettings, m_database, m_rng, &pool));
        objectFactories.push_back(std::make_shared<ObjectFactoryArtifact>(m_map, zoneSettings.m_generators.m_artifacts, scoreSettings, m_database, m_rng, &pool));
        objectFactories.push_back(std::make_shared<ObjectFactoryResourcePile>(m_map, zoneSettings.m_generators.m_resources, scoreSettings, m_database, m_rng));

        for (int i = 0; i < 1000000; i++) {
            if (!tryGen(targetScore, currentScore, objectFactories)) {
                m_logOutput << indentBase << scoreId << " finished on [" << i << "] iteration\n";
                break;
            }
        }

        m_logOutput << indentBase << scoreId << " target score:" << targetScore << "\n";
        m_logOutput << indentBase << scoreId << " end score:" << currentScore << "\n";
        m_logOutput << indentBase << scoreId << " deficit score:" << (targetScore - currentScore) << "\n";
    }
}

}
