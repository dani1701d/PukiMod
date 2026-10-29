#include "cbase.h"
#include "engine/IEngineSound.h"
#include "filesystem.h"
#include "interface.h"
#include "luamanager.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "weapon_lua.h"
#include <windows.h>
#include <iostream>
#include <string>

#ifndef CLIENT_DLL
#include "networkstringtable_gamedll.h"
#include "entitylist.h"
#endif

#ifdef CLIENT_DLL
#include "networkstringtable_clientdll.h"
#include "lua_vguipanel.h"
#include "iclientmode.h"
#endif

#include "igamesystem.h"
#include <string>

#ifdef CLIENT_DLL
#include "usermessages.h"
#include "inetchannelinfo.h"   // <- добавить
#else
#include "recipientfilter.h"
#endif
#include "utlbuffer.h"

// Уникальное имя нашей метаблицы для проверки типов в Lua
#define PLAYER_META "SourcePlayer"
#define ENTITY_META "Entity"
#define PANEL_META  "VGUIPanel"
#define CLIENTPLAYER_META "ClientPlayer"

CLuaManager* g_pLuaManager = nullptr;

// ========================================== Ардуино ==========================================
// Дескриптор последовательного порта
static HANDLE hSerial = INVALID_HANDLE_VALUE;

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

#ifdef CLIENT_DLL
#include "iinput.h"

int Lua_RunConsoleCommand(lua_State* L)
{
    const char* cmd = luaL_checkstring(L, 1);
    engine->ClientCmd_Unrestricted(cmd);
    return 0;
}

int Lua_ServerCmd(lua_State* L)
{
    char buf[512];
    int n = lua_gettop(L);

    V_strncpy(buf, luaL_checkstring(L, 1), sizeof(buf));
    for (int i = 2; i <= n; i++)
    {
        V_strncat(buf, " ", sizeof(buf)); //V_strncat(buf, " \"", sizeof(buf)); 
        V_strncat(buf, luaL_checkstring(L, i), sizeof(buf));
        //V_strncat(buf, "\"", sizeof(buf)); // хз может эта фигня нужна, но для аргумента в виде строки мешает
    }

    engine->ServerCmd(buf);   // выполнится на сервере от имени этого игрока
    return 0;
}

