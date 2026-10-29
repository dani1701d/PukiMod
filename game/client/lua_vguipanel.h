#ifndef LUA_VGUIPANEL_H
#define LUA_VGUIPANEL_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include "utldict.h"
#include "utlvector.h"
#include "../server/lua.hpp"

class CLuaFrame : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CLuaFrame, vgui::Frame);

public:
    CLuaFrame(vgui::Panel* pParent, const char* pPanelName);
    virtual ~CLuaFrame();

    virtual void OnCommand(const char* command) override;

    void RegisterButtonCallback(const char* command, int luaCallbackRef);
    void AddLuaChild(vgui::Panel* pChild, int x, int y, int w, int h);

protected:
    virtual void PerformLayout() override;

private:
    struct LuaChild_t
    {
        vgui::PHandle hPanel;
        int x, y, w, h;
    };

    CUtlDict<int, int>     m_ButtonCallbacks;   // "luabtn_N" -> luaL_ref
    CUtlVector<LuaChild_t> m_Children;
};

struct LuaPanel_t
{
    vgui::PHandle hPanel;   // Get() вернёт NULL, если панель уже удалена
};

#endif