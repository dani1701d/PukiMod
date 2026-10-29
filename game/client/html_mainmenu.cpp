#include "cbase.h"
#include "html_mainmenu.h"
#include <vgui/ISurface.h>
#include <vgui/KeyCode.h>
#include "cdll_int.h"
#include "filesystem.h"
#include "igamesystem.h"
#include "ienginevgui.h"

#include "tier0/memdbgon.h"

extern IVEngineClient* engine;

CHTMLMainMenu* g_pHTMLMainMenu = nullptr;
static float s_flOpenTime = 0.0f;   // когда меню создали
static float s_flCloseTime = 0.0f;   // когда меню закрыли

void CCustomHTML::OnKeyCodeTyped(vgui::KeyCode code)
{
    if (code == KEY_ESCAPE && engine->IsInGame())
    {
        if (Plat_FloatTime() - s_flOpenTime > 0.3f)
            CHTMLMainMenu::CloseMenu();
        return;
    }
    BaseClass::OnKeyCodeTyped(code);
}

void CCustomHTML::OnFinishRequest(const char* url, const char* pageTitle,
    const CUtlMap<CUtlString, CUtlString>& headers)
{
    BaseClass::OnFinishRequest(url, pageTitle, headers);
    CHTMLMainMenu::PushGameState();   // страница загрузилась - отдаём ей состояние
}

// Строки попадают внутрь JS-литерала, поэтому вычищаем всё опасное
static void SanitizeForJS(const char* in, char* out, int outSize)
{
    int j = 0;
    for (; *in && j < outSize - 1; ++in)
    {
        unsigned char c = (unsigned char)*in;
        out[j++] = (c == '"' || c == '\'' || c == '\\' || c == '<' || c < 32) ? '_' : (char)c;
    }
    out[j] = '\0';
}

void CHTMLMainMenu::PushGameState()
{
    if (!g_pHTMLMainMenu)
        return;

    const bool bInGame = engine->IsInGame();
    char map[128] = "", name[128] = "";
    int maxPlayers = 0;

    if (bInGame)
    {
        SanitizeForJS(engine->GetLevelName(), map, sizeof(map));
        maxPlayers = engine->GetMaxClients();

        player_info_t pi;
        if (engine->GetPlayerInfo(engine->GetLocalPlayer(), &pi))
            SanitizeForJS(pi.name, name, sizeof(name));
    }

    char js[512];
    Q_snprintf(js, sizeof(js),
        "window.gameState={inGame:%s,map:\"%s\",maxPlayers:%d,playerName:\"%s\"};"
        "if(window.onGameState)window.onGameState(window.gameState);",
        bInGame ? "true" : "false", map, maxPlayers, name);

    g_pHTMLMainMenu->m_pHTMLPanel->RunJavascript(js);
}

CHTMLMainMenu::CHTMLMainMenu(vgui::Panel* parent, const char* name)
    : BaseClass(parent, name, false)
{
    int w, h;
    engine->GetScreenSize(w, h);

    SetBounds(0, 0, w, h);
    SetTitleBarVisible(false);
    SetCloseButtonVisible(false);
    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetMenuButtonVisible(false);
    SetSizeable(false);
    SetMoveable(false);

    m_pHTMLPanel = new CCustomHTML(this, "HTMLMainMenuPanel");
    m_pHTMLPanel->SetBounds(0, 0, w, h);

    char absolutePath[MAX_PATH];
    if (g_pFullFileSystem && g_pFullFileSystem->GetLocalPath("html/menu.html", absolutePath, sizeof(absolutePath)))
    {
        V_FixSlashes(absolutePath, '/');   // для file:/// надёжнее прямые слэши
        char url[MAX_PATH + 32];
        Q_snprintf(url, sizeof(url), "file:///%s", absolutePath);
        m_pHTMLPanel->OpenURL(url, "");
    }

    SetVisible(true);
    SetKeyBoardInputEnabled(true);
    SetMouseInputEnabled(true);
    MoveToFront();

    s_flOpenTime = Plat_FloatTime();
}

CHTMLMainMenu::~CHTMLMainMenu()
{
    // Обнуляем, только если глобал всё ещё указывает на НАС.
    // Иначе затрём указатель на уже созданное новое меню.
    if (g_pHTMLMainMenu == this)
        g_pHTMLMainMenu = nullptr;
}

void CHTMLMainMenu::PerformLayout()
{
    BaseClass::PerformLayout();

    int w, h;
    GetSize(w, h);
    m_pHTMLPanel->SetBounds(0, 0, w, h);
}

