#pragma once

#include <vgui_controls/Frame.h>
#include <vgui_controls/HTML.h>

class CCustomHTML : public vgui::HTML
{
    DECLARE_CLASS_SIMPLE(CCustomHTML, vgui::HTML);
public:
    CCustomHTML(vgui::Panel* parent, const char* name) : BaseClass(parent, name) {}

    virtual void OnKeyCodeTyped(vgui::KeyCode code) override;
    virtual void OnFinishRequest(const char* url, const char* pageTitle,
        const CUtlMap<CUtlString, CUtlString>& headers) override;
};

class CHTMLMainMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CHTMLMainMenu, vgui::Frame);

public:
    CHTMLMainMenu(vgui::Panel* parent, const char* name);
    virtual ~CHTMLMainMenu();

    static void OpenMenu();
    static void CloseMenu();
    static void ToggleMenu();

    virtual void PerformLayout() override;
    virtual void OnCommand(const char* command) override;
    virtual void OnKeyCodePressed(vgui::KeyCode code) override;

    static void PushGameState();

private:
    CCustomHTML* m_pHTMLPanel;
};