#include "cbase.h"
#include "luamanager.h"
#include "filesystem.h"
#include "interface.h"
#include "engine/IEngineSound.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "weapon_lua.h"

#ifndef CLIENT_DLL
#include "networkstringtable_gamedll.h"
#include "entitylist.h"
#endif

#ifdef CLIENT_DLL
#include "networkstringtable_clientdll.h"
#endif

#include "igamesystem.h"
#include <string>

// Уникальное имя нашей метаблицы для проверки типов в Lua
#define PLAYER_META "SourcePlayer"
#define ENTITY_META "Entity"

CLuaManager* g_pLuaManager = nullptr;

// ========================================== Глобальный код ==========================================

#ifndef CLIENT_DLL
int LuaPlayer_ConCommand(lua_State* L)
{
    // 1. Достаем игрока из userdata (первый аргумент при вызове через ":")
    // (Замени "Entity" или "Player" на то имя, под которым у тебя привязан userdata игрока)
    CBasePlayer* pPlayer = (CBasePlayer*)luaL_checkudata(L, 1, "Player");

    // 2. Получаем саму команду из второго аргумента
    const char* pCommand = luaL_checkstring(L, 2);

    if (pPlayer && pCommand && engine)
    {
        // Достаем эдикт игрока, который так требовал серверный движок!
        edict_t* pEdict = pPlayer->edict();
        if (pEdict)
        {
            // Сервер заставляет конкретно этого игрока выполнить клиентскую команду
            engine->ClientCommand(pEdict, pCommand);
        }
    }
    return 0;
}

int Lua_ServerCommand(lua_State* L)
{
    const char* pCommand = luaL_checkstring(L, 1);
    if (pCommand && engine)
    {
        // Добавляем принудительный перенос строки \n, чтобы движок понял, что команда отправлена
        CUtlString safeCmd;
        safeCmd.Format("%s\n", pCommand);

        engine->ServerCommand(safeCmd.Get());
    }
    return 0;
}
#endif

int LuaWeapon_GetClassname(lua_State* L)
{
    CBaseEntity* pEnt = *(CBaseEntity**)luaL_checkudata(L, 1, "Entity");
    CWeaponLua* pWeapon = dynamic_cast<CWeaponLua*>(pEnt);
    if (pWeapon) {
        pWeapon->GetClassname();
        return 1;
    }
    return 0;
}

#ifndef CLIENT_DLL
// Регистрируем кастомное имя оружия в фабрике Source Engine
int Lua_RegisterWeapon(lua_State* L)
{
    const char* pszCustomName = luaL_checkstring(L, 1);
    if (!pszCustomName) return 0;

    IEntityFactoryDictionary* pDict = EntityFactoryDictionary();
    if (pDict)
    {
        IEntityFactory* pBaseFactory = pDict->FindFactory("weapon_lua");
        if (pBaseFactory && !pDict->FindFactory(pszCustomName))
        {
            // 1. Регистрируем имя в фабрике энтити
            pDict->InstallFactory( pBaseFactory, pszCustomName );
            
            LookupWeaponInfoSlot(pszCustomName);

            Msg("Lua Weapon Registered & Precached: %s\n", pszCustomName);
        }
    }
    return 0;
}
#endif

// Функция для выстрела стандартными пулями Source Engine
int LuaWeapon_ShootBullets(lua_State* L) {
    // Первым делом достаем энтити оружия из метатаблицы "Entity"
    CBaseEntity* pEnt = *(CBaseEntity**)luaL_checkudata(L, 1, "Entity");
    int iDamage = luaL_checkinteger(L, 2);   // Урон
    int iShots = luaL_checkinteger(L, 3);    // Количество пуль (дробь)
    float flCone = luaL_checknumber(L, 4);   // Разброс (например, 0.02f)

    CWeaponLua* pWeapon = dynamic_cast<CWeaponLua*>(pEnt);
    if (pWeapon) {
        CBasePlayer* pOwner = ToBasePlayer(pWeapon->GetOwner());
        if (pOwner) {
            // Играем стандартный звук выстрела пушки
            pWeapon->WeaponSound(SINGLE);

            // Стреляем через стандартную структуру движка
            FireBulletsInfo_t info;
            info.m_iShots = iShots;
            info.m_vecSrc = pOwner->Weapon_ShootPosition();
            info.m_vecDirShooting = pOwner->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
            info.m_vecSpread = Vector(flCone, flCone, flCone);
            info.m_flDistance = MAX_COORD_RANGE;
            info.m_iAmmoType = pWeapon->GetPrimaryAmmoType();
            info.m_flDamage = (float)iDamage;

            pOwner->FireBullets(info);

            // Добавляем визуальную отдачу игроку
            pOwner->ViewPunch(QAngle(random->RandomFloat(-2, -1), random->RandomFloat(-1, 1), 0));
        }
    }
    return 0;
}

// Функция-помощник для создания и передачи энтити в Lua
void Lua_PushEntity(lua_State* L, CBaseEntity* pEntity) {
    if (!pEntity) {
        lua_pushnil(L);
        return;
    }
    // Создаем userdata, которая будет хранить указатель на энтити
    CBaseEntity** udata = (CBaseEntity**)lua_newuserdata(L, sizeof(CBaseEntity*));
    *udata = pEntity;

    // Привязываем метатаблицу ENTITY_META к этому объекту
    luaL_getmetatable(L, ENTITY_META);
    lua_setmetatable(L, -2);
}

