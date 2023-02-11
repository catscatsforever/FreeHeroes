/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "TemplateDifficultyWidget.hpp"

#include "ui_TemplateDifficultyWidget.h"

#include "FHTemplate.hpp"

namespace FreeHeroes {

TemplateDifficultyWidget::TemplateDifficultyWidget(QWidget* parent)
    : QFrame(parent)
    , m_ui(std::make_unique<Ui::TemplateDifficultyWidget>())
{
    m_ui->setupUi(this);

    m_ui->sliderMinGuard->setRange(10, 400);
    m_ui->sliderMaxGuard->setRange(10, 400);

    m_ui->sliderMinGuard->setText(tr("Min guard multiply %"), 150);
    m_ui->sliderMaxGuard->setText(tr("Max guard multiply %"), 150);

    m_ui->sliderMinArmy->setRange(0, 300);
    m_ui->sliderMaxArmy->setRange(0, 300);

    m_ui->sliderMinArmy->setText(tr("Min army value multiply %"), 150);
    m_ui->sliderMaxArmy->setText(tr("Max army value multiply %"), 150);

    m_ui->sliderMinGold->setRange(0, 300);
    m_ui->sliderMaxGold->setRange(0, 300);

    m_ui->sliderMinGold->setText(tr("Min gold value multiply %"), 150);
    m_ui->sliderMaxGold->setText(tr("Max gold value multiply %"), 150);

    connect(m_ui->pushButtonReset, &QAbstractButton::clicked, this, [this] {
        m_userSettings->m_difficulty = {};
        updateUI();
    });
}

TemplateDifficultyWidget::~TemplateDifficultyWidget() = default;

void TemplateDifficultyWidget::updateUI()
{
    m_ui->sliderMinGuard->setValue(m_userSettings->m_difficulty.m_minGuardsPercent);
    m_ui->sliderMaxGuard->setValue(m_userSettings->m_difficulty.m_maxGuardsPercent);

    m_ui->sliderMinArmy->setValue(m_userSettings->m_difficulty.m_minArmyPercent);
    m_ui->sliderMaxArmy->setValue(m_userSettings->m_difficulty.m_maxArmyPercent);

    m_ui->sliderMinGold->setValue(m_userSettings->m_difficulty.m_minGoldPercent);
    m_ui->sliderMaxGold->setValue(m_userSettings->m_difficulty.m_maxGoldPercent);
}

void TemplateDifficultyWidget::save()
{
    m_userSettings->m_difficulty.m_minGuardsPercent = m_ui->sliderMinGuard->getValue();
    m_userSettings->m_difficulty.m_maxGuardsPercent = m_ui->sliderMaxGuard->getValue();

    m_userSettings->m_difficulty.m_minArmyPercent = m_ui->sliderMinArmy->getValue();
    m_userSettings->m_difficulty.m_maxArmyPercent = m_ui->sliderMaxArmy->getValue();

    m_userSettings->m_difficulty.m_minGoldPercent = m_ui->sliderMinGold->getValue();
    m_userSettings->m_difficulty.m_maxGoldPercent = m_ui->sliderMaxGold->getValue();
}

}
