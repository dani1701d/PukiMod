#include "cbase.h"
#include "ai_basenpc.h"
#include "luamanager.h"

// Наследуемся от базового класса NPC в Source
class CNPC_LuaBot : public CAI_BaseNPC
{
    DECLARE_CLASS(CNPC_LuaBot, CAI_BaseNPC);
public:
    void Precache(void) override
    {
        // Прекешим спрайт/модель, чтобы игра не вылетала
        PrecacheModel("models/error.mdl");
        BaseClass::Precache();
    }

    void CNPC_LuaBot::Spawn(void)
    {
        Precache();
        SetModel("models/error.mdl");

        SetHullType(HULL_HUMAN);
        SetHullSizeNormal();

        SetSolid(SOLID_BBOX);
        AddSolidFlags(FSOLID_NOT_STANDABLE);

        // ВАЖНО: Указываем тип передвижения "Шаг" и включаем гравитацию
        SetMoveType(MOVETYPE_STEP);

        m_bloodColor = BLOOD_COLOR_RED;
        m_iHealth = 100;

        // ВАЖНО: Даем ИИ способность передвигаться по земле
        CapabilitiesAdd(bits_CAP_MOVE_GROUND);

        BaseClass::Spawn();

        // ПРИНУДИТЕЛЬНО РАЗБУДИМ ИИ ПОСЛЕ СПАВНА
        ClearCondition(COND_SEE_ENEMY);
        SetState(NPC_STATE_ALERT); // Переводим бота в состояние тревоги, чтобы он начал думать
    }

    // Логика ИИ (вызывается движком каждый кадр)
    void CNPC_LuaBot::RunAI(void)
    {
        BaseClass::RunAI();

        if (LuaManager() && LuaManager()->GetLuaHandle())
        {
            // Находим первого игрока на сервере (на локальном сервере это вы)
            CBasePlayer* pTargetPlayer = UTIL_PlayerByIndex(1);

            if (pTargetPlayer)
            {
                // Передаем игрока в ваш работающий CallPlayerHook
                // В Lua этот хук сработает и передаст объект этого игрока
                LuaManager()->CallHook("OnNPCThink", pTargetPlayer);
            }
        }
    }
};

LINK_ENTITY_TO_CLASS(npc_luabot, CNPC_LuaBot);