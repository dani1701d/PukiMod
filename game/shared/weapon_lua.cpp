#include "cbase.h"
#include "basecombatweapon_shared.h"
#include "luamanager.h" // Подставь свой хидер менеджера Lua

extern CLuaManager* g_pLuaManager;

void Lua_PushEntity(lua_State* L, CBaseEntity* pEntity);

class CWeaponLua : public CBaseCombatWeapon
{
public:
    DECLARE_CLASS(CWeaponLua, CBaseCombatWeapon);
    DECLARE_NETWORKCLASS();
    DECLARE_PREDICTABLE();

    CWeaponLua();

    virtual void Spawn();
    virtual void Precache();
    virtual void PrimaryAttack();
    virtual void SecondaryAttack();

    // Имя Lua-скрипта, который управляет этой пушкой (например, "weapon_pistol")
    char m_szLuaClassName[64];
};

// ============================================================================
// СЕТЕВАЯ ТАБЛИЦА (Универсальный пустой shared-вариант)
// ============================================================================
#ifdef CLIENT_DLL

    // Короткое объявление пустой клиентской таблицы
IMPLEMENT_CLIENTCLASS_DT(CWeaponLua, DT_WeaponLua, CWeaponLua)
END_RECV_TABLE()

BEGIN_PREDICTION_DATA(CWeaponLua)
END_PREDICTION_DATA()

#else

    // Короткое объявление пустой серверной таблицы
IMPLEMENT_SERVERCLASS_ST(CWeaponLua, DT_WeaponLua)
END_SEND_TABLE()

#endif
// ============================================================================

LINK_ENTITY_TO_CLASS(weapon_lua, CWeaponLua);
PRECACHE_WEAPON_REGISTER(weapon_lua);

CWeaponLua::CWeaponLua()
{
    // По умолчанию имя Lua-класса пустое
    Q_strncpy(m_szLuaClassName, "default", sizeof(m_szLuaClassName));
}

void CWeaponLua::Precache()
{
    BaseClass::Precache();
    // Тут можно прекешить дефолтные звуки
}

void CWeaponLua::Spawn()
{
    BaseClass::Spawn();

    // Получаем имя энтити, под которым её заспавнили (например, "weapon_molotov")
    const char* pszClassname = GetClassname();
    if (pszClassname)
    {
        Q_strncpy(m_szLuaClassName, pszClassname, sizeof(m_szLuaClassName));
    }

    // Вместо жесткого хардкода модели пистолета
    // Мы заставляем движок прочитать файл scripts/имя_оружия.txt
    // Движок сам возьмет оттуда и v_model, и w_model!
    WEAPON_FILE_INFO_HANDLE hWFI = LookupWeaponInfoSlot(m_szLuaClassName);
    if (hWFI != GetInvalidWeaponInfoHandle())
    {
        FileWeaponInfo_t* pWeaponInfo = GetFileWeaponInfoFromHandle(hWFI);
        if (pWeaponInfo)
        {
            if (pWeaponInfo->szWorldModel[0] != '\0')
                SetModel(pWeaponInfo->szWorldModel);
        }
    }
}

// Самое интересное: Перехват выстрела!
void CWeaponLua::PrimaryAttack()
{
    // Получаем глобальное состояние Lua из твоего менеджера
    lua_State* L = g_pLuaManager->GetLuaHandle();

    // Ищем в глобальной таблице Lua функцию с выстрелом для этой пушки
    // Например: weapon_lua_primary( weapon_entity )
    lua_getglobal(L, "OnLuaWeaponPrimary");
    if (lua_isfunction(L, -1))
    {
        // Передаем саму пушку в Lua, чтобы скрипт знал, кто стреляет
        // (Для этого можно использовать Lua_PushEntity, которую мы писали ранее!)
        Lua_PushEntity(L, this);

        // Вызываем функцию с 1 аргументом и 0 возвращаемых значений
        if (lua_pcall(L, 1, 0, 0) != 0)
        {
            Warning("Lua Error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    // Обновляем время следующей атаки, чтобы не стрелять каждый кадр
    m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
}

void CWeaponLua::SecondaryAttack() {}