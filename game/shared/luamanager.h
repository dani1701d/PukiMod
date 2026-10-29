#ifndef LUAMANAGER_H
#define LUAMANAGER_H
#pragma once

#include "../server/lua.hpp" // Подключаем сам Lua

class CLuaManager {
public:
    CLuaManager();
    ~CLuaManager();

    bool Init();
    void Shutdown();

    // Регистрация глобальных переменных и функций
    void RegGlobals();
    void RegFunctions();

    void Think();
    void CallPlayerHook(const char* hookName, CBasePlayer* pPlayer);
    void CallHook(const char* hookName);
    void LoadAddons(); // Серверный метод
    void LoadClientAddons(); // Клиентский метод

    lua_State* GetLuaHandle() { return m_pLuaState; }

private:
    lua_State* m_pLuaState;
    double m_flLastThinkTime = 0.0;
};

// Объявляем глобальный указатель, чтобы вызывать менеджер из любой точки мода
extern CLuaManager* LuaManager();

#endif // LUAMANAGER_H