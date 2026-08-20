#ifndef WEAPON_LUA_H
#define WEAPON_LUA_H
#pragma once

#include "basecombatweapon_shared.h"

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

    char m_szLuaClassName[64];
};

#endif // WEAPON_LUA_H