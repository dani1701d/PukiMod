#include "cbase.h"
#include "props.h"

// НОВАЯ ИСПРАВЛЕННАЯ ФУНКЦИЯ РАСЧЕТА ПОЗИЦИИ СПАВНА
Vector GetGModSpawnPosition(CBasePlayer* pPlayer, const char* modelName)
{
    Vector vecForward;
    AngleVectors(pPlayer->EyeAngles(), &vecForward);

    trace_t tr;
    UTIL_TraceLine(pPlayer->EyePosition(), pPlayer->EyePosition() + vecForward * 4000.0f,
        MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr);

    if (tr.fraction == 1.0f)
        return pPlayer->EyePosition() + vecForward * 150.0f;

    Vector spawnPos = tr.endpos;
    if (tr.plane.normal.z > 0.7f)
        spawnPos.z += 24.0f;
    else if (fabsf(tr.plane.normal.z) < 0.3f)
        spawnPos += tr.plane.normal * 20.0f;
    return spawnPos;
}

// НАША УНИВЕРСАЛЬНАЯ КОМАНДА СПАВНА
CON_COMMAND_F(spawn_item, "Universal spawn command for props, weapons, NPCs and vehicles", FCVAR_GAMEDLL | FCVAR_CLIENTCMD_CAN_EXECUTE)
{
    CBasePlayer* pPlayer = UTIL_GetCommandClient();
    if (!pPlayer) return;

    if (args.ArgC() < 3)
    {
        Msg("Использование: spawn_item <type> <class_or_model>\n");
        return;
    }

    const char* pType = args.Arg(1);      // "prop", "weapon", "npc", "vehicle"
    const char* pTarget = args.Arg(2);    // Имя класса или путь к модели

    // Вызываем расчет позиции, передавая путь к модели (для расчета хитбокса)
    Vector spawnPos = GetGModSpawnPosition(pPlayer, pTarget);

    // СЦЕНАРИЙ 1: Физический проп
    if (Q_strcmp(pType, "prop") == 0)
    {
        CBaseEntity::PrecacheModel(pTarget);
        CPhysicsProp* pProp = (CPhysicsProp*)CreateEntityByName("prop_physics");
        if (pProp)
        {
            pProp->SetModelName(AllocPooledString(pTarget));
            pProp->SetAbsOrigin(spawnPos);
            pProp->Spawn();

            // Будим физику, чтобы объект сразу упал, если заспавнился на неровной поверхности
            IPhysicsObject* pPhysObj = pProp->VPhysicsGetObject();
            if (pPhysObj) pPhysObj->Wake();
        }
    }
    // СЦЕНАРИЙ 2: Оружие или патроны
    else if (Q_strcmp(pType, "weapon") == 0)
    {
        CBaseEntity* pWeapon = CreateEntityByName(pTarget);
        if (pWeapon)
        {
            pWeapon->Precache();
            pWeapon->SetAbsOrigin(spawnPos + Vector(0, 0, 10));
            DispatchSpawn(pWeapon);
            pWeapon->Activate();
        }
    }
    // СЦЕНАРИЙ 3: Спавн NPC
    else if (Q_strcmp(pType, "npc") == 0)
    {
        CBaseEntity* pNPC = CreateEntityByName(pTarget);
        if (pNPC)
        {
            pNPC->SetAbsOrigin(spawnPos + Vector(0, 0, 5));

            QAngle faceAngles = pPlayer->GetAbsAngles();
            faceAngles.y += 180.0f; // Разворачиваем NPC лицом к игроку
            pNPC->SetAbsAngles(faceAngles);

            pNPC->Spawn();
            pNPC->Activate();
        }
    }
    // СЦЕНАРИЙ 4: Спавн транспорта (Машины)
    else if (Q_strcmp(pType, "vehicle") == 0)
    {
        const char* pModelPath = "models/airboat.mdl";
        const char* pScriptPath = "scripts/vehicles/airboat.txt";

        if (Q_strcmp(pTarget, "prop_vehicle_jeep") == 0)
        {
            pModelPath = "models/buggy.mdl";
            pScriptPath = "scripts/vehicles/jeep_test.txt";
            pTarget = "prop_vehicle_driveable";
        }

        CBaseEntity::PrecacheModel(pModelPath);

        CBaseEntity* pVehicle = CreateEntityByName(pTarget);
        if (pVehicle)
        {
            // Машину поднимаем повыше (на 25 юнитов), чтобы её колеса точно не застряли в геометрии земли
            pVehicle->SetAbsOrigin(spawnPos + Vector(0, 0, 25));
            pVehicle->SetAbsAngles(pPlayer->GetAbsAngles());

            pVehicle->KeyValue("model", pModelPath);
            pVehicle->KeyValue("VehicleScript", pScriptPath);

            pVehicle->Spawn();
            pVehicle->Activate();
        }
    }
    // СЦЕНАРИЙ 1: Физический проп
    if (Q_strcmp(pType, "ragdoll") == 0)
    {
        CBaseEntity::PrecacheModel(pTarget);
        CPhysicsProp* pProp = (CPhysicsProp*)CreateEntityByName("prop_ragdoll");
        if (pProp)
        {
            pProp->SetModelName(AllocPooledString(pTarget));
            pProp->SetAbsOrigin(spawnPos);
            pProp->Spawn();

            // Будим физику, чтобы объект сразу упал, если заспавнился на неровной поверхности
            IPhysicsObject* pPhysObj = pProp->VPhysicsGetObject();
            if (pPhysObj) pPhysObj->Wake();
        }
    }
}