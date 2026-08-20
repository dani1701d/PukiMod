#include "cbase.h"
#include "basecombatweapon_shared.h"
#include "luamanager.h" // Подставь свой хидер менеджера Lua

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

// Простейшая сетевая таблица
IMPLEMENT_NETWORKCLASS_ALIASED(WeaponLua, DT_WeaponLua)
BEGIN_NETWORK_TABLE(CWeaponLua, DT_WeaponLua)
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeaponLua)
END_PREDICTION_DATA()

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

    // ХАК: Задаем дефолтную модель, если Lua еще не подгрузился
    SetModel("models/weapons/w_pistol.mdl");
}

// Самое интересное: Перехват выстрела!
void CWeaponLua::PrimaryAttack()
{
    // Получаем глобальное состояние Lua из твоего менеджера
    lua_State* L = g_pLuaManager->GetState();

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