// 1. Применение линейной силы (Импульса) к центру масс объекта
int LuaEntity_ApplyForce(lua_State* L) {
    // Достаем энтити из первого аргумента (self)
    CBaseEntity* pEntity = *(CBaseEntity**)luaL_checkudata(L, 1, ENTITY_META);

    // Получаем вектор силы (X, Y, Z) из аргументов Lua
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);

    if (pEntity) {
        // Достаем физический объект энтити
        IPhysicsObject* pPhys = pEntity->VPhysicsGetObject();
        if (pPhys) {
            Vector vecForce(x, y, z);
            // Применяем импульс силы к объекту
            pPhys->ApplyForceCenter(vecForce);
        }
    }
    return 0;
}

// 2. Применение вращательного импульса (Torque / Крутящий момент)
int LuaEntity_ApplyTorque(lua_State* L) {
    CBaseEntity* pEntity = *(CBaseEntity**)luaL_checkudata(L, 1, ENTITY_META);
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    float z = luaL_checknumber(L, 4);

    if (pEntity) {
        IPhysicsObject* pPhys = pEntity->VPhysicsGetObject();
        if (pPhys) {
            AngularImpulse angTorque(x, y, z);
            // Применяем вращающий импульс
            pPhys->ApplyTorqueCenter(angTorque);
        }
    }
    return 0;
}

// 3. Получение текущей позиции энтити в мире
int LuaEntity_GetPos(lua_State* L) {
    CBaseEntity* pEntity = *(CBaseEntity**)luaL_checkudata(L, 1, ENTITY_META);
    if (pEntity) {
        Vector pos = pEntity->GetAbsOrigin();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3; // Возвращаем 3 числа в Lua
    }
    return 0;
}

int Lua_Msg(lua_State* L)
{
    // Проверяем, передал ли пользователь строку в качестве первого аргумента (индекс 1)
    if (lua_isstring(L, 1))
    {
        const char* text = lua_tostring(L, 1); // Получаем строку из стека Lua
        Msg("%s", text);                       // Выводим в стандартную консоль Source
    }
    return 0; // Возвращаем 0, так как функция ничего не возвращает обратно в Lua
}

int Lua_Warning(lua_State* L)
{
    // Проверяем, передал ли пользователь строку в качестве первого аргумента (индекс 1)
    if (lua_isstring(L, 1))
    {
        const char* text = lua_tostring(L, 1); // Получаем строку из стека Lua
        Warning("%s", text);                       // Выводим в стандартную консоль Source
    }
    return 0; // Возвращаем 0, так как функция ничего не возвращает обратно в Lua
}

int Lua_Error(lua_State* L)
{
    // Проверяем, передал ли пользователь строку в качестве первого аргумента (индекс 1)
    if (lua_isstring(L, 1))
    {
        const char* text = lua_tostring(L, 1); // Получаем строку из стека Lua
        Error("%s", text);                       // Выводим в стандартную консоль Source
    }
    return 0; // Возвращаем 0, так как функция ничего не возвращает обратно в Lua
}

// Наш C++ коллбэк, который будет вызываться движком при вводе команды
void LuaCommandCallback(const CCommand& args)
{
    if (LuaManager()->GetLuaHandle())
    {
        lua_State* L = LuaManager()->GetLuaHandle();

        // args.Arg(0) — это имя самой команды (например, "lua_superman")
        const char* cmdName = args.Arg(0);

        // Ищем в Lua глобальную функцию по имени команды
        lua_getglobal(L, cmdName);

        if (lua_isfunction(L, -1))
        {
            // Передаем аргументы, которые ввели ПОСЛЕ команды (если они есть)
            lua_pushstring(L, args.ArgS());

            if (lua_pcall(L, 1, 0, 0) != 0)
            {
                Warning("Lua Command Error (%s): %s\n", cmdName, lua_tostring(L, -1));
            }
        }
        else
        {
            // Если функция не найдена, очищаем стек
            lua_pop(L, 1);
        }
    }
}

// Сама функция, которую мы отдадим в Lua
int Lua_RegisterConCommand(lua_State* L)
{
    if (lua_isstring(L, 1))
    {
        const char* cmdName = lua_tostring(L, 1);

        // Создаем официальную команду движка Source Engine
        // Используем макрос и выделяем память под имя команды
        new ConCommand(strdup(cmdName), LuaCommandCallback, "Custom Lua Command");
    }
    return 0;
}

// Вспомогательная функция: оборачивает C++ указатель CBasePlayer в объект Lua Userdata
void PushPlayerObject(lua_State* L, CBasePlayer* pPlayer)
{
    if (!pPlayer)
    {
        lua_pushnil(L);
        return;
    }

    // Выделяем память в Lua под указатель (Userdata)
    CBasePlayer** udata = (CBasePlayer**)lua_newuserdata(L, sizeof(CBasePlayer*));
    *udata = pPlayer;

    // Привязываем метаблицу к этому userdata, чтобы у него появились методы
    luaL_getmetatable(L, PLAYER_META);
    lua_setmetatable(L, -2);
}

// ========================================== Серверный код ==========================================

#ifndef CLIENT_DLL

#include "ai_basenpc.h"     
#include "ai_navigator.h"   
#include "ai_motor.h"

// Вспомогательная функция: безопасно достает C++ указатель из объекта Lua
CBasePlayer* GetPlayerFromLua(lua_State* L, int index)
{
    CBasePlayer** udata = (CBasePlayer**)luaL_checkudata(L, index, PLAYER_META);
    return (udata != NULL) ? *udata : NULL;
}