void CHTMLMainMenu::OpenMenu()
{
    if (g_pHTMLMainMenu)
        return;

    // В игре вешаем на клиентскую корневую панель, вне игры - на GameUI
    vgui::VPANEL parent = enginevgui->GetPanel(engine->IsInGame() ? PANEL_CLIENTDLL : PANEL_GAMEUIDLL);

    CHTMLMainMenu* pMenu = new CHTMLMainMenu(NULL, "HTMLMainMenu");
    if (parent)
        pMenu->SetParent(parent);

    g_pHTMLMainMenu = pMenu;
    Msg("[HTMLMenu] opened, inGame=%d, parent=%d\n", engine->IsInGame(), parent != 0);
}

void CHTMLMainMenu::CloseMenu()
{
    if (!g_pHTMLMainMenu)
        return;

    Msg("[HTMLMenu] closed\n");

    CHTMLMainMenu* pMenu = g_pHTMLMainMenu;
    g_pHTMLMainMenu = nullptr;
    s_flCloseTime = Plat_FloatTime();

    pMenu->SetVisible(false);
    pMenu->SetMouseInputEnabled(false);
    pMenu->SetKeyBoardInputEnabled(false);
    pMenu->MarkForDeletion();
}

void CHTMLMainMenu::ToggleMenu()
{
    if (g_pHTMLMainMenu) CloseMenu();
    else OpenMenu();
}

void CHTMLMainMenu::OnKeyCodePressed(vgui::KeyCode code)
{
    // В главном меню (без карты) ESC не должен оставлять пустой экран
    if (code == KEY_ESCAPE && engine->IsInGame())
    {
        CloseMenu();
        return;
    }
    BaseClass::OnKeyCodePressed(code);
}

void CHTMLMainMenu::OnCommand(const char* command)
{
    if (Q_stricmp(command, "Close") == 0)
        CloseMenu();
    else
        BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Жизненный цикл меню
//-----------------------------------------------------------------------------
class CHTMLMenuSystem : public CAutoGameSystemPerFrame
{
public:
    CHTMLMenuSystem() : CAutoGameSystemPerFrame("CHTMLMenuSystem"),
        m_bArmed(false), m_bWaitingHide(false), m_nStockFrames(0), m_flNextPush(0) {
    }

    virtual void PostInit() override { CHTMLMainMenu::OpenMenu(); }
    virtual void Shutdown() override { CHTMLMainMenu::CloseMenu(); }
    virtual void LevelInitPreEntity() override { CHTMLMainMenu::CloseMenu(); m_bArmed = false; m_bWaitingHide = false; m_nStockFrames = 0; m_flNextPush = 0; }
    virtual void LevelShutdownPostEntity() override { m_bArmed = false; m_bWaitingHide = false; m_nStockFrames = 0; m_flNextPush = 0; CHTMLMainMenu::OpenMenu(); }

    virtual void Update(float frametime) override
    {
        if (g_pHTMLMainMenu && Plat_FloatTime() > m_flNextPush)
        {
            m_flNextPush = Plat_FloatTime() + 0.5f;
            CHTMLMainMenu::PushGameState();
        }

        if (!engine->IsInGame() || !enginevgui)
            return;

        const bool bStockVisible = enginevgui->IsGameUIVisible();
        const bool bConsole = engine->Con_IsVisible();

        // Ждём, пока загрузочный экран уйдёт и игра реально пойдёт
        if (!m_bArmed)
        {
            if (!bStockVisible)
                m_bArmed = true;
            return;
        }

        // Мы уже отправили gameui_hide, ждём его выполнения
        if (m_bWaitingHide)
        {
            if (!bStockVisible)
                m_bWaitingHide = false;
            m_nStockFrames = 0;
            return;
        }

        // Стандартное меню видно, а консоль нет - это Esc.
        // Требуем 2 кадра подряд, чтобы не поймать момент,
        // когда консоль уже открылась, а Con_IsVisible() ещё не обновился.
        if (bStockVisible && !bConsole)
            m_nStockFrames++;
        else
            m_nStockFrames = 0;

        if (m_nStockFrames < 2)
            return;

        m_nStockFrames = 0;

        if (g_pHTMLMainMenu)
        {
            CHTMLMainMenu::CloseMenu();
        }
        else if (Plat_FloatTime() - s_flCloseTime > 0.3f)
        {
            CHTMLMainMenu::OpenMenu();
        }

        engine->ClientCmd_Unrestricted("gameui_hide");
        m_bWaitingHide = true;
    }

private:
    bool m_bArmed;        // загрузка завершена, можно реагировать на Esc
    bool m_bWaitingHide;  // ждём, пока стандартное меню реально скроется
    int m_nStockFrames;
    float m_flNextPush;
};
static CHTMLMenuSystem s_HTMLMenuSystem;

CON_COMMAND(toggle_html_menu, "Opens or closes the HTML main menu")
{
    CHTMLMainMenu::ToggleMenu();
}