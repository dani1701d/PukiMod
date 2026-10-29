#include "cbase.h"
#include "vgui_thankspanel.h"
#include "filesystem.h"
#include "luamanager.h"

// Подключаем необходимые интерфейсы VGUI
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui_controls/RichText.h>

CThanksPanel* g_pThanksPanel = nullptr;

CThanksPanel::CThanksPanel(vgui::VPANEL parent) : BaseClass(nullptr, "ThanksPanel")
{
    // Привязываем панель к родителю (например, к viewport игры)
    SetParent(parent);

    SetSize(500, 800);
    MoveToCenterOfScreen();
    SetTitle("Special Thanks", true);

    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetCloseButtonVisible(true);
    SetSizeable(false);
    SetMoveable(true);
    SetVisible(true);

    m_pRichText = new vgui::RichText(this, "ContentRichText");

    m_pRichText->SetBounds(20, 30, 460, 750);
}

CThanksPanel::~CThanksPanel()
{
}

void CThanksPanel::ApplySchemeSettings(vgui::IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    // Настройки фона панели (опционально)
    SetBgColor(Color(0, 0, 0, 150));

    vgui::HFont hFont = pScheme->GetFont("DefaultSmall", IsProportional());
    m_pRichText->SetFont(hFont);

    // 2. Set custom text colors (RGBA format)
    Color textColor(255, 255, 255, 255); // White text
    m_pRichText->SetFgColor(textColor);

    m_pRichText->SetText("Special thanks to these guys:\n\n* Rasbur - created bg for main menu and logo; also created map pm_hill; created Agu model.\n\n* snus - created another Agu model.");
}

CON_COMMAND(open_thanks_panel, "Opens Thanks Panel")
{
    // 1. Если панель еще не создана — создаем ее
    if (!g_pThanksPanel)
    {
        // Привязываем к главному экрану (vgui::surface()->GetEmbeddedPanel())
        g_pThanksPanel = new CThanksPanel(vgui::surface()->GetEmbeddedPanel());
    }

    if (g_pThanksPanel)
    {
        // 2. Если она уже открыта — закрываем (тоггл), если скрыта — показываем
        if (g_pThanksPanel->IsVisible())
        {
            g_pThanksPanel->SetVisible(false);
        }
        else
        {
            g_pThanksPanel->SetVisible(true);
            g_pThanksPanel->Activate();
        }
    }
}  