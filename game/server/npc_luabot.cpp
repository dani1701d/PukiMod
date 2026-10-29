#include "cbase.h"
#include "npc_luabot.h"
#include "luamanager.h"

// Модель-заглушка (анимации не обязательны; если у модели есть ACT_IDLE / ACT_WALK, они проиграются)
#define LUABOT_MODEL        "models/error.mdl"

#define LUABOT_SPEED        170.0f   // скорость ходьбы, юнитов/с
#define LUABOT_STEP_HEIGHT  18.0f    // на какую ступеньку бот поднимается без прыжка
#define LUABOT_MAX_CLIMB    20.0f    // подъём между навмеш-зонами выше этого бот не осилит
#define LUABOT_GRAVITY      800.0f
#define LUABOT_TURN_RATE    540.0f   // градусов в секунду
#define LUABOT_THINK        0.03f    // период мышления, с

LINK_ENTITY_TO_CLASS(npc_luabot, CNPC_LuaBot);

BEGIN_DATADESC(CNPC_LuaBot)
DEFINE_THINKFUNC(BotThink),
END_DATADESC()

// ---------------------------------------------------------------------------
// Стоимость пути по навмешу: запрещаем то, что бот физически не может пройти.
// Если сигнатура не совпала с вашим nav_pathfind.h, скопируйте её из ShortestPathCost.
// ---------------------------------------------------------------------------
class LuaBotPathCost
{
public:
    float operator()(CNavArea* area, CNavArea* fromArea, const CNavLadder* ladder, const CFuncElevator* elevator, float length)
    {
        if (!fromArea)
            return 0.0f;                                  // стартовая зона

        if (ladder || elevator)
            return -1.0f;                                 // лестницы и лифты бот не использует

        if (area->GetAttributes() & NAV_MESH_CROUCH)
            return -1.0f;                                 // бот не умеет приседать

        if (fromArea->ComputeAdjacentConnectionHeightChange(area) > LUABOT_MAX_CLIMB)
            return -1.0f;                                 // слишком высокий уступ (бот не прыгает)

        float dist = (length > 0.0f) ? length : (area->GetCenter() - fromArea->GetCenter()).Length();
        return dist + fromArea->GetCostSoFar();
    }
};

static CBasePlayer* FindNearestPlayer(const Vector& vecFrom)
{
    CBasePlayer* pBest = NULL;
    float flBest = FLT_MAX;

    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
        if (!pPlayer || !pPlayer->IsAlive())
            continue;

        float flDist = vecFrom.DistToSqr(pPlayer->GetAbsOrigin());
        if (flDist < flBest)
        {
            flBest = flDist;
            pBest = pPlayer;
        }
    }
    return pBest;
}

// ---------------------------------------------------------------------------

CNPC_LuaBot::CNPC_LuaBot()
{
    m_mode = MODE_CHASE;
    m_vecGoal = vec3_origin;
    m_iWaypoint = 0;
    m_flFallSpeed = 0.0f;
    m_flLastThink = 0.0f;
    m_vecLastProgressPos = vec3_origin;
    m_vecPendingDelta = vec3_origin;
}

void CNPC_LuaBot::Precache()
{
    BaseClass::Precache();
    PrecacheModel(LUABOT_MODEL);
}

void CNPC_LuaBot::Spawn()
{
    Precache();
    SetModel(LUABOT_MODEL);

    BaseClass::Spawn();

    SetMoveType(MOVETYPE_NONE);            // позицию бота меняем сами (см. MoveStep)
    SetSolid(SOLID_BBOX);
    SetCollisionGroup(COLLISION_GROUP_NPC);
    UTIL_SetSize(this, Vector(-16, -16, 0), Vector(16, 16, 72));

    SetHealth(100);
    SetMaxHealth(100);
    m_takedamage = DAMAGE_YES;

    m_vecLastProgressPos = GetAbsOrigin();
    m_flLastThink = gpGlobals->curtime;

    SetThink(&CNPC_LuaBot::BotThink);
    SetNextThink(gpGlobals->curtime + 0.1f);

    Msg("[LuaBot %d] spawned, nav areas loaded: %u\n", entindex(),
        TheNavMesh ? (unsigned int)TheNavMesh->GetNavAreaCount() : 0u);
}

void CNPC_LuaBot::Event_Killed(const CTakeDamageInfo& info)
{
    BaseClass::Event_Killed(info);
    UTIL_Remove(this);
}

