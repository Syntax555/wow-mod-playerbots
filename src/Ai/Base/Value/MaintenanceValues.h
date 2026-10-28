/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MAINTENANCEVALUES_H
#define PLAYERBOTS_MAINTENANCEVALUES_H

#include "Value.h"

class PlayerbotAI;

enum class NaturalMaintenanceType : uint8
{
    NONE,
    SELL,
    REPAIR,
    TRAIN,
    BUY
};

struct NaturalMaintenanceTask
{
    ObjectGuid target;
    NaturalMaintenanceType type = NaturalMaintenanceType::NONE;
    uint32 itemId = 0;
    uint32 budgetType = 0;
};

// Natural upkeep runs only for a living, idle bot that may move and has no game-client master.
bool CanRunNaturalMaintenance(PlayerbotAI* botAI);

// Natural bots do not craft, so a spell on a profession skill line is no upkeep goal.
bool IsProfessionSpell(uint32 spellId);

class NaturalMaintenanceValue : public CalculatedValue<NaturalMaintenanceTask>
{
public:
    NaturalMaintenanceValue(PlayerbotAI* botAI)
        : CalculatedValue<NaturalMaintenanceTask>(botAI, "natural maintenance", 30 * IN_MILLISECONDS)
    {
    }

    NaturalMaintenanceTask Calculate() override;
};

class CanMoveAroundValue : public BoolCalculatedValue
{
public:
    CanMoveAroundValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can move around", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class ShouldHomeBindValue : public BoolCalculatedValue
{
public:
    ShouldHomeBindValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "should home bind", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class ShouldRepairValue : public BoolCalculatedValue
{
public:
    ShouldRepairValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "should repair", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class CanRepairValue : public BoolCalculatedValue
{
public:
    CanRepairValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can repair", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class ShouldSellValue : public BoolCalculatedValue
{
public:
    ShouldSellValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "should sell", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class CanSellValue : public BoolCalculatedValue
{
public:
    CanSellValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can sell", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class CanTrainValue : public BoolCalculatedValue
{
public:
    CanTrainValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can train", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class CanFightEqualValue : public BoolCalculatedValue
{
public:
    CanFightEqualValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can fight equal", 4 * IN_MILLISECONDS) {}

    bool Calculate() override;
};

class CanFightEliteValue : public BoolCalculatedValue
{
public:
    CanFightEliteValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can fight elite") {}

    bool Calculate() override;
};

class CanFightBossValue : public BoolCalculatedValue
{
public:
    CanFightBossValue(PlayerbotAI* botAI) : BoolCalculatedValue(botAI, "can fight boss") {}

    bool Calculate() override;
};

#endif
