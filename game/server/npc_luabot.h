#ifndef NPC_LUABOT_H
#define NPC_LUABOT_H
#pragma once

#include "basecombatcharacter.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"

// —амосто¤тельный бот: без NextBot и без AI-нод.
// ѕуть строитс¤ по навмешу (.nav), движение (шаги, гравитаци¤, скольжение вдоль стен) сделано вручную.
class CNPC_LuaBot : public CBaseCombatCharacter
{
    DECLARE_CLASS(CNPC_LuaBot, CBaseCombatCharacter);
    DECLARE_DATADESC();

public:
    enum BotMode_t
    {
        MODE_IDLE,    // стоит на месте
        MODE_GOTO,    // идЄт в точку (после прибыти¤ переходит в IDLE)
        MODE_CHASE,   // бежит к ближайшему живому игроку
    };

    CNPC_LuaBot();

    virtual Class_T Classify() override { return CLASS_COMBINE; }
    virtual void Precache() override;
    virtual void Spawn() override;
    virtual void Event_Killed(const CTakeDamageInfo& info) override;

    // ”правление из Lua
    void MoveTo(const Vector& vecGoal);
    void Chase();
    void Stop();

    void BotThink();

private:
    bool BuildPath(const Vector& vecGoal);
    void WalkToward(const Vector& vecTarget, float dt);
    void MoveStep(const Vector& vecDelta, float dt);
    void UpdateAnimation(bool bMoving);

    BotMode_t           m_mode;
    Vector              m_vecGoal;
    CUtlVector<Vector>  m_waypoints;
    int                 m_iWaypoint;
    float               m_flFallSpeed;
    float               m_flLastThink;
    Vector              m_vecLastProgressPos;
    Vector              m_vecPendingDelta;

    CountdownTimer      m_repathTimer;
    CountdownTimer      m_hookTimer;
    CountdownTimer      m_stuckTimer;
};

#endif // NPC_LUABOT_H
