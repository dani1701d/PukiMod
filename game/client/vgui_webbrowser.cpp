#include "cbase.h"
#include "vgui_webbrowser.h"

// Подключаем необходимые интерфейсы VGUI
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>

CWebBrowserPanel* g_pWebBrowserPanel = nullptr;

CWebBrowserPanel::CWebBrowserPanel(vgui::VPANEL parent) : BaseClass(nullptr, "WebBrowser")
{
    // Привязываем панель к родителю (например, к viewport игры)
    SetParent(parent);

    SetSize(1000, 800);
    MoveToCenterOfScreen();
    SetTitle("Web Browser", true);

    // Создаем сам объект браузера HTML
    // Важно: для работы CEF в Source 2013 этот класс внутри вызывает инициализацию Steam HTML Surface
    m_pHTML = new vgui::HTML(this, "EngineWebBrowser");
    m_pHTML->SetSize(950, 750);
    m_pHTML->SetPos(25, 25);

    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetCloseButtonVisible(true);
    SetSizeable(false);
    SetMoveable(true);

    SetVisible(false);
}

CWebBrowserPanel::~CWebBrowserPanel()
{
}

void CWebBrowserPanel::ApplySchemeSettings(vgui::IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    // Настройки фона панели (опционально)
    SetBgColor(Color(0, 0, 0, 150));
}

void CWebBrowserPanel::OpenURL(const char* URL, const char* postData, bool force)
{
    if (m_pHTML)
    {
        m_pHTML->OpenURL(URL, postData, force);
    }
}

void WebBrowserPanel_In(const CCommand& args)
{
    if (!g_pWebBrowserPanel)
    {
        g_pWebBrowserPanel = new CWebBrowserPanel(vgui::surface()->GetEmbeddedPanel());
    }

    if (g_pWebBrowserPanel && !g_pWebBrowserPanel->IsVisible())
    {
        g_pWebBrowserPanel->SetVisible(true);
        g_pWebBrowserPanel->Activate();

        g_pWebBrowserPanel->SetMouseInputEnabled(true);
        g_pWebBrowserPanel->SetKeyBoardInputEnabled(true);
        vgui::surface()->SetCursor(vgui::dc_arrow);

        g_pWebBrowserPanel->OpenURL("https://www.multverse.ru", "", false);
    }
}

void WebBrowserPanel_Out(const CCommand& args)
{
    if (g_pWebBrowserPanel && g_pWebBrowserPanel->IsVisible())
    {
        g_pWebBrowserPanel->SetVisible(false);

        g_pWebBrowserPanel->SetMouseInputEnabled(false);
        g_pWebBrowserPanel->SetKeyBoardInputEnabled(false);
    }
}

// 2. А теперь регистрируем их в движке как официальные плюс-минус команды через кавычки
static ConCommand webbrowser_plus_cmd("+webbrowser", WebBrowserPanel_In, "Opens web browser");
static ConCommand webbrowser_minus_cmd("-webbrowser", WebBrowserPanel_Out, "Closes web browser");

CON_COMMAND_F(webbrowser_url, "Custom URL for Web Browser", FCVAR_GAMEDLL | FCVAR_CLIENTCMD_CAN_EXECUTE)
{
    const char* pUrl = args.Arg(1);

    if (!g_pWebBrowserPanel)
    {
        g_pWebBrowserPanel = new CWebBrowserPanel(vgui::surface()->GetEmbeddedPanel());
    }

    if (g_pWebBrowserPanel && !g_pWebBrowserPanel->IsVisible())
    {
        g_pWebBrowserPanel->SetVisible(true);
        g_pWebBrowserPanel->Activate();

        g_pWebBrowserPanel->SetMouseInputEnabled(true);
        g_pWebBrowserPanel->SetKeyBoardInputEnabled(true);
        vgui::surface()->SetCursor(vgui::dc_arrow);

        g_pWebBrowserPanel->OpenURL(pUrl, "", false);
    }
}