int Lua_IsKeyDown(lua_State* L)
{
    int key = luaL_checknumber(L, 1);

    C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
    if (pLocalPlayer)
    {
        // Получаем маску текущих нажатых кнопок
        int buttons = pLocalPlayer->m_nButtons;

        if (buttons & key)
        {
            lua_pushboolean(L, 1);
            return 1;
        }
        else 
        {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    return 0;
}

int Lua_IsKeyPressed(lua_State* L)
{
    int key = luaL_checknumber(L, 1);

    C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
    if (pLocalPlayer)
    {
        // Получаем маску текущих нажатых кнопок
        int buttons = pLocalPlayer->m_nButtons;
        double m_flLastPressTime = 0.0;

        if (buttons & key)
        {
            double flCurrentTime = Plat_FloatTime(); // Получаем текущее время в секундах

            if (flCurrentTime - m_flLastPressTime >= 1)
            {
                lua_pushboolean(L, 1);
                return 1;
            }
        }
        else
        {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    return 0;
}
#endif

int LuaWeapon_GetClassname(lua_State* L)
{
    CBaseEntity* pEnt = *(CBaseEntity**)luaL_checkudata(L, 1, "Entity");
    CWeaponLua* pWeapon = dynamic_cast<CWeaponLua*>(pEnt);
    if (pWeapon) {
        lua_pushstring(L, pWeapon->GetClassname());
        return 1;
    }
    return 0;
}

#ifdef CLIENT_DLL
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui/ILocalize.h>

// Вспомогательная функция проталкивания панели в Lua
static void PushPanelObject(lua_State* L, vgui::Panel* pPanel)
{
    if (!pPanel)
    {
        lua_pushnil(L);
        return;
    }
    LuaPanel_t* pLuaPanel = (LuaPanel_t*)lua_newuserdata(L, sizeof(LuaPanel_t));
    Construct(pLuaPanel);
    pLuaPanel->hPanel.Set(pPanel);

    luaL_getmetatable(L, PANEL_META);
    lua_setmetatable(L, -2);
}

static vgui::Panel* GetPanelFromLua(lua_State* L, int index)
{
    LuaPanel_t* pLuaPanel = (LuaPanel_t*)luaL_checkudata(L, index, PANEL_META);
    return pLuaPanel ? pLuaPanel->hPanel.Get() : nullptr;
}

// vgui.CreateFrame(title)
int Lua_VGUI_CreateFrame(lua_State* L)
{
    const char* title = luaL_optstring(L, 1, "Window");

    // Создаем окно с родителем g_pClientMode->GetViewport() или NULL
    CLuaFrame* pFrame = new CLuaFrame(g_pClientMode ? g_pClientMode->GetViewport() : nullptr, "LuaFrame");
    pFrame->SetTitle(title, true);
    pFrame->SetScheme("ClientScheme");
    pFrame->SetMoveable(true);
    pFrame->SetSizeable(true);
    pFrame->SetMenuButtonVisible(false);
    pFrame->SetSize(400, 300);
    pFrame->SetVisible(true);
    pFrame->Activate(); // Показывает окно и перехватывает ввод фокуса

    PushPanelObject(L, pFrame);
    return 1;
}

// panel:SetSize(w, h)
int LuaPanel_SetSize(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    int w = luaL_checkinteger(L, 2);
    int h = luaL_checkinteger(L, 3);

    if (pPanel)
        pPanel->SetSize(w, h);

    return 0;
}

// panel:SetPos(x, y)
int LuaPanel_SetPos(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);

    if (pPanel)
        pPanel->SetPos(x, y);

    return 0;
}

// panel:AddButton(text, x, y, w, h, onClickCallback)
int LuaPanel_AddButton(lua_State* L)
{
    vgui::Panel* pParent = GetPanelFromLua(L, 1);
    const char* text = luaL_checkstring(L, 2);
    int x = luaL_checkinteger(L, 3);
    int y = luaL_checkinteger(L, 4);
    int w = luaL_checkinteger(L, 5);
    int h = luaL_checkinteger(L, 6);

    if (!pParent) return 0;

    CLuaFrame* pFrame = dynamic_cast<CLuaFrame*>(pParent);
    if (!pFrame) return 0;

    vgui::Button* pBtn = new vgui::Button(pFrame, "LuaButton", text);
    pBtn->AddActionSignalTarget(pFrame);
    pFrame->AddLuaChild(pBtn, x, y, w, h);
    
    // Если 7-й аргумент — функция, сохраняем ее в реестре Lua
    if (lua_isfunction(L, 7))
    {
        lua_pushvalue(L, 7);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);

        static int g_cmdID = 0;
        char cmdBuf[32];
        Q_snprintf(cmdBuf, sizeof(cmdBuf), "luabtn_%d", g_cmdID++);

        pBtn->SetCommand(cmdBuf);
        pFrame->RegisterButtonCallback(cmdBuf, ref);
    }

    PushPanelObject(L, pBtn);
    return 1;
}

// panel:AddImage(path, x, y, w, h)
int LuaPanel_AddImage(lua_State* L)
{
    vgui::Panel* pParent = GetPanelFromLua(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int x = luaL_checkinteger(L, 3);
    int y = luaL_checkinteger(L, 4);
    int w = luaL_checkinteger(L, 5);
    int h = luaL_checkinteger(L, 6);

    if (!pParent) return 0;

    CLuaFrame* pFrame = dynamic_cast<CLuaFrame*>(pParent);
    if (!pFrame) return 0;

    vgui::ImagePanel* pImg = new vgui::ImagePanel(pFrame, "LuaImagePanel");
    pImg->SetImage(path);
    pImg->SetShouldScaleImage(true);
    pFrame->AddLuaChild(pImg, x, y, w, h);

    PushPanelObject(L, pImg);
    return 1;
}

// panel:AddTextEntry(x, y, w, h)
int LuaPanel_AddTextEntry(lua_State* L)
{
    vgui::Panel* pParent = GetPanelFromLua(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int w = luaL_checkinteger(L, 4);
    int h = luaL_checkinteger(L, 5);

    if (!pParent) return 0;

    CLuaFrame* pFrame = dynamic_cast<CLuaFrame*>(pParent);
    if (!pFrame) return 0;

    vgui::TextEntry* pTextEntry = new vgui::TextEntry(pFrame, "LuaTextEntry");
    pFrame->AddLuaChild(pTextEntry, x, y, w, h);

    PushPanelObject(L, pTextEntry);
    return 1;
}

// panel:GetTextEntry(1)
int LuaPanel_GetTextEntry(lua_State* L)
{
    vgui::Panel* pParent = GetPanelFromLua(L, 1);
    int index = luaL_checkinteger(L, 2);

    if (!pParent) return 0;

    CLuaFrame* pFrame = dynamic_cast<CLuaFrame*>(pParent);
    if (!pFrame) return 0;

    CUtlVector<vgui::TextEntry*> m_TextEntries;

    for (int i = 0; i < pFrame->GetChildCount(); i++)
    {
        vgui::Panel* pChild = pFrame->GetChild(i);

        // Проверяем, совпадает ли имя
        if (pChild && Q_strcmp(pChild->GetName(), "LuaTextEntry") == 0)
        {
            // Пытаемся привести к типу TextEntry
            vgui::TextEntry* pEntry = dynamic_cast<vgui::TextEntry*>(pChild);
            if (pEntry)
            {
                m_TextEntries.AddToTail(pEntry);
            }
        }
    }

    char buffer[256];

    if (m_TextEntries.Count() > 0)
    {
        m_TextEntries[index]->GetText(buffer, sizeof(buffer));
        if (buffer[0] != '\0')
        {
            lua_pushstring(L, buffer);
            return 1;
        }
    }

    lua_pushnil(L);
    return 0;
}

// panel:Close()
int LuaPanel_Close(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    if (pPanel)
    {
        pPanel->MarkForDeletion(); // Безопасное удаление панели VGUI
    }
    return 0;
}

// panel:MakePopup()
int LuaPanel_MakePopup(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    if (pPanel)
    {
        pPanel->MakePopup();
        pPanel->SetMouseInputEnabled(true);
        pPanel->SetKeyBoardInputEnabled(true);
    }
    return 0;
}

// cl_player:GetHealth()
int LuaClientPlayer_GetHealth(lua_State* L)
{
    C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

    if (pPlayer)
    {
        lua_pushnumber(L, pPlayer->GetHealth());
        return 1;
    }

    return 0;
}

// cl_player:GetVelocity()
int LuaClientPlayer_GetVelocity(lua_State* L)
{
    C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

    if (pPlayer)
    {
        lua_pushnumber(L, pPlayer->GetAbsVelocity().Length2D());
        return 1;
    }

    return 0;
}

// cl_player:GetCurrentItem()
int LuaClientPlayer_GetCurrentItem(lua_State* L)
{
    C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

    if (pPlayer)
    {
        CBaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
        if (pWeapon)
        {
            lua_pushstring(L, pWeapon->GetClassname());
            return 1;
        }
    }
    return 0;
}

static CUtlVector<vgui::PHandle> s_HudLabels;

// vgui.CreateLabel(text, x, y)
int Lua_VGUI_CreateLabel(lua_State* L)
{
    const char* text = luaL_optstring(L, 1, "");
    int x = (int)luaL_optnumber(L, 2, 0);
    int y = (int)luaL_optnumber(L, 3, 0);

    vgui::Panel* pParent = g_pClientMode ? g_pClientMode->GetViewport() : nullptr;
    vgui::Label* pLabel = new vgui::Label(pParent, "LuaHudLabel", text);

    pLabel->SetScheme("ClientScheme");
    pLabel->SetMouseInputEnabled(false);       // клики проходят сквозь текст
    pLabel->SetKeyBoardInputEnabled(false);    // клавиатура не перехватывается
    pLabel->SetPaintBackgroundEnabled(false);
    pLabel->SetContentAlignment(vgui::Label::a_west);
    pLabel->SetFgColor(Color(255, 255, 255, 255));
    pLabel->SetPos(x, y);
    pLabel->SetVisible(true);
    pLabel->SizeToContents();

    vgui::PHandle h;
    h.Set(pLabel);
    s_HudLabels.AddToTail(h);

    PushPanelObject(L, pLabel);
    return 1;
}

// Удаляет все метки (вызывается при смене карты)
void LuaHud_RemoveAll()
{
    FOR_EACH_VEC(s_HudLabels, i)
    {
        vgui::Panel* p = s_HudLabels[i].Get();
        if (p)
            p->MarkForDeletion();
    }
    s_HudLabels.Purge();
}

// label:SetText(text)
int LuaPanel_SetText(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    const char* text = luaL_checkstring(L, 2);

    vgui::Label* pLabel = dynamic_cast<vgui::Label*>(pPanel);
    if (pLabel)
    {
        pLabel->SetText(text);
        if (!dynamic_cast<vgui::Button*>(pPanel))   // кнопки (они тоже Label) не растягиваем
            pLabel->SizeToContents();
    }
    return 0;
}

// label:SetText(text)
int LuaPanel_GetText(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);

    vgui::Label* pLabel = dynamic_cast<vgui::Label*>(pPanel);
    if (pLabel)
    {
        wchar_t szBuffer[256];
        pLabel->GetText(szBuffer, sizeof(szBuffer));

        char szText[256];
        vgui::ILocalize::ConvertUnicodeToANSI(szBuffer, szText, sizeof(szText));

        lua_pushstring(L, szText);
        return 1;
    }
    return 0;
}

// label:SetColor(r, g, b [, a])
int LuaPanel_SetColor(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    int r = clamp((int)luaL_checknumber(L, 2), 0, 255);
    int g = clamp((int)luaL_checknumber(L, 3), 0, 255);
    int b = clamp((int)luaL_checknumber(L, 4), 0, 255);
    int a = clamp((int)luaL_optnumber(L, 5, 255), 0, 255);

    if (pPanel)
        pPanel->SetFgColor(Color(r, g, b, a));
    return 0;
}

// label:SetFont("Default")  -- имя шрифта из ClientScheme.res
int LuaPanel_SetFont(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    const char* name = luaL_checkstring(L, 2);

    vgui::Label* pLabel = dynamic_cast<vgui::Label*>(pPanel);
    if (pLabel)
    {
        vgui::IScheme* pScheme = vgui::scheme()->GetIScheme(vgui::scheme()->GetScheme("ClientScheme"));
        vgui::HFont font = pScheme ? pScheme->GetFont(name, true) : 0;
        if (font != 0)
        {
            pLabel->SetFont(font);
            if (!dynamic_cast<vgui::Button*>(pPanel))
                pLabel->SizeToContents();
        }
        else
        {
            Warning("[Lua] unknown font '%s'\n", name);
        }
    }
    return 0;
}

// panel:SetVisible(bool)
int LuaPanel_SetVisible(lua_State* L)
{
    vgui::Panel* pPanel = GetPanelFromLua(L, 1);
    if (pPanel)
        pPanel->SetVisible(lua_toboolean(L, 2) != 0);
    return 0;
}

// GetScreenSize() -> width, height
int Lua_GetScreenSize(lua_State* L)
{
    int w, h;
    engine->GetScreenSize(w, h);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

// Вызов глобальной Lua-функции без аргументов (для покадровых хуков)
void Lua_CallGlobal(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        // не чаще раза в 2 секунды, чтобы не забить консоль
        static double s_flNextWarn = 0.0;
        if (Plat_FloatTime() >= s_flNextWarn)
        {
            Warning("[Lua] error in %s: %s\n", name, lua_tostring(L, -1));
            s_flNextWarn = Plat_FloatTime() + 2.0;
        }
        lua_pop(L, 1);
    }
}
#endif

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
        Warning("%s", text);                   // Выводим в стандартную консоль Source
    }
    return 0; // Возвращаем 0, так как функция ничего не возвращает обратно в Lua
}

int Lua_Error(lua_State* L)
{
    // Проверяем, передал ли пользователь строку в качестве первого аргумента (индекс 1)
    if (lua_isstring(L, 1))
    {
        const char* text = lua_tostring(L, 1); // Получаем строку из стека Lua
        Error("%s", text);                     // Крашим игру
    }
    return 0; // Возвращаем 0, так как функция ничего не возвращает обратно в Lua
}

int Lua_InitSerialPort(lua_State* L)
{
    const char* portName = luaL_checkstring(L, 1); // COM3, COM4 и т.д.
    DWORD baudRate = luaL_checkinteger(L, 2); // 9600

    hSerial = CreateFileA(
        portName,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        Warning("Serial: Failed to open %s\n", portName);
        return 0;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return 0;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return 0;
    }

    // Настройка тайм-аутов записи
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.WriteTotalTimeoutConstant = 50;
    SetCommTimeouts(hSerial, &timeouts);

    Msg("Serial: Connected successfully to %s\n", portName);
    return 0;
}

int Lua_SendToArduino(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);

    if (hSerial != INVALID_HANDLE_VALUE)
    {
        // Формируем строку формата "H:100|V:30\n"
        std::string data = text;

        DWORD bytesWritten;
        WriteFile(hSerial, data.c_str(), (DWORD)data.length(), &bytesWritten, NULL);
    }

    return 0;
}