// Вспомогательный метод: собирает данные попадания луча в удобную таблицу для Lua
void PushTraceToLua(lua_State* L, const trace_t& tr)
{
    lua_newtable(L); // Создаем таблицу в Lua {}

    // 1. Записываем координаты HitPos
    lua_pushstring(L, "HitPos");
    lua_newtable(L);
    lua_pushnumber(L, tr.endpos.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, tr.endpos.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, tr.endpos.z); lua_setfield(L, -2, "z");
    lua_settable(L, -3);

    // 2. Записываем нормаль поверхности HitNormal (куда смотрит стена/пол)
    lua_pushstring(L, "HitNormal");
    lua_newtable(L);
    lua_pushnumber(L, tr.plane.normal.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, tr.plane.normal.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, tr.plane.normal.z); lua_setfield(L, -2, "z");
    lua_settable(L, -3);

    // 3. Записываем индекс объекта, в который попали
    lua_pushstring(L, "EntityIndex");
    if (tr.m_pEnt)
        lua_pushinteger(L, tr.m_pEnt->entindex());
    else
        lua_pushinteger(L, 0);
    lua_settable(L, -3);
}

// Функция 1: Создание физического объекта (prop_physics)
// Использование в Lua: SpawnPhysicsProp(x, y, z, "models/props_borealis/bluebarrel001.mdl")
int Lua_SpawnPhysicsProp(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);
    const char* modelPath = luaL_checkstring(L, 4);

    // Обязательно прекешим модель в движке, иначе будет вылет или невидимость
    CBaseEntity::PrecacheModel(modelPath);

    CBaseEntity* pProp = CreateEntityByName("prop_physics");
    if (pProp)
    {
        pProp->SetAbsOrigin(Vector(x, y, z));

        // Устанавливаем путь к модели через ключевое поле (KeyValue) движка
        pProp->KeyValue("model", modelPath);

        // Настраиваем тип физического взаимодействия по умолчанию
        pProp->KeyValue("physicsmode", "0");
        pProp->Spawn();
        pProp->Activate();

        pProp->NetworkStateChanged();

        Lua_PushEntity(L, pProp);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// Функция 2: Создание динамического объекта (prop_dynamic)
// Использование в Lua: SpawnDynamicProp(x, y, z, "models/combine_turrets/floor_turret.mdl")
int Lua_SpawnDynamicProp(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);
    const char* modelPath = luaL_checkstring(L, 4);

    CBaseEntity::PrecacheModel(modelPath);

    CBaseEntity* pProp = CreateEntityByName("prop_dynamic");
    if (pProp)
    {
        pProp->SetAbsOrigin(Vector(x, y, z));
        pProp->KeyValue("model", modelPath);

        // Настраиваем коллизию (Solid = 6 означает использование физики самой модели)
        pProp->KeyValue("solid", "6");
        pProp->Spawn();
        pProp->Activate();

        pProp->NetworkStateChanged();

        lua_pushnumber(L, pProp->entindex());
        return 1;
    }
    return 0;
}

// Функция 1: Проиграть пространственный 3D-звук из энтити (игрока или NPC)
// Использование в Lua: EmitEntitySound(botID, "npc/zombie/zo_attack1.wav")
int Lua_EmitEntitySound(lua_State* L)
{
    int entIndex = luaL_checkinteger(L, 1);
    const char* soundPath = luaL_checkstring(L, 2);

    enginesound->PrecacheSound(soundPath, true);

    CBaseEntity* pEnt = CBaseEntity::Instance(entIndex);
    if (pEnt)
    {
        // Создаем фильтр "все игроки на сервере"
        CPASFilter filter;
        filter.AddAllPlayers();
        filter.MakeReliable(); // Делаем доставку звука по сети обязательной

        // Используем глобальную C++ функцию движка для принудительного вещания 3D звука
        CBaseEntity::EmitSound(filter, pEnt->entindex(), soundPath);
    }
    return 0;
}

// Функция 2: Проиграть 2D-звук глобально для конкретного игрока (например, музыку в уши)
// Использование в Lua: player:PlaySound("ui/gamestart.wav")
int LuaPlayer_PlaySound(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    const char* soundPath = luaL_checkstring(L, 2);

    if (pPlayer)
    {
        // Форматируем команду в виде строки, например: "play common/wpn_hudon.wav"
        char cmdBuffer[256];
        Q_snprintf(cmdBuffer, sizeof(cmdBuffer), "play %s", soundPath);

        // Отправляем команду на выполнение клиенту этого конкретного игрока
        engine->ClientCommand(pPlayer->edict(), cmdBuffer);
    }
    return 0;
}

int Lua_PlaySoundAtCoordinates(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);
    const char* soundPath = luaL_checkstring(L, 4);

    // 1. Обязательный прекеш звука на сервере
    enginesound->PrecacheSound(soundPath, true);

    // 2. Создаем вектор позиции для звука
    Vector soundTargetPos(x, y, z);

    // 3. Создаем стандартный сетевой фильтр слышимости для этой точки (PAS)
    CPASFilter filter(soundTargetPos);
    filter.MakeReliable(); // Гарантирует доставку звукового пакета клиентам

    // 4. Используем родную статическую функцию EmitSound самого движка Source.
    // Передавая индекс 1 (индекс энтити мира/карты), мы заставляем звук 
    // воспроизводиться в конкретной точке пространства в обход жестких проверок IEngineSound.
    CBaseEntity::EmitSound(filter, 1, soundPath, &soundTargetPos);


    return 0;
}

