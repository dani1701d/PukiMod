#pragma once
#include <vgui_controls/Frame.h>
#include <vgui_controls/HTML.h>

class CWebBrowserPanel : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CWebBrowserPanel, vgui::Frame);

public:
    CWebBrowserPanel(vgui::VPANEL parent);
    ~CWebBrowserPanel();

    void OpenURL(const char* URL, const char* postData, bool force);

protected:
    virtual void ApplySchemeSettings(vgui::IScheme* pScheme) override;

private:
    vgui::HTML* m_pHTML;
};

extern CWebBrowserPanel* g_pWebBrowserPanel;