int Lua_CloseSerialPort(lua_State* L)
{
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
    }

    return 0;
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
        const char* safeCmdName = strdup(cmdName); // Выделяем память под имя команды

#ifdef CLIENT_DLL
        // Для КЛИЕНТА: обязательно передаем флаг FCVAR_CLIENTDLL
        ConCommand* pCmd = new ConCommand(safeCmdName, LuaCommandCallback, "Custom Client Lua Command", FCVAR_CLIENTDLL);
#else
        // Для СЕРВЕРА: обычная регистрация
        ConCommand* pCmd = new ConCommand(safeCmdName, LuaCommandCallback, "Custom Server Lua Command", FCVAR_GAMEDLL);
#endif

        // Принудительно связываем с системой cvar
        if (cvar)
        {
            cvar->RegisterConCommand(pCmd);
        }
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

// GetPlayers() -> таблица всех подключённых игроков
int Lua_GetPlayers(lua_State* L)
{
    lua_newtable(L);
    int idx = 1;
    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
        if (pPlayer && pPlayer->IsConnected())
        {
            PushPlayerObject(L, pPlayer);
            lua_rawseti(L, -2, idx++);
        }
    }
    return 1;
}

// GetCommandPlayer() -> игрок, вызвавший текущую команду (внутри функции RegisterCommand)
int Lua_GetCommandPlayer(lua_State* L)
{
    CBasePlayer* pPlayer = UTIL_GetCommandClient();
    if (!pPlayer) { lua_pushnil(L); return 1; }
    PushPlayerObject(L, pPlayer);
    return 1;
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

int Lua_ShowTitle(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    int duration = luaL_checknumber(L, 2);

    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
        if (pPlayer)
        {
            hudtextparms_t params;
            params.x = -1.0; // Центр по горизонтали
            params.y = 0.4;  // Чуть выше центра по вертикали (чтобы не мешать прицелу)
            params.a1 = 255; // Альфа-канал (прозрачность)
            params.r1 = 255; params.g1 = 255; params.b1 = 255; // Белый цвет
            params.fadeinTime = 0.1;
            params.fadeoutTime = 0.1;
            params.holdTime = duration; // Сколько секунд держать текст

            UTIL_HudMessage(pPlayer, params, text);
        }
    }
    return 0;
}