int Lua_CreateNextBot(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float z = (float)luaL_checknumber(L, 3);

    Msg("[C++] Attempting to spawn npc_luabot at coordinates: %.1f, %.1f, %.1f\n", x, y, z);

    CBaseEntity* pEntity = CreateEntityByName("npc_luabot");
    if (pEntity)
    {
        pEntity->SetAbsOrigin(Vector(x, y, z));

        CAI_BaseNPC* pNPC = pEntity->MyNPCPointer();
        if (pNPC)
        {
            // ПРИНУДИТЕЛЬНО задаем базовую заглушку-модель перед спавном,
            // чтобы движок не удалил энтити из-за отсутствия физического тела
            pNPC->SetModelName(AllocPooledString("models/error.mdl"));

            pNPC->Spawn();
            pNPC->Activate(); // Активируем энтити в игровом мире

            int newID = pNPC->entindex();
            Msg("[C++] Success! npc_luabot spawned with Entity Index: %d\n", newID);

            lua_pushnumber(L, newID);
            return 1;
        }
    }

    Warning("[C++] Critical: Failed to create npc_luabot entity!\n");
    return 0;
}

// Заставить NPC бежать к определенной точке
// Использование в Lua: NPCMoveTo(botID, targetX, targetY, targetZ)
int Lua_NPCMoveTo(lua_State* L)
{
    int npcIndex = luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    CBaseEntity* pEnt = CBaseEntity::Instance(npcIndex);
    if (!pEnt) return 0;

    CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
    if (pNPC && pNPC->GetMotor())
    {
        Vector target(x, y, z);

        // 1. Поворачиваем лицо бота в сторону цели движения
        pNPC->GetMotor()->SetIdealYawToTarget(target);
        pNPC->GetMotor()->UpdateYaw();

        // 2. Принудительно двигаем физическое тело бота вперед в этом кадре
        // Передаем скорость (например, 200) и тип анимации ACT_RUN
        pNPC->GetMotor()->MoveGroundStep(target, pNPC->GetGoalEnt(), ACT_RUN, true);
    }
    return 0;
}

int Lua_SetBotSprite(lua_State* L)
{
    int npcIndex = luaL_checkinteger(L, 1);
    const char* spritePath = luaL_checkstring(L, 2);

    CBaseEntity* pEnt = CBaseEntity::Instance(npcIndex);
    if (!pEnt) return 0;

    CAI_BaseNPC* pNPC = pEnt->MyNPCPointer();
    if (pNPC)
    {
        // 1. Прячем стандартную 3D-модель (error.mdl), делая её невидимой
        pNPC->AddEffects(EF_NODRAW);

        // 2. Создаем встроенную энтити-спрайт движка Source
        CBaseEntity* pSprite = CreateEntityByName("env_sprite");
        if (pSprite)
        {
            // Настраиваем спрайт
            pSprite->SetModelName(AllocPooledString(spritePath));
            pSprite->SetAbsOrigin(pNPC->GetAbsOrigin());

            // Заставляем спрайт намертво прикрепиться к телу нашего NPC
            pSprite->SetParent(pNPC);
            pSprite->Spawn();

            // Настраиваем параметры рендеринга (обычный прозрачный спрайт)
            pSprite->m_nRenderMode = kRenderTransAlpha;
            pSprite->m_nRenderFX = kRenderFxNone;
            pSprite->m_clrRender.SetR(255);
            pSprite->m_clrRender.SetG(255);
            pSprite->m_clrRender.SetB(255);
            pSprite->m_clrRender.SetA(255);
        }
    }
    return 0;
}

int Lua_GetLocalPlayer(lua_State* L)
{
    CBasePlayer* pPlayer = UTIL_PlayerByIndex(1); // Игрок-хост на локальном сервере
    PushPlayerObject(L, pPlayer);
    return 1; // Возвращаем 1 объект игрока в Lua
}

// Функция нанесения урона или убийства всех игроков (для примера взаимодействия с миром)
// Использование в Lua: KillAllPlayers()
int Lua_KillAllPlayers(lua_State* L)
{
    // Цикл по всем сущностям игроков в Source Engine
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
        if (pPlayer && pPlayer->IsAlive())
        {
            pPlayer->CommitSuicide(); // Убиваем игрока средствами движка
        }
    }
    return 0;
}



// Метод: player:GiveItem("weapon_357")
int LuaPlayer_GiveItem(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    const char* weaponClass = luaL_checkstring(L, 2); // 2-й аргумент — класс оружия

    if (pPlayer)
    {
        pPlayer->GiveNamedItem(weaponClass);
    }
    return 0;
}

// Метод: player:Kill()
int LuaPlayer_Kill(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);

    if (pPlayer)
    {
        pPlayer->CommitSuicide();
    }
    return 0;
}

// Метод: player:SetSpeed(450)
int LuaPlayer_GetSpeed(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);

    if (pPlayer)
    {
        pPlayer->GetPlayerMaxSpeed();
        return 1;
    }
    return 0;
}

// Метод: player:SetSpeed(450)
int LuaPlayer_SetSpeed(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float newSpeed = (float)luaL_checknumber(L, 2); // 2-й аргумент — скорость

    if (pPlayer)
    {
        pPlayer->SetMaxSpeed(newSpeed);
    }
    return 0;
}

// Метод: player:GetHealth()
int LuaPlayer_GetHealth(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1); // 1-й аргумент — это сам player (self)
    if (pPlayer)
    {
        lua_pushnumber(L, pPlayer->GetHealth());
        return 1;
    }
    return 0;
}

