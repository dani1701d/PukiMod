#pragma once
#include <vgui_controls/Frame.h>

class CAddonsListPanel : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CAddonsListPanel, vgui::Frame);

public:
    CAddonsListPanel(vgui::VPANEL parent);
    ~CAddonsListPanel();

    void GetAddonsList();

protected:
    virtual void ApplySchemeSettings(vgui::IScheme* pScheme) override;

private:
    vgui::ListPanel* m_pListPanel;
};

extern CAddonsListPanel* g_pAddonsListPanel;