int Lua_GetMapName(lua_State* L)
{
    const char* pMapName = STRING(gpGlobals->mapname);

    lua_pushstring(L, pMapName);
    return 1;
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

// Метод: player:GetCurrentItem()
int LuaPlayer_GetCurrentItem(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);

    if (pPlayer)
    {
        CBaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();
        if (pWeapon)
        {
            lua_pushstring(L, pWeapon->GetClassname());
            return 1;
        }
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

// Метод: player:Ignite(3)
int LuaPlayer_Ignite(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float fireTime = (float)luaL_checknumber(L, 2); // время длительности огня в секундах

    if (pPlayer)
    {
        pPlayer->Ignite(fireTime, false, 0.5f);
    }
    return 0;
}

// Метод: player:SetFOV(90)
int LuaPlayer_SetFOV(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float fov = (float)luaL_checknumber(L, 2); // поле зрения

    if (pPlayer)
    {
        pPlayer->SetFOV(pPlayer, fov);
    }
    return 0;
}

int LuaPlayer_SetModel(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    const char* modelPath = luaL_checkstring(L, 2); // путь к модели

    if (pPlayer)
    {
        CBaseEntity::PrecacheModel(modelPath);
        pPlayer->SetModel(modelPath);
        
    }
    return 0;
}

// Метод: player:SetColor(255, 0, 0)
int LuaPlayer_SetColor(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    float r = (float)luaL_checknumber(L, 2); // красный
    float g = (float)luaL_checknumber(L, 3); // зелёный
    float b = (float)luaL_checknumber(L, 4); // синий

    if (pPlayer)
    {
        pPlayer->SetRenderColor(r, g, b);
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

// Метод: player:GetEyeVector(100)
// Возвращает позицию конца луча взгляда игрока
int LuaPlayer_GetEyeVector(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    int force = luaL_checknumber(L, 2);

    if (pPlayer)
    {
        QAngle qAngles = pPlayer->EyeAngles();

        // 2. Конвертируем углы в нормализованный вектор направления вперед
        Vector vecForward;
        AngleVectors(qAngles, &vecForward);

        Vector vecImpulse = vecForward * force;
        lua_pushnumber(L, vecImpulse.x);
        lua_pushnumber(L, vecImpulse.y);
        lua_pushnumber(L, vecImpulse.z);
        return 3;
    }

    return 0;
}

// Метод: player:ApplyImpulse(x, y, z)
// Применяет к игроку импульс силы
int LuaPlayer_ApplyImpulse(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    int x = luaL_checknumber(L, 2);
    int y = luaL_checknumber(L, 3);
    int z = luaL_checknumber(L, 4);

    Vector impulse = Vector(x, y, z);

    if (pPlayer)
    {
        pPlayer->ApplyAbsVelocityImpulse(impulse);
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

// Метод: player:IsKeyPressed(IN_JUMP)
int LuaPlayer_IsKeyPressed(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    int key = luaL_checknumber(L, 2);

    if (pPlayer && (pPlayer->m_nButtons & key))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    else
    {
        lua_pushboolean(L, 0);
        return 1;
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

// Метод: player:ShowHint("Сообщение")
// Использование в Lua: player:ShowHint("Welcome to the server!")
int LuaPlayer_ShowHint(lua_State* L)
{
    CBasePlayer* pPlayer = GetPlayerFromLua(L, 1);
    const char* message = luaL_checkstring(L, 2);

    if (pPlayer)
    {
        UTIL_HudHintText(pPlayer, message);
    }

    return 0;
}
#endif


// NOT USED: engine file download (downloadables) does not deliver .lua files.
// Client scripts are sent through the "LuaFile" user message instead (see LuaSync below).
#if 0
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
            pTable->AddString(true, szFullFilePath);
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
#endif

// Рекурсивный запуск клиентских скриптов
void IncludeClientFolder_R(const char* pszCurrentPath)
{
    char szSearchPath[MAX_PATH];
    Q_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.lua", pszCurrentPath);

    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirst(szSearchPath, &findHandle);

    while (pFileName)
    {
        if (pFileName[0] != '.')
        {
            char szFullFilePath[MAX_PATH];
            Q_snprintf(szFullFilePath, sizeof(szFullFilePath), "%s/%s", pszCurrentPath, pFileName);

            // Выполняем файл в текущем Lua-контексте!
            lua_State* L = LuaManager()->GetLuaHandle();
            if (L)
            {
                FileHandle_t hFile = filesystem->Open(szFullFilePath, "rb", "GAME");
                if (hFile)
                {
                    int nFileSize = filesystem->Size(hFile);
                    char* pBuffer = new char[nFileSize + 1];
                    int nBytesRead = filesystem->Read(pBuffer, nFileSize, hFile);
                    pBuffer[nBytesRead] = '\0';
                    filesystem->Close(hFile);

                    Msg("[Lua] exec %s\n", szFullFilePath);
                    if (luaL_dostring(L, pBuffer) != 0)
                    {
                        Warning("[Lua-Client Error] %s: %s\n", pFileName, lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                    delete[] pBuffer;
                }
            }
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

// ======================= LuaSync =======================
// Sending client Lua scripts from the server to connecting clients through
// the "LuaFile" user message (the engine does not download .lua files).
// Message "LuaFile" must be registered on BOTH sides: usermessages->Register("LuaFile", -1);
#define LUASYNC_CHUNK      200   // bytes of file data per message
#define LUASYNC_PER_FRAME  4     // messages per server frame (avoids reliable overflow)

#ifndef CLIENT_DLL

struct LuaSync_t
{
    int  userid;
    CUtlVector<CUtlString> files;
    int  fileIdx;
    int  offset;
    bool haveFile;
    CUtlBuffer data;
    LuaSync_t() : userid(0), fileIdx(0), offset(0), haveFile(false) {}
};
static CUtlVector<LuaSync_t*> g_LuaSyncs;

static void LuaSync_Collect_R(const char* dir, CUtlVector<CUtlString>& out)
{
    char mask[MAX_PATH];
    Q_snprintf(mask, sizeof(mask), "%s/*", dir);

    FileFindHandle_t h;
    for (const char* f = filesystem->FindFirstEx(mask, "GAME", &h); f; f = filesystem->FindNext(h))
    {
        if (f[0] == '.') continue;

        char full[MAX_PATH];
        Q_snprintf(full, sizeof(full), "%s/%s", dir, f);

        if (filesystem->FindIsDirectory(h))
        {
            LuaSync_Collect_R(full, out);
        }
        else
        {
            const char* ext = V_GetFileExtension(f);
            if (ext && !Q_stricmp(ext, "lua"))
                out.AddToTail(full);
        }
    }
    filesystem->FindClose(h);
}

// A client asks for its scripts
CON_COMMAND(lua_sync, "Internal: send client Lua addons to the requesting player")
{
    CBasePlayer* pPlayer = UTIL_GetCommandClient();
    if (!pPlayer || pPlayer->IsBot())
        return;

    // the listen-server host reads files from disk
    if (pPlayer == UTIL_GetListenServerHost())
        return;

    int userid = engine->GetPlayerUserId(pPlayer->edict());
    FOR_EACH_VEC(g_LuaSyncs, i)
    {
        if (g_LuaSyncs[i]->userid == userid)
            return;   // already sending
    }

    LuaSync_t* s = new LuaSync_t;
    s->userid = userid;
    LuaSync_Collect_R("lua/addons/client", s->files);
    Msg("[LuaSync] sending %d file(s) to %s\n", s->files.Count(), pPlayer->GetPlayerName());
    g_LuaSyncs.AddToTail(s);
}

static void LuaSync_Pump()
{
    for (int si = g_LuaSyncs.Count() - 1; si >= 0; si--)
    {
        LuaSync_t* sync = g_LuaSyncs[si];
        CBasePlayer* pPlayer = UTIL_PlayerByUserId(sync->userid);
        if (!pPlayer)
        {
            delete sync;
            g_LuaSyncs.Remove(si);
            continue;
        }

        CSingleUserRecipientFilter filter(pPlayer);
        filter.MakeReliable();

        bool bDone = false;
        for (int n = 0; n < LUASYNC_PER_FRAME && !bDone; n++)
        {
            if (!sync->haveFile)
            {
                if (sync->fileIdx >= sync->files.Count())
                {
                    UserMessageBegin(filter, "LuaFile");
                        WRITE_BYTE(3);               // everything sent
                    MessageEnd();
                    bDone = true;
                    break;
                }

                sync->data.Purge();
                const char* path = sync->files[sync->fileIdx].Get();
                if (!filesystem->ReadFile(path, "GAME", sync->data))
                {
                    sync->fileIdx++;
                    n--;                              // do not count a skipped file
                    continue;
                }
                sync->offset   = 0;
                sync->haveFile = true;

                UserMessageBegin(filter, "LuaFile");
                    WRITE_BYTE(1);                   // file header
                    WRITE_STRING(path);
                    WRITE_LONG(sync->data.TellPut());
                MessageEnd();
                continue;
            }

            int left = sync->data.TellPut() - sync->offset;
            if (left <= 0)
            {
                sync->haveFile = false;
                sync->fileIdx++;
                n--;
                continue;
            }

            int len = (left < LUASYNC_CHUNK) ? left : LUASYNC_CHUNK;
            const unsigned char* pBytes = (const unsigned char*)sync->data.Base() + sync->offset;

            UserMessageBegin(filter, "LuaFile");
                WRITE_BYTE(2);                       // data chunk
                WRITE_BYTE(len);
                for (int b = 0; b < len; b++)
                    WRITE_BYTE(pBytes[b]);
            MessageEnd();
            sync->offset += len;
        }

        if (bDone)
        {
            delete sync;
            g_LuaSyncs.Remove(si);
        }
    }
}

#else   // -------------------- CLIENT --------------------

struct LuaRecvFile_t
{
    CUtlString path;
    CUtlBuffer data;
    int        total;
    LuaRecvFile_t() : total(0) {}
};
static CUtlVector<LuaRecvFile_t*> s_RecvFiles;
static LuaRecvFile_t*             s_pCurFile = NULL;

static void LuaSync_ClearReceived()
{
    s_RecvFiles.PurgeAndDeleteElements();
    delete s_pCurFile;
    s_pCurFile = NULL;
}

static void LuaSync_ExecuteReceived()
{
    lua_State* L = LuaManager()->GetLuaHandle();

    FOR_EACH_VEC(s_RecvFiles, i)
    {
        LuaRecvFile_t* f = s_RecvFiles[i];
        if (L)
        {
            char chunkName[300];
            Q_snprintf(chunkName, sizeof(chunkName), "@%s", f->path.Get());

            Msg("[Lua] exec (from server) %s\n", f->path.Get());
            int err = luaL_loadbuffer(L, (const char*)f->data.Base(), f->data.TellPut(), chunkName);
            if (err == 0)
                err = lua_pcall(L, 0, 0, 0);
            if (err != 0)
            {
                Warning("[Lua] %s: %s\n", f->path.Get(), lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
    }
    LuaSync_ClearReceived();
}

static void __MsgFunc_LuaFile(bf_read& msg)
{
    int type = msg.ReadByte();

    if (type == 1)          // file header
    {
        char path[260];
        msg.ReadString(path, sizeof(path));
        int total = msg.ReadLong();

        delete s_pCurFile;
        s_pCurFile = NULL;

        // accept only scripts from lua/addons/client/
        if (Q_strnicmp(path, "lua/addons/client/", 18) != 0 || Q_strstr(path, ".."))
            return;

        s_pCurFile = new LuaRecvFile_t;
        s_pCurFile->path  = path;
        s_pCurFile->total = total;

        if (total <= 0)
        {
            s_RecvFiles.AddToTail(s_pCurFile);
            s_pCurFile = NULL;
        }
    }
    else if (type == 2)     // data chunk
    {
        int len = msg.ReadByte();
        unsigned char buf[256];
        for (int i = 0; i < len; i++)
            buf[i] = (unsigned char)msg.ReadByte();

        if (!s_pCurFile)
            return;

        s_pCurFile->data.Put(buf, len);
        if (s_pCurFile->data.TellPut() >= s_pCurFile->total)
        {
            s_RecvFiles.AddToTail(s_pCurFile);
            s_pCurFile = NULL;
        }
    }
    else if (type == 3)     // all files received
    {
        LuaSync_ExecuteReceived();
    }
}

#endif
// ===================== /LuaSync ========================

class CLuaAddonSystem : public CAutoGameSystemPerFrame
{
public:
    CLuaAddonSystem() : CAutoGameSystemPerFrame("LuaAddonSystem") {}

    virtual bool Init() override
    {
        LuaManager()->Init();
        return true;   // false here would abort loading client.dll / server.dll
    }

    virtual void Shutdown() override
    {
        LuaManager()->Shutdown();
    }

#ifdef CLIENT_DLL
    virtual void PostInit() override
    {
        // all user messages are registered by now
        usermessages->HookMessage("LuaFile", __MsgFunc_LuaFile);
    }

    virtual void LevelInitPostEntity() override
    {
        Msg("[Lua] LevelInitPostEntity (client)\n");

        // local files (listen-server host / developer machine)
        IncludeClientFolder_R("lua/addons/client");

        // remote client: ask the server for its client scripts a bit later
        INetChannelInfo* nci = engine->GetNetChannelInfo();
        m_flSyncAt = (nci && !nci->IsLoopback()) ? gpGlobals->realtime + 2.0f : 0.0f;
    }

    virtual void LevelShutdownPostEntity() override
    {
        m_flSyncAt = 0.0f;
        LuaSync_ClearReceived();
        LuaHud_RemoveAll();
    }

    virtual void Update(float frametime) override
    {
        if (m_flSyncAt > 0.0f && gpGlobals->realtime >= m_flSyncAt)
        {
            m_flSyncAt = 0.0f;
            Msg("[Lua] requesting client scripts from server\n");
            engine->ServerCmd("lua_sync");
        }

        lua_State* L = LuaManager()->GetLuaHandle();
        if (L)
            Lua_CallGlobal(L, "HUDThink");
    }

private:
    float m_flSyncAt = 0.0f;

#else
    virtual void LevelInitPostEntity() override
    {
        Msg("[Lua] LevelInitPostEntity (server)\n");
        IncludeClientFolder_R("lua/addons/server");
    }

    virtual void FrameUpdatePostEntityThink() override
    {
        LuaSync_Pump();
    }

    virtual void LevelShutdownPreEntity() override
    {
        g_LuaSyncs.PurgeAndDeleteElements();
    }
#endif
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

    // Регистрация клавиш
    lua_pushnumber(m_pLuaState, (1 << 0)); lua_setglobal(m_pLuaState, "IN_ATTACK");
    lua_pushnumber(m_pLuaState, (1 << 1)); lua_setglobal(m_pLuaState, "IN_JUMP");
    lua_pushnumber(m_pLuaState, (1 << 2)); lua_setglobal(m_pLuaState, "IN_DUCK");
    lua_pushnumber(m_pLuaState, (1 << 3)); lua_setglobal(m_pLuaState, "IN_FORWARD");
    lua_pushnumber(m_pLuaState, (1 << 4)); lua_setglobal(m_pLuaState, "IN_BACK");
    lua_pushnumber(m_pLuaState, (1 << 5)); lua_setglobal(m_pLuaState, "IN_USE");
    lua_pushnumber(m_pLuaState, (1 << 6)); lua_setglobal(m_pLuaState, "IN_CANCEL"); 
    lua_pushnumber(m_pLuaState, (1 << 7)); lua_setglobal(m_pLuaState, "IN_LEFT");
    lua_pushnumber(m_pLuaState, (1 << 8)); lua_setglobal(m_pLuaState, "IN_RIGHT");
    lua_pushnumber(m_pLuaState, (1 << 9)); lua_setglobal(m_pLuaState, "IN_MOVELEFT");
    lua_pushnumber(m_pLuaState, (1 << 10)); lua_setglobal(m_pLuaState, "IN_MOVERIGHT");
    lua_pushnumber(m_pLuaState, (1 << 11)); lua_setglobal(m_pLuaState, "IN_ATTACK2");
    lua_pushnumber(m_pLuaState, (1 << 12)); lua_setglobal(m_pLuaState, "IN_RUN");
    lua_pushnumber(m_pLuaState, (1 << 13)); lua_setglobal(m_pLuaState, "IN_RELOAD");
    lua_pushnumber(m_pLuaState, (1 << 14)); lua_setglobal(m_pLuaState, "IN_ALT1");
    lua_pushnumber(m_pLuaState, (1 << 15)); lua_setglobal(m_pLuaState, "IN_ALT2");
    lua_pushnumber(m_pLuaState, (1 << 16)); lua_setglobal(m_pLuaState, "IN_SCORE");
    lua_pushnumber(m_pLuaState, (1 << 17)); lua_setglobal(m_pLuaState, "IN_SPEED");
    lua_pushnumber(m_pLuaState, (1 << 18)); lua_setglobal(m_pLuaState, "IN_WALK");
    lua_pushnumber(m_pLuaState, (1 << 19)); lua_setglobal(m_pLuaState, "IN_ZOOM");
    lua_pushnumber(m_pLuaState, (1 << 20)); lua_setglobal(m_pLuaState, "IN_WEAPON1");
    lua_pushnumber(m_pLuaState, (1 << 21)); lua_setglobal(m_pLuaState, "IN_WEAPON2");
    lua_pushnumber(m_pLuaState, (1 << 22)); lua_setglobal(m_pLuaState, "IN_BULLRUSH");
    lua_pushnumber(m_pLuaState, (1 << 23)); lua_setglobal(m_pLuaState, "IN_GRENADE1");
    lua_pushnumber(m_pLuaState, (1 << 24)); lua_setglobal(m_pLuaState, "IN_GRENADE2");
    lua_pushnumber(m_pLuaState, (1 << 25)); lua_setglobal(m_pLuaState, "IN_ATTACK3");
    lua_pushnumber(m_pLuaState, (1 << 26)); lua_setglobal(m_pLuaState, "IN_DROP");
}

// 3.4 RegFunctions
void CLuaManager::RegFunctions() {
    lua_register(m_pLuaState, "Msg",             Lua_Msg);
    lua_register(m_pLuaState, "Warning",         Lua_Warning);
    lua_register(m_pLuaState, "Error",           Lua_Error);
    lua_register(m_pLuaState, "RegisterCommand", Lua_RegisterConCommand);
    lua_register(m_pLuaState, "InitSerialPort",  Lua_InitSerialPort);
    lua_register(m_pLuaState, "SendToArduino",   Lua_SendToArduino);
    lua_register(m_pLuaState, "CloseSerialPort", Lua_CloseSerialPort);

#ifdef CLIENT_DLL
    lua_register(m_pLuaState, "RunConsoleCommand", Lua_RunConsoleCommand);
    lua_register(m_pLuaState, "GetScreenSize", Lua_GetScreenSize);
    lua_register(m_pLuaState, "ServerCmd", Lua_ServerCmd);
    lua_register(m_pLuaState, "IsKeyDown", Lua_IsKeyDown);
    lua_register(m_pLuaState, "IsKeyPressed", Lua_IsKeyPressed);

    // Глобальная библиотека vgui в Lua
    lua_newtable(m_pLuaState);
    lua_pushcfunction(m_pLuaState, Lua_VGUI_CreateFrame);   lua_setfield(m_pLuaState, -2, "CreateFrame");
    lua_pushcfunction(m_pLuaState, Lua_VGUI_CreateLabel);   lua_setfield(m_pLuaState, -2, "CreateLabel");
    lua_setglobal(m_pLuaState, "vgui");

    // Метатаблица объектов VGUIPanel
    luaL_newmetatable(m_pLuaState, PANEL_META);

    lua_pushvalue(m_pLuaState, -1);
    lua_setfield(m_pLuaState, -2, "__index");

    lua_pushcfunction(m_pLuaState, LuaPanel_SetSize);       lua_setfield(m_pLuaState, -2, "SetSize");
    lua_pushcfunction(m_pLuaState, LuaPanel_SetPos);        lua_setfield(m_pLuaState, -2, "SetPos");
    lua_pushcfunction(m_pLuaState, LuaPanel_AddButton);     lua_setfield(m_pLuaState, -2, "AddButton");
    lua_pushcfunction(m_pLuaState, LuaPanel_AddImage);      lua_setfield(m_pLuaState, -2, "AddImage");
    lua_pushcfunction(m_pLuaState, LuaPanel_AddTextEntry);  lua_setfield(m_pLuaState, -2, "AddTextEntry");
    lua_pushcfunction(m_pLuaState, LuaPanel_GetTextEntry);  lua_setfield(m_pLuaState, -2, "GetTextEntry");
    lua_pushcfunction(m_pLuaState, LuaPanel_Close);         lua_setfield(m_pLuaState, -2, "Close");
    lua_pushcfunction(m_pLuaState, LuaPanel_MakePopup);     lua_setfield(m_pLuaState, -2, "MakePopup");
    lua_pushcfunction(m_pLuaState, LuaPanel_GetText);       lua_setfield(m_pLuaState, -2, "GetText");
    lua_pushcfunction(m_pLuaState, LuaPanel_SetText);       lua_setfield(m_pLuaState, -2, "SetText");
    lua_pushcfunction(m_pLuaState, LuaPanel_SetColor);      lua_setfield(m_pLuaState, -2, "SetColor");
    lua_pushcfunction(m_pLuaState, LuaPanel_SetFont);       lua_setfield(m_pLuaState, -2, "SetFont");
    lua_pushcfunction(m_pLuaState, LuaPanel_SetVisible);    lua_setfield(m_pLuaState, -2, "SetVisible");

    lua_pop(m_pLuaState, 1);

    // Глобальная библотека cl_player в Lua
    lua_newtable(m_pLuaState);
    lua_pushcfunction(m_pLuaState, LuaClientPlayer_GetHealth);       lua_setfield(m_pLuaState, -2, "GetHealth");
    lua_pushcfunction(m_pLuaState, LuaClientPlayer_GetVelocity);     lua_setfield(m_pLuaState, -2, "GetVelocity");
    lua_pushcfunction(m_pLuaState, LuaClientPlayer_GetCurrentItem);  lua_setfield(m_pLuaState, -2, "GetCurrentItem");
    lua_setglobal(m_pLuaState, "cl_player");
#endif

#ifndef CLIENT_DLL
    lua_register(m_pLuaState, "ServerCommand",          Lua_ServerCommand);
    lua_register(m_pLuaState, "KillAllPlayers",         Lua_KillAllPlayers);
    lua_register(m_pLuaState, "ShowTitle",              Lua_ShowTitle);
    lua_register(m_pLuaState, "GetMapName",             Lua_GetMapName);
    lua_register(m_pLuaState, "GetLocalPlayer",         Lua_GetLocalPlayer);
    lua_register(m_pLuaState, "GetPlayers",             Lua_GetPlayers);
    lua_register(m_pLuaState, "GetCommandPlayer",       Lua_GetCommandPlayer);
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
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetHealth);        lua_setfield(m_pLuaState, -2, "GetHealth");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetHealth);        lua_setfield(m_pLuaState, -2, "SetHealth");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GiveItem);         lua_setfield(m_pLuaState, -2, "GiveItem");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetCurrentItem);   lua_setfield(m_pLuaState, -2, "GetCurrentItem");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetSpeed);         lua_setfield(m_pLuaState, -2, "GetSpeed");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetSpeed);         lua_setfield(m_pLuaState, -2, "SetSpeed");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetName);          lua_setfield(m_pLuaState, -2, "GetName");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetTeam);          lua_setfield(m_pLuaState, -2, "SetTeam");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetTeam);          lua_setfield(m_pLuaState, -2, "GetTeam");
    lua_pushcfunction(m_pLuaState, LuaPlayer_IsOnGround);       lua_setfield(m_pLuaState, -2, "IsOnGround");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ChatPrint);        lua_setfield(m_pLuaState, -2, "ChatPrint");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ShowHint);         lua_setfield(m_pLuaState, -2, "ShowHint");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetPos);           lua_setfield(m_pLuaState, -2, "GetPos");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetPos);           lua_setfield(m_pLuaState, -2, "SetPos");
    lua_pushcfunction(m_pLuaState, LuaPlayer_GetEyeVector);     lua_setfield(m_pLuaState, -2, "GetEyeVector");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ApplyImpulse);     lua_setfield(m_pLuaState, -2, "ApplyImpulse");
    lua_pushcfunction(m_pLuaState, LuaPlayer_StripWeapons);     lua_setfield(m_pLuaState, -2, "StripWeapons");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetGravity);       lua_setfield(m_pLuaState, -2, "SetGravity");
    lua_pushcfunction(m_pLuaState, LuaPlayer_Kill);             lua_setfield(m_pLuaState, -2, "Kill");
    lua_pushcfunction(m_pLuaState, LuaPlayer_Ignite);           lua_setfield(m_pLuaState, -2, "Ignite");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetFOV);           lua_setfield(m_pLuaState, -2, "SetFOV");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetModel);         lua_setfield(m_pLuaState, -2, "SetModel");
    lua_pushcfunction(m_pLuaState, LuaPlayer_SetColor);         lua_setfield(m_pLuaState, -2, "SetColor");
    lua_pushcfunction(m_pLuaState, LuaPlayer_PlaySound);        lua_setfield(m_pLuaState, -2, "PlaySound");
    lua_pushcfunction(m_pLuaState, LuaPlayer_ConCommand);       lua_setfield(m_pLuaState, -2, "ConCommand");

    // Очищаем стек после регистрации метаблицы
    lua_pop(m_pLuaState, 1);