void CNPC_LuaBot::MoveTo(const Vector& vecGoal)
{
    m_vecGoal = vecGoal;
    m_mode = MODE_GOTO;
    m_repathTimer.Invalidate();   // построить путь сразу
}

void CNPC_LuaBot::Chase()
{
    m_mode = MODE_CHASE;
    m_repathTimer.Invalidate();
}

void CNPC_LuaBot::Stop()
{
    m_mode = MODE_IDLE;
    m_waypoints.RemoveAll();
    m_iWaypoint = 0;
}

// ---------------------------------------------------------------------------
// Путь по навмешу: A* по зонам, затем список точек (центры зон + сама цель)
// ---------------------------------------------------------------------------
bool CNPC_LuaBot::BuildPath(const Vector& vecGoal)
{
    m_waypoints.RemoveAll();
    m_iWaypoint = 0;

    if (!TheNavMesh)
        return false;

    CNavArea* pStart = TheNavMesh->GetNearestNavArea(GetAbsOrigin(), false, 300.0f, false, true);
    CNavArea* pGoal = TheNavMesh->GetNearestNavArea(vecGoal, false, 300.0f, false, true);
    if (!pStart || !pGoal)
        return false;

    if (pStart == pGoal)
    {
        m_waypoints.AddToTail(vecGoal);
        return true;
    }

    CNavArea* pClosest = NULL;
    LuaBotPathCost cost;
    bool bReached = NavAreaBuildPath(pStart, pGoal, &vecGoal, cost, &pClosest);

    // если цель недостижима, идём к ближайшей достижимой зоне
    CNavArea* pEnd = bReached ? pGoal : pClosest;
    if (!pEnd)
        return false;

    // путь восстанавливается от конца к началу по родителям зон
    CUtlVector<CNavArea*> areas;
    for (CNavArea* pArea = pEnd; pArea; pArea = pArea->GetParent())
        areas.AddToHead(pArea);

    // первая зона — та, где бот уже стоит, её центр пропускаем
    for (int i = 1; i < areas.Count(); i++)
        m_waypoints.AddToTail(areas[i]->GetCenter());

    if (bReached)
        m_waypoints.AddToTail(vecGoal);

    return m_waypoints.Count() > 0;
}

// ---------------------------------------------------------------------------
// Движение: шаг вперёд (со ступеньками и скольжением вдоль стен) + гравитация
// ---------------------------------------------------------------------------
void CNPC_LuaBot::MoveStep(const Vector& vecDelta, float dt)
{
    Vector pos = GetAbsOrigin();
    trace_t tr;

    // --- горизонтальный шаг: напрямую, затем скольжением по осям X и Y ---
    if (vecDelta.LengthSqr() > 0.0001f)
    {
        Vector attempts[3] = { vecDelta, Vector(vecDelta.x, 0, 0), Vector(0, vecDelta.y, 0) };

        for (int i = 0; i < 3; i++)
        {
            if (attempts[i].LengthSqr() < 0.0001f)
                continue;

            // поднимаемся на высоту ступеньки, идём вперёд
            UTIL_TraceEntity(this, pos, pos + Vector(0, 0, LUABOT_STEP_HEIGHT), MASK_NPCSOLID, &tr);
            Vector raised = tr.endpos;

            UTIL_TraceEntity(this, raised, raised + attempts[i], MASK_NPCSOLID, &tr);
            if (tr.fraction >= 1.0f && !tr.startsolid)
            {
                pos = tr.endpos;
                break;
            }
        }
    }

    // --- вертикаль: прилипаем к земле в пределах ступеньки, иначе падаем ---
    UTIL_TraceEntity(this, pos, pos - Vector(0, 0, LUABOT_STEP_HEIGHT * 2.0f + 1.0f), MASK_NPCSOLID, &tr);
    if (tr.startsolid)
    {
        // застряли в геометрии, не двигаем по вертикали
    }
    else if (tr.fraction < 1.0f && tr.plane.normal.z >= 0.7f)
    {
        pos = tr.endpos;
        m_flFallSpeed = 0.0f;
        SetGroundEntity(tr.m_pEnt);
    }
    else
    {
        m_flFallSpeed += LUABOT_GRAVITY * dt;
        UTIL_TraceEntity(this, pos, pos - Vector(0, 0, m_flFallSpeed * dt), MASK_NPCSOLID, &tr);
        pos = tr.endpos;
        if (tr.fraction < 1.0f)
            m_flFallSpeed = 0.0f;
        SetGroundEntity(NULL);
    }

    UTIL_SetOrigin(this, pos, true);
}

