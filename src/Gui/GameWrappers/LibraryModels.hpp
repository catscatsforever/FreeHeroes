/*
 * Copyright (C) 2020 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#pragma once

#include "GuiGameWrappersExport.hpp"

#include "LibraryWrappers.hpp"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QConcatenateTablesProxyModel>

#include <memory>

namespace FreeHeroes::Core {
class IGameDatabase;
}

namespace FreeHeroes::Gui {

template<typename WrapperTypeT>
class GUIGAMEWRAPPERS_EXPORT AbstractGuiWrapperListModel : public QAbstractListModel {
public:
    using WrapperType = WrapperTypeT;
    using SrcType = typename WrapperType::SrcType;
    using WrapperTypePtr = const WrapperType *;
    using SrcTypePtr = const SrcType *;

    AbstractGuiWrapperListModel(Sound::IMusicBox & musicBox, IGraphicsLibrary & graphicsLibrary, QObject * parent);
    ~AbstractGuiWrapperListModel();

    void clear();
    void addRecord(SrcTypePtr record);
    WrapperTypePtr find(SrcTypePtr source) const;
    WrapperTypePtr find(const std::string & id) const;

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    enum ItemDataRole {
        GuiObject = Qt::UserRole + 1, // WrapperTypePtr
        SourceObject,                 // SrcTypePtr

        IconSmall,
        IconMedium,
        IconLarge,

        LastBaseRole
    };

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    IGraphicsLibrary & m_graphicsLibrary;
    Sound::IMusicBox & m_musicBox;
};

class GUIGAMEWRAPPERS_EXPORT ArtifactsModel : public AbstractGuiWrapperListModel<GuiArtifact>
{
public:
    using Base = AbstractGuiWrapperListModel<GuiArtifact>;
    ArtifactsModel(Sound::IMusicBox & musicBox, IGraphicsLibrary & graphicsLibrary, QObject * parent);

    QVariant data(const QModelIndex &index, int role) const override;

    GuiArtifactConstPtr findCatapult() const { return this->find(s_catapult);}
    GuiArtifactConstPtr findSpellbook() const { return this->find(s_spellbook);}
    QPixmap getLockIcon() const { return m_lock->get(); }
private:
    static const std::string s_catapult;
    static const std::string s_spellbook;
    IAsyncPixmapPtr m_lock;
};

class GUIGAMEWRAPPERS_EXPORT ArtifactsFilterModel  : public QSortFilterProxyModel
{
public:
    ArtifactsFilterModel(QObject * parent);

    void setFilterIndex(int filter);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    int m_filter = -1;

};



class GUIGAMEWRAPPERS_EXPORT UnitsModel : public AbstractGuiWrapperListModel<GuiUnit>, public GuiUnitProvider
{
public:
    using Base = AbstractGuiWrapperListModel<GuiUnit>;
    using AbstractGuiWrapperListModel::AbstractGuiWrapperListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    GuiUnitConstPtr find(Core::LibraryUnitConstPtr source) const override {
        return Base::find(source);
    }
    using Base::find;
};

class GUIGAMEWRAPPERS_EXPORT UnitsFilterModel  : public QSortFilterProxyModel
{
public:
    UnitsFilterModel(QObject * parent);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};


class GUIGAMEWRAPPERS_EXPORT HeroesModel : public AbstractGuiWrapperListModel<GuiHero>, public GuiHeroProvider
{
public:
    using Base = AbstractGuiWrapperListModel<GuiHero>;
    using AbstractGuiWrapperListModel::AbstractGuiWrapperListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    GuiHeroConstPtr find(Core::LibraryHeroConstPtr source) const override {
        return Base::find(source);
    }
    using Base::find;
};


class GUIGAMEWRAPPERS_EXPORT SkillsModel : public AbstractGuiWrapperListModel<GuiSkill> , public GuiSkillProvider
{
public:
    using Base = AbstractGuiWrapperListModel<GuiSkill>;
    using AbstractGuiWrapperListModel::AbstractGuiWrapperListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    GuiSkillConstPtr find(Core::LibrarySecondarySkillConstPtr source) const override {
        return Base::find(source);
    }
    using Base::find;
};

class GUIGAMEWRAPPERS_EXPORT SpellsModel : public AbstractGuiWrapperListModel<GuiSpell> , public GuiSpellProvider
{
public:
    using Base = AbstractGuiWrapperListModel<GuiSpell>;
    using AbstractGuiWrapperListModel::AbstractGuiWrapperListModel;

    QVariant data(const QModelIndex &index, int role) const override;
    GuiSpellConstPtr find(Core::LibrarySpellConstPtr source) const override {
        return Base::find(source);
    }
    using Base::find;
};

class GUIGAMEWRAPPERS_EXPORT SpellsFilterModel  : public QSortFilterProxyModel
{
public:
    SpellsFilterModel(QObject * parent);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class GUIGAMEWRAPPERS_EXPORT FactionsModel : public AbstractGuiWrapperListModel<GuiFaction>
{
public:
    using Base = AbstractGuiWrapperListModel<GuiFaction>;
    using AbstractGuiWrapperListModel::AbstractGuiWrapperListModel;

    QVariant data(const QModelIndex &index, int role) const override;

};

class GUIGAMEWRAPPERS_EXPORT FactionsFilterModel  : public QSortFilterProxyModel
{
public:
    FactionsFilterModel(QObject * parent);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class UiCommonModel;
class GUIGAMEWRAPPERS_EXPORT LibraryModelsProvider : public QObject
{
    Q_OBJECT
public:
    LibraryModelsProvider(Core::IGameDatabase & gameDatabase,
                          Sound::IMusicBox & musicBox,
                          IGraphicsLibrary & graphicsLibrary,
                          QObject * parent = nullptr);

    ArtifactsModel * artifacts() const noexcept { return m_artifacts;}
    UnitsModel     * units    () const noexcept { return m_units;}
    HeroesModel    * heroes   () const noexcept { return m_heroes;}
    SkillsModel    * skills   () const noexcept { return m_skills;}
    SpellsModel    * spells   () const noexcept { return m_spells;}
    FactionsModel  * factions () const noexcept { return m_factions;}
    UiCommonModel  * ui       () const noexcept { return m_uiCommon;}

private:
    ArtifactsModel * m_artifacts;
    UnitsModel     * m_units;
    HeroesModel    * m_heroes;
    SkillsModel    * m_skills;
    SpellsModel    * m_spells;
    FactionsModel  * m_factions;
    UiCommonModel  * m_uiCommon;
};

}