#endif

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

    // Очищаем стек после регистрации метатаблицы
    lua_pop(m_pLuaState, 1);
#endif
}

void CLuaManager::Think()
{
    double flCurrentTime = Plat_FloatTime(); // Получаем текущее время в секундах

    // 100 миллисекунд = 0.1 секунды
    if (flCurrentTime - m_flLastThinkTime >= 0.1)
    {
        m_flLastThinkTime = flCurrentTime; // Обновляем время последнего вызова
        CallHook("Think");                 // Вызываем наш хук
    }
}

void CLuaManager::CallPlayerHook(const char* hookName, CBasePlayer* pPlayer)
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

void CLuaManager::CallHook(const char* hookName)
{
    if (!m_pLuaState) return;

    lua_getglobal(m_pLuaState, hookName);

    if (lua_isfunction(m_pLuaState, -1))
    {
        // ВАЖНО: Используем нашу функцию для упаковки указателя в ООП-объект Lua
        if (lua_pcall(m_pLuaState, 0, 0, 0) != 0)
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
    if (!m_pLuaState)
    {
        Warning(" [Client Lua] m_pLuaState is NULL!\n");
        return;
    }

    // ==========================================
    // СТРАХОВКА: Регистрируем vgui прямо здесь перед запуском скриптов!
    // ==========================================
    lua_getglobal(m_pLuaState, "vgui");
    if (!lua_istable(m_pLuaState, -1))
    {
        lua_pop(m_pLuaState, 1); // убираем nil со стека

        // Создаем таблицу vgui
        lua_newtable(m_pLuaState);
        lua_pushcfunction(m_pLuaState, Lua_VGUI_CreateFrame);
        lua_setfield(m_pLuaState, -2, "CreateFrame");
        lua_setglobal(m_pLuaState, "vgui");

        Msg("[Client Lua] Таблица 'vgui' была аварийно зарегистрирована в LoadClientAddons!\n");
    }
    else
    {
        lua_pop(m_pLuaState, 1);
    }
    // ==========================================

    if (!filesystem)
    {
        Warning(" [Client Lua] Filesystem interface is not ready yet!\n");
        return;
    }

    FileFindHandle_t findHandle;
    const char* pFileName = filesystem->FindFirstEx("lua/addons/client/*.lua", "GAME", &findHandle);

    while (pFileName != NULL)
    {
        char fullPath[256];
        Q_snprintf(fullPath, sizeof(fullPath), "lua/addons/client/%s", pFileName);

        Msg("\n****** LUA ADDON LOADING ******\n");
        Msg("Loading Lua Addon: %s\n", fullPath);

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