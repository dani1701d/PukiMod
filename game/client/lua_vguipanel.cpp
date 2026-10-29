#include "cbase.h"
#include "lua_vguipanel.h"
#include "luamanager.h"
#include <vgui/IInput.h>
#include <vgui/ISurface.h>

CLuaFrame::CLuaFrame(vgui::Panel* pParent, const char* pPanelName)
    : BaseClass(pParent, pPanelName)
{
    SetTitle("Lua Window", true);
    SetSize(300, 200);
    SetVisible(true);
    SetScheme("ClientScheme");
}

CLuaFrame::~CLuaFrame()
{
    lua_State* L = LuaManager()->GetLuaHandle();
    if (L)
    {
        for (int i = m_ButtonCallbacks.First(); i != m_ButtonCallbacks.InvalidIndex(); i = m_ButtonCallbacks.Next(i))
        {
            luaL_unref(L, LUA_REGISTRYINDEX, m_ButtonCallbacks[i]);
        }
    }
}

void CLuaFrame::RegisterButtonCallback(const char* command, int luaCallbackRef)
{
    m_ButtonCallbacks.Insert(command, luaCallbackRef);
}

void CLuaFrame::AddLuaChild(vgui::Panel* pChild, int x, int y, int w, int h)
{
    LuaChild_t c;
    c.hPanel.Set(pChild);
    c.x = x; c.y = y; c.w = w; c.h = h;
    m_Children.AddToTail(c);
    pChild->SetBounds(x, y, w, h);
}

void CLuaFrame::PerformLayout()
{
    BaseClass::PerformLayout();

    FOR_EACH_VEC(m_Children, i)
    {
        vgui::Panel* p = m_Children[i].hPanel.Get();
        if (p)
            p->SetBounds(m_Children[i].x, m_Children[i].y, m_Children[i].w, m_Children[i].h);
    }
}

void CLuaFrame::OnCommand(const char* command)
{
    int idx = m_ButtonCallbacks.Find(command);
    if (idx != m_ButtonCallbacks.InvalidIndex())
    {
        lua_State* L = LuaManager()->GetLuaHandle();
        if (L)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, m_ButtonCallbacks[idx]);
            if (lua_pcall(L, 0, 0, 0) != 0)
            {
                Warning("[Lua] button callback error: %s\n", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }
        return;
    }
    BaseClass::OnCommand(command);
}