void CNPC_LuaBot::WalkToward(const Vector& vecTarget, float dt)
{
    Vector dir = vecTarget - GetAbsOrigin();
    dir.z = 0.0f;
    float flDist = dir.NormalizeInPlace();
    if (flDist < 0.01f)
        return;

    // плавный поворот лицом по ходу движения
    QAngle ang = GetAbsAngles();
    ang.y = UTIL_ApproachAngle(UTIL_VecToYaw(dir), ang.y, LUABOT_TURN_RATE * dt);
    SetAbsAngles(ang);

    float flStep = MIN(LUABOT_SPEED * dt, flDist);   // не перешагиваем цель
    m_vecPendingDelta = dir * flStep;
}

void CNPC_LuaBot::UpdateAnimation(bool bMoving)
{
    int seq = SelectWeightedSequence(bMoving ? ACT_WALK : ACT_IDLE);
    if (seq >= 0 && seq != GetSequence())
        ResetSequence(seq);

    StudioFrameAdvance();
}

// ---------------------------------------------------------------------------
// Главный цикл бота
// ---------------------------------------------------------------------------
void CNPC_LuaBot::BotThink()
{
    SetNextThink(gpGlobals->curtime + LUABOT_THINK);

    float dt = gpGlobals->curtime - m_flLastThink;
    m_flLastThink = gpGlobals->curtime;
    dt = clamp(dt, 0.001f, 0.1f);

    if (!IsAlive())
        return;

    m_vecPendingDelta = vec3_origin;
    bool bMoving = false;

    // --- выбор цели ---
    Vector vecTarget = vec3_origin;
    bool bHaveTarget = false;

    if (m_mode == MODE_GOTO)
    {
        Vector toGoal = m_vecGoal - GetAbsOrigin();
        toGoal.z = 0.0f;
        if (toGoal.LengthSqr() < Square(40.0f))
        {
            Stop();   // дошли
        }
        else
        {
            vecTarget = m_vecGoal;
            bHaveTarget = true;
        }
    }
    else if (m_mode == MODE_CHASE)
    {
        CBasePlayer* pNearest = FindNearestPlayer(GetAbsOrigin());
        if (pNearest)
        {
            vecTarget = pNearest->GetAbsOrigin();
            bHaveTarget = true;
        }
    }

    // --- движение по пути ---
    if (bHaveTarget)
    {
        if (m_repathTimer.IsElapsed())
        {
            m_repathTimer.Start(m_mode == MODE_CHASE ? 0.5f : 2.0f);

            if (!BuildPath(vecTarget))
            {
                DevMsg("[LuaBot %d] no path to target (nav areas loaded: %u)\n", entindex(),
                    TheNavMesh ? (unsigned int)TheNavMesh->GetNavAreaCount() : 0u);
            }
        }

        if (m_iWaypoint < m_waypoints.Count())
        {
            Vector toWp = m_waypoints[m_iWaypoint] - GetAbsOrigin();
            toWp.z = 0.0f;

            if (toWp.LengthSqr() < Square(24.0f))
            {
                m_iWaypoint++;    // точка достигнута
            }
            else
            {
                WalkToward(m_waypoints[m_iWaypoint], dt);
                bMoving = true;
            }
        }

        // не продвигаемся? пропускаем точку и пересчитываем путь
        if (bMoving && m_stuckTimer.IsElapsed())
        {
            m_stuckTimer.Start(1.5f);
            if (GetAbsOrigin().DistToSqr(m_vecLastProgressPos) < Square(10.0f))
            {
                m_iWaypoint++;
                m_repathTimer.Invalidate();
            }
            m_vecLastProgressPos = GetAbsOrigin();
        }
    }

    // применяем шаг (и гравитацию, даже если бот стоит)
    MoveStep(m_vecPendingDelta, dt);
    UpdateAnimation(bMoving);

    // --- хук для Lua (не чаще 10 раз в секунду) ---
    if (m_hookTimer.IsElapsed() && LuaManager() && LuaManager()->GetLuaHandle())
    {
        m_hookTimer.Start(0.1f);
        LuaManager()->CallPlayerHook("OnNPCThink", FindNearestPlayer(GetAbsOrigin()));
    }
}