// Метод: player:SetHealth(100)
int LuaPlayer_SetHealth(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    int newHealth = luaL_checkinteger(L, 2); // 2-й аргумент — новое здоровье

    if (pPlayer)
    {
        pPlayer->SetHealth(newHealth);
    }
    return 0;
}

// Метод: player:GetPos()
// Возвращает три числа (X, Y, Z) — координаты игрока
int LuaPlayer_GetPos(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    if (pPlayer)
    {
        Vector pos = pPlayer->GetAbsOrigin(); // Получаем позицию из движка
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        return 3; // Возвращаем 3 значения на стек Lua
    }
    return 0;
}

// Метод: player:SetPos(x, y, z)
// Телепортирует игрока на указанные координаты
int LuaPlayer_SetPos(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    if (pPlayer)
    {
        Vector targetPos(x, y, z);
        pPlayer->Teleport(&targetPos, NULL, NULL); // Телепортация средствами Source
    }
    return 0;
}

// Метод: player:SetGravity(0.5) -- 50% от обычной гравитации
int LuaPlayer_SetGravity(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float gravityScale = (float)luaL_checknumber(L, 2);

    if (pPlayer)
    {
        pPlayer->SetGravity(gravityScale); // Меняет гравитацию конкретно для этого игрока
    }
    return 0;
}

// Метод: player:StripWeapons() -- Отобрать всё оружие
int LuaPlayer_StripWeapons(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    if (pPlayer)
    {
        pPlayer->RemoveAllItems(true); // Очищает инвентарь игрока
    }
    return 0;
}

// Метод: player:GetName()
// Использование в Lua: local name = player:GetName()
int LuaPlayer_GetName(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    if (pPlayer)
    {
        // GetPlayerName() возвращает имя игрока из его сетевого профиля
        lua_pushstring(L, pPlayer->GetPlayerName());
        return 1;
    }
    return 0;
}

// Метод: player:SetTeam(TEAM_COMBINE)
// Использование в Lua: player:SetTeam(2)
int LuaPlayer_SetTeam(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    int teamIndex = luaL_checkinteger(L, 2);

    if (pPlayer)
    {
        pPlayer->ChangeTeam(teamIndex); // Меняем команду средствами Source Engine
    }
    return 0;
}

// Метод: player:GetTeam()
// Использование в Lua: if player:GetTeam() == 2 then ...
int LuaPlayer_GetTeam(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    if (pPlayer)
    {
        lua_pushnumber(L, pPlayer->GetTeamNumber());
        return 1;
    }
    return 0;
}

// Метод: player:IsOnGround()
// Использование в Lua: if player:IsOnGround() then ...
int LuaPlayer_IsOnGround(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    if (pPlayer)
    {
        // Проверяем флаги игрока в движке (битовая маска FL_ONGROUND)
        bool onGround = (pPlayer->GetFlags() & FL_ONGROUND) != 0;
        lua_pushboolean(L, onGround);
        return 1;
    }
    return 0;
}

// Метод: player:ChatPrint("Сообщение")
// Использование в Lua: player:ChatPrint("Welcome to the server!")
int LuaPlayer_ChatPrint(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    const char* message = luaL_checkstring(L, 2);

    if (pPlayer)
    {
        // ClientPrint отправляет текстовое уведомление прямо в чат/экран игрока
        // Используем hud_printtalk для вывода в игровой чат
        ClientPrint(pPlayer, HUD_PRINTTALK, message);
    }
    return 0;
}

#endif

void RegisterLuaFolder_R(INetworkStringTable* pTable, const char* pszCurrentPath)
{
    char szSearchPath[MAX_PATH];
    // Формируем маску поиска (например, "lua/addons/client/*.lua" или "lua/addons/client/my_addon/*.lua")
    Q_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.lua", pszCurrentPath);

    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirst(szSearchPath, &findHandle);

    // 1. Сначала ищем и регистрируем все .lua файлы в текущей папке
    while (pFileName)
    {
        if (pFileName[0] != '.') // Игнорируем "." и ".."
        {
            char szFullFilePath[MAX_PATH];
            Q_snprintf(szFullFilePath, sizeof(szFullFilePath), "%s/%s", pszCurrentPath, pFileName);

            // Добавляем файл в таблицу закачек движка
            pTable->AddString(false, szFullFilePath);
            Msg("[Lua-Server] Registered for download: %s\n", szFullFilePath);
        }
        pFileName = filesystem->FindNext(findHandle);
    }
    filesystem->FindClose(findHandle);

    // 2. Теперь ищем подпапки внутри текущей папки, чтобы зайти в них рекурсивно
    char szSubFolderSearch[MAX_PATH];
    Q_snprintf(szSubFolderSearch, sizeof(szSubFolderSearch), "%s/*", pszCurrentPath);

    const char* pSubFolderName = filesystem->FindFirst(szSubFolderSearch, &findHandle);
    while (pSubFolderName)
    {
        // Проверяем, что это папка, а не файл, и игнорируем "." / ".."
        if (pSubFolderName[0] != '.' && filesystem->FindIsDirectory(findHandle))
        {
            char szNextFolderPath[MAX_PATH];
            Q_snprintf(szNextFolderPath, sizeof(szNextFolderPath), "%s/%s", pszCurrentPath, pSubFolderName);

            // Шагаем глубже в подпапку
            RegisterLuaFolder_R(pTable, szNextFolderPath);
        }
        pSubFolderName = filesystem->FindNext(findHandle);
    }
    filesystem->FindClose(findHandle);
}

