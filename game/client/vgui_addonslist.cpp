#include "cbase.h"
#include "vgui_addonslist.h"
#include "filesystem.h"
#include "luamanager.h"

// Подключаем необходимые интерфейсы VGUI
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ListPanel.h>

CAddonsListPanel* g_pAddonsListPanel = nullptr;

CAddonsListPanel::CAddonsListPanel(vgui::VPANEL parent) : BaseClass(nullptr, "AddonsList")
{
    // Привязываем панель к родителю (например, к viewport игры)
    SetParent(parent);

    SetSize(500, 800);
    MoveToCenterOfScreen();
    SetTitle("Addons List", true);

    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetCloseButtonVisible(true);
    SetSizeable(false);
    SetMoveable(true);
    SetVisible(true);

    GetAddonsList();
}

CAddonsListPanel::~CAddonsListPanel()
{
}

void CAddonsListPanel::GetAddonsList()
{
    m_pListPanel = new vgui::ListPanel(this, "AddonsListPanel");

    // Set bounds: X, Y, Width, Height
    m_pListPanel->SetBounds(20, 30, 460, 750);

    // Optional settings
    m_pListPanel->SetMultiselectEnabled(false); // Disallow selecting multiple rows
    m_pListPanel->SetVisible(true);

    // Parameters: Column ID (int), Key Name, Column Header Text, Flags, Width
    m_pListPanel->AddColumnHeader(0, "name", "File name", 350, vgui::ListPanel::COLUMN_FIXEDSIZE);
    m_pListPanel->AddColumnHeader(1, "type", "Type", 110, vgui::ListPanel::COLUMN_FIXEDSIZE);

    m_pListPanel->RemoveAll();

    // добавляем клиентские аддоны
    FileFindHandle_t clientFindHandle;
    const char* pClientFileName = filesystem->FindFirstEx("lua/addons/client/*.lua", "GAME", &clientFindHandle);

    while (pClientFileName != NULL)
    {
        KeyValues* pRowData = new KeyValues("ItemData");

        pRowData->SetString("name", pClientFileName);
        pRowData->SetString("type", "Client");

        m_pListPanel->AddItem(pRowData, 0, false, false);
        pRowData->deleteThis();

        pClientFileName = filesystem->FindNext(clientFindHandle);
    }

    // добавляем серверные аддоны
    FileFindHandle_t serverFindHandle;
    const char* pServerFileName = filesystem->FindFirstEx("lua/addons/server/*.lua", "GAME", &serverFindHandle);

    while (pServerFileName != NULL)
    {
        KeyValues* pRowData = new KeyValues("ItemData");

        pRowData->SetString("name", pServerFileName);
        pRowData->SetString("type", "Server");

        m_pListPanel->AddItem(pRowData, 0, false, false);
        pRowData->deleteThis();

        pServerFileName = filesystem->FindNext(serverFindHandle);
    }
}

void CAddonsListPanel::ApplySchemeSettings(vgui::IScheme* pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);
    // Настройки фона панели (опционально)
    SetBgColor(Color(0, 0, 0, 150));
}

CON_COMMAND(lua_addonslist_ui, "Opens Addons List")
{
    // 1. Если панель еще не создана — создаем ее
    if (!g_pAddonsListPanel)
    {
        // Привязываем к главному экрану (vgui::surface()->GetEmbeddedPanel())
        g_pAddonsListPanel = new CAddonsListPanel(vgui::surface()->GetEmbeddedPanel());
    }

    if (g_pAddonsListPanel)
    {
        // 2. Если она уже открыта — закрываем (тоггл), если скрыта — показываем
        if (g_pAddonsListPanel->IsVisible())
        {
            g_pAddonsListPanel->SetVisible(false);
        }
        else
        {
            g_pAddonsListPanel->SetVisible(true);
            g_pAddonsListPanel->Activate();

            // Включаем мышь и клавиатуру, чтобы можно было кликать и писать в строку
            g_pAddonsListPanel->SetMouseInputEnabled(true);
            g_pAddonsListPanel->SetKeyBoardInputEnabled(true);
        }
    }
}