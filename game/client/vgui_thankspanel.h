#pragma once
#include <vgui_controls/Frame.h>

class CThanksPanel : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CThanksPanel, vgui::Frame);

public:
    CThanksPanel(vgui::VPANEL parent);
    ~CThanksPanel();

protected:
    virtual void ApplySchemeSettings(vgui::IScheme* pScheme) override;

private:
    vgui::RichText* m_pRichText;
};

extern CThanksPanel* g_pThanksPanel;