// Главная функция, которую ты вызываешь при старте сервера (в game_init)
void ForceDownloadClientAddons()
{
    INetworkStringTable* pDownloadablesTable = networkstringtable->FindTable("downloadables");
    if (!pDownloadablesTable)
    {
        Warning("Downloadables table not found! Cannot sync Lua files.\n");
        return;
    }

    // Запускаем рекурсивный обход с нашей корневой клиентской папки
    RegisterLuaFolder_R(pDownloadablesTable, "lua/addons/client");
}

void ForceDownloadServerAddons()
{
    INetworkStringTable* pDownloadablesTable = networkstringtable->FindTable("downloadables");
    if (!pDownloadablesTable)
    {
        Warning("Downloadables table not found! Cannot sync Lua files.\n");
        return;
    }

    // Запускаем рекурсивный обход с нашей корневой клиентской папки
    RegisterLuaFolder_R(pDownloadablesTable, "lua/addons/server");
}

// Рекурсивный запуск клиентских скриптов
void IncludeClientFolder_R(const char* pszCurrentPath)
{
    char szSearchPath[MAX_PATH];
    Q_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.lua", pszCurrentPath);

    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirst(szSearchPath, &findHandle);

    // 1. Запускаем файлы в текущей папке
    while (pFileName)
    {
        if (pFileName[0] != '.')
        {
            char szFullFilePath[MAX_PATH];
            Q_snprintf(szFullFilePath, sizeof(szFullFilePath), "%s/%s", pszCurrentPath, pFileName);

            // ТУТ ТВOЙ КОД ЗАПУСКА LUA. Например:
            // MyLuaState->ExecuteFile(szFullFilePath);

            Msg("[Lua-Client] Executing addon file: %s\n", szFullFilePath);
        }
        pFileName = filesystem->FindNext(findHandle);
    }
    filesystem->FindClose(findHandle);

    // 2. Идем вглубь по подпапкам аддонов
    char szSubFolderSearch[MAX_PATH];
    Q_snprintf(szSubFolderSearch, sizeof(szSubFolderSearch), "%s/*", pszCurrentPath);

    const char* pSubFolderName = filesystem->FindFirst(szSubFolderSearch, &findHandle);
    while (pSubFolderName)
    {
        if (pSubFolderName[0] != '.' && filesystem->FindIsDirectory(findHandle))
        {
            char szNextFolderPath[MAX_PATH];
            Q_snprintf(szNextFolderPath, sizeof(szNextFolderPath), "%s/%s", pszCurrentPath, pSubFolderName);

            IncludeClientFolder_R(szNextFolderPath);
        }
        pSubFolderName = filesystem->FindNext(findHandle);
    }
    filesystem->FindClose(findHandle);
}

// Вызывать при полной готовности клиента на карте
void LoadDownloadedClientAddons()
{
    Msg("[Lua-Client] Loading downloaded addons...\n");
    IncludeClientFolder_R("lua/addons/client");
}
void LoadDownloadedServerAddons()
{
    Msg("[Lua-Client] Loading downloaded addons...\n");
    IncludeClientFolder_R("lua/addons/server");
}

class CLuaAddonSystem : public CAutoGameSystemPerFrame
{
public:
    // Конструктор передает имя системы в базовый класс движка
    CLuaAddonSystem() : CAutoGameSystemPerFrame("LuaAddonSystem") {}

    // Этот метод движок сам вызовет на клиенте, когда карта загрузится
    // и все объекты (Entities) будут созданы и готовы к работе
    virtual void LevelInitPostEntity() override
    {
        // Вызываем твою функцию загрузки аддонов
        LoadDownloadedClientAddons();
        LoadDownloadedServerAddons();
    }

    // (Опционально) Вызывается, когда игрок выходит с сервера или меняет карту
    virtual void LevelShutdownPostEntity() override
    {
        // Если у тебя в Lua есть событие "ShutDown" или очистка памяти,
        // её можно вызвать здесь.
    }
};

// КРИТИЧЕСКИ ВАЖНАЯ СТРОКА: Создаем статический экземпляр класса.
// Как только игра запустится, движок увидит эту переменную и сам добавит её в список систем.
static CLuaAddonSystem g_LuaAddonSystem;

// Конструктор и деструктор
CLuaManager::CLuaManager() { m_pLuaState = NULL; }
CLuaManager::~CLuaManager() { Shutdown(); }

// 3.1 Init
bool CLuaManager::Init() {
    m_pLuaState = luaL_newstate();
    if (!m_pLuaState) return false;

    g_pLuaManager = this;

    luaL_openlibs(m_pLuaState);
    RegGlobals();
    RegFunctions();

    const char* pScriptName = "lua/main.lua";

    // 2. ОТКРЫВАЕМ В РЕЖИМЕ "rb" (Бинарное чтение) вместо "r"
    FileHandle_t hFile = filesystem->Open(pScriptName, "rb", "GAME");
    if (hFile)
    {
        int nFileSize = filesystem->Size(hFile);
        char* pBuffer = new char[nFileSize + 1];
        int nBytesRead = filesystem->Read(pBuffer, nFileSize, hFile);
        pBuffer[nBytesRead] = '\0';

        filesystem->Close(hFile);

        // 3. Передаем в Lua
        if (luaL_dostring(m_pLuaState, pBuffer) != 0)
        {
            Warning("Lua Script Error: %s\n", lua_tostring(m_pLuaState, -1));
        }

        delete[] pBuffer;
    }
    else
    {
        Warning("Lua Loading error: Source filesystem cannot find '%s'\n", pScriptName);
        return false;
    }

#ifndef CLIENT_DLL
    LoadAddons();
#else
    LoadClientAddons();
#endif

    return INIT_OK;
}

// 3.2 Shutdown
void CLuaManager::Shutdown() {
    if (m_pLuaState) {
        lua_close(m_pLuaState);
        m_pLuaState = NULL;
    }
}

// 3.3 RegGlobals
void CLuaManager::RegGlobals() {
    if (!m_pLuaState) return;

    // Регистрация командных констант (из shareddefs.h / hl2mp_player.h)
    lua_pushnumber(m_pLuaState, 0);  lua_setglobal(m_pLuaState, "TEAM_UNASSIGNED");
    lua_pushnumber(m_pLuaState, 1);  lua_setglobal(m_pLuaState, "TEAM_SPECTATOR");
    lua_pushnumber(m_pLuaState, 2);  lua_setglobal(m_pLuaState, "TEAM_COMBINE"); // Для HL2MP
    lua_pushnumber(m_pLuaState, 3);  lua_setglobal(m_pLuaState, "TEAM_REBELS");  // Для HL2MP

    // Регистрация флагов игрока (из const.h)
    lua_pushnumber(m_pLuaState, (1 << 0)); lua_setglobal(m_pLuaState, "FL_ONGROUND"); // Игрок на земле
    lua_pushnumber(m_pLuaState, (1 << 1)); lua_setglobal(m_pLuaState, "FL_DUCKING");  // Игрок присел
}

// 3.4 RegFunctions
void CLuaManager::RegFunctions() {
    lua_register(m_pLuaState, "Msg",             Lua_Msg);
    lua_register(m_pLuaState, "Warning",         Lua_Warning);
    lua_register(m_pLuaState, "Error",           Lua_Error);
    lua_register(m_pLuaState, "RegisterCommand", Lua_RegisterConCommand);

#ifndef CLIENT_DLL
    lua_register(m_pLuaState, "ServerCommand",          Lua_ServerCommand);
    lua_register(m_pLuaState, "ShootBullets",           LuaWeapon_ShootBullets);
    lua_register(m_pLuaState, "RegisterWeapon",         Lua_RegisterWeapon);
    lua_register(m_pLuaState, "KillAllPlayers",         Lua_KillAllPlayers);
    lua_register(m_pLuaState, "GetLocalPlayer",         Lua_GetLocalPlayer);
    lua_register(m_pLuaState, "CreateNextBot",          Lua_CreateNextBot);
    lua_register(m_pLuaState, "SetBotSprite",           Lua_SetBotSprite);
    lua_register(m_pLuaState, "EmitEntitySound",        Lua_EmitEntitySound);
    lua_register(m_pLuaState, "SpawnPhysicsProp",       Lua_SpawnPhysicsProp);
    lua_register(m_pLuaState, "SpawnDynamicProp",       Lua_SpawnDynamicProp);
    lua_register(m_pLuaState, "PlaySoundAtCoordinates", Lua_PlaySoundAtCoordinates);

    // ---- СОЗДАНИЕ КЛАССА PLAYER В LUA ----

    // 1. Создаем новую метаблицу в реестре Lua
    luaL_newmetatable(m_pLuaState, PLAYER_META);

    // 2. Указываем метаблице, где искать методы при вызове через ":" (индекс __index)
    lua_pushvalue(m_pLuaState, -1);
    lua_setfield(m_pLuaState, -2, "__index");

    // 3. Заполняем таблицу методами нашего класса
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetHealth);    lua_setfield(m_pLuaState, -2, "GetHealth");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetHealth);    lua_setfield(m_pLuaState, -2, "SetHealth");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GiveItem);     lua_setfield(m_pLuaState, -2, "GiveItem");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetSpeed);     lua_setfield(m_pLuaState, -2, "GetSpeed");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetSpeed);     lua_setfield(m_pLuaState, -2, "SetSpeed");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetName);      lua_setfield(m_pLuaState, -2, "GetName");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetTeam);      lua_setfield(m_pLuaState, -2, "SetTeam");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetTeam);      lua_setfield(m_pLuaState, -2, "GetTeam");
    lua_pushcfunction(m_pLuaState, LuaPlayer_IsOnGround);   lua_setfield(m_pLuaState, -2, "IsOnGround");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ChatPrint);    lua_setfield(m_pLuaState, -2, "ChatPrint");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetPos);       lua_setfield(m_pLuaState, -2, "GetPos");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetPos);       lua_setfield(m_pLuaState, -2, "SetPos");
    lua_pushcfunction(m_pLuaState, LuaPlayer_StripWeapons); lua_setfield(m_pLuaState, -2, "StripWeapons");
    lua_pushcfunction(m_pLuaState, LuaPlayer_Kill);         lua_setfield(m_pLuaState, -2, "Kill");
    lua_pushcfunction(m_pLuaState, LuaPlayer_PlaySound);    lua_setfield(m_pLuaState, -2, "PlaySound");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ConCommand);   lua_setfield(m_pLuaState, -2, "ConCommand");
#endif

    // Очищаем стек после регистрации метаблицы
    lua_pop(m_pLuaState, 1);

#ifndef CLIENT_DLL
    // 1. Создаем метатаблицу для Entity в реестре Lua
    luaL_newmetatable(m_pLuaState, ENTITY_META);

    // 2. Указываем метатаблице саму себя для индексации через ":"
    lua_pushvalue(m_pLuaState, -1);
    lua_setfield(m_pLuaState, -2, "__index");

    // 3. Заполняем таблицу физическими методами
    lua_pushcfunction(m_pLuaState, LuaEntity_ApplyForce);   lua_setfield(m_pLuaState, -2, "ApplyForce");
    lua_pushcfunction(m_pLuaState, LuaEntity_ApplyTorque);  lua_setfield(m_pLuaState, -2, "ApplyTorque");
    lua_pushcfunction(m_pLuaState, LuaEntity_GetPos);       lua_setfield(m_pLuaState, -2, "GetPos");
    lua_pushcfunction(m_pLuaState, LuaWeapon_GetClassname); lua_setfield(m_pLuaState, -2, "GetClassname");
#endif

    // Очищаем стек после регистрации метатаблицы
    lua_pop(m_pLuaState, 1);
}

void CLuaManager::CallHook(const char* hookName, CBasePlayer* pPlayer)
{
    if (!m_pLuaState || !pPlayer) return;

    lua_getglobal(m_pLuaState, hookName);

    if (lua_isfunction(m_pLuaState, -1))
    {
        // ВАЖНО: Используем нашу функцию для упаковки указателя в ООП-объект Lua
        PushPlayerObject(m_pLuaState, pPlayer);

        if (lua_pcall(m_pLuaState, 1, 0, 0) != 0)
        {
            Warning("Lua Hook Error (%s): %s\n", hookName, lua_tostring(m_pLuaState, -1));
        }
    }
    else
    {
        lua_pop(m_pLuaState, 1);
    }
}

#ifndef CLIENT_DLL
void CLuaManager::LoadAddons()
{
    FileFindHandle_t findHandle;
    // Ищем все файлы с расширением .lua в папке lua/addons/
    const char* pFileName = filesystem->FindFirstEx("lua/addons/server/*.lua", "GAME", &findHandle);

    while (pFileName != NULL)
    {
        // Строим полный путь к найденному файлу
        char fullPath[256];
        Q_snprintf(fullPath, sizeof(fullPath), "lua/addons/server/%s", pFileName);

        Msg("\n****** LUA ADDON LOADING ******\n");
        Msg("Loading Lua Addon: %s\n", fullPath);

        // Читаем и запускаем этот файл (код аналогичен вашему Init())
        FileHandle_t hFile = filesystem->Open(fullPath, "r", "GAME");
        if (hFile)
        {
            int nFileSize = filesystem->Size(hFile);
            char* pBuffer = new char[nFileSize + 1];
            int nBytesRead = filesystem->Read(pBuffer, nFileSize, hFile);
            pBuffer[nBytesRead] = '\0';
            filesystem->Close(hFile);

            if (luaL_dostring(m_pLuaState, pBuffer) != 0)
            {
                Warning("Error in server addon %s: %s\n", pFileName, lua_tostring(m_pLuaState, -1));
            }
            delete[] pBuffer;
        }

        // Переходим к следующему файлу в папке
        pFileName = filesystem->FindNext(findHandle);
    }

    filesystem->FindClose(findHandle);
}
#endif

#ifdef CLIENT_DLL
void CLuaManager::LoadClientAddons()
{
    if (!filesystem)
    {
        Warning(" [Client Lua] Filesystem interface is not ready yet!\n");
        return;
    }

    FileFindHandle_t findHandle;
    // Ищем все файлы с расширением .lua в папке lua/addons/
    const char* pFileName = filesystem->FindFirstEx("lua/addons/client/*.lua", "GAME", &findHandle);

    while (pFileName != NULL)
    {
        // Строим полный путь к найденному файлу
        char fullPath[256];
        Q_snprintf(fullPath, sizeof(fullPath), "lua/addons/client/%s", pFileName);

        Msg("\n****** LUA ADDON LOADING ******\n");
        Msg("Loading Lua Addon: %s\n", fullPath);

        // Читаем и запускаем этот файл (код аналогичен вашему Init())
        FileHandle_t hFile = filesystem->Open(fullPath, "r", "GAME");
        if (hFile)
        {
            int nFileSize = filesystem->Size(hFile);
            char* pBuffer = new char[nFileSize + 1];
            int nBytesRead = filesystem->Read(pBuffer, nFileSize, hFile);
            pBuffer[nBytesRead] = '\0';
            filesystem->Close(hFile);

            if (luaL_dostring(m_pLuaState, pBuffer) != 0)
            {
                Warning("Error in client addon %s: %s\n", pFileName, lua_tostring(m_pLuaState, -1));
            }
            delete[] pBuffer;
        }

        // Переходим к следующему файлу в папке
        pFileName = filesystem->FindNext(findHandle);
    }

    filesystem->FindClose(findHandle);
}
#endif

CON_COMMAND(lua_server_addons_list, "Shows all addons in lua/addons/server folder.")
{
    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirstEx("lua/addons/server/*.lua", "GAME", &findHandle);

    Msg("List of addons: ");

    while (pFileName != NULL)
    {
        Msg("%s; ", pFileName);

        pFileName = filesystem->FindNext(findHandle);
    }

    Msg("\n");
}

CON_COMMAND(lua_client_addons_list, "Shows all addons in lua/addons/client folder.")
{
    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirstEx("lua/addons/client/*.lua", "GAME", &findHandle);

    Msg("List of addons: ");

    while (pFileName != NULL)
    {
        Msg("%s; ", pFileName);

        pFileName = filesystem->FindNext(findHandle);
    }

    Msg("\n");
}

// Инициализация глобального указателя
static CLuaManager g_LuaManager;
CLuaManager* LuaManager() {
    return &g_LuaManager;
}