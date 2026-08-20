#include "cbase.h"
#include "vgui_spawnmenu.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Tooltip.h>
#include <filesystem.h>
#include "KeyValues.h"

CSpawnMenu* g_pSpawnMenu = nullptr;

CSpawnMenu::CSpawnMenu(vgui::VPANEL parent) : BaseClass(nullptr, "SpawnMenu")
{
    SetParent(parent);

    // Сделаем меню шире, чтобы сетка иконок смотрелась красиво
    SetSize(1000, 800);
    MoveToCenterOfScreen();
    SetTitle("Spawn Menu", true);

    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetCloseButtonVisible(true);
    SetSizeable(false);
    SetMoveable(true);

    // Главный прокручиваемый вертикальный список (контейнер для категорий и рядов сетки)
    m_pMainScrollList = new vgui::PanelListPanel(this, "MainScrollList");
    m_pMainScrollList->SetBounds(10, 40, 980, 750);
    m_pMainScrollList->SetFirstColumnWidth(0);

    LoadItemsFromConfig();

    SetVisible(false);
}

CSpawnMenu::~CSpawnMenu()
{
}

void CSpawnMenu::LoadItemsFromConfig()
{
    KeyValues* pKV = new KeyValues("SpawnMenuItems");

    if (pKV->LoadFromFile(g_pFullFileSystem, "scripts/spawn_items.txt", "MOD"))
    {
        // Проходим по всем главным ключам в файле — это наши категории (заголовки)
        for (KeyValues* pCategory = pKV->GetFirstSubKey(); pCategory != nullptr; pCategory = pCategory->GetNextKey())
        {
            const char* pCategoryName = pCategory->GetName(); // Например: "Категория: Строительство"

            // 1. Создаем панель-заголовок для группы
            vgui::Label* pHeaderLabel = new vgui::Label(m_pMainScrollList, "CategoryHeader", pCategoryName);
            pHeaderLabel->SetSize(480, 30);

            // Настраиваем внешний вид заголовка (шрифт выделится автоматически через схемы VGUI)
            pHeaderLabel->SetContentAlignment(vgui::Label::a_west);

            // Добавляем заголовок в главный скролл-список
            m_pMainScrollList->AddItem(nullptr, pHeaderLabel);

            // Параметры для формирования горизонтальной сетки
            int iMaxColumns = 12;       // Количество иконок в одном ряду
            int iCurrentColumn = 0;
            int iIconSize = 64;        // Размер квадратной иконки (64x64px)
            int iPadding = 10;         // Расстояние между иконками

            vgui::Panel* pRowPanel = nullptr;

            // Проходим по всем элементам (пропам) внутри текущей категории
            for (KeyValues* pItem = pCategory->GetFirstSubKey(); pItem != nullptr; pItem = pItem->GetNextKey())
            {
                const char* pIconPath = pItem->GetString("icon", "vgui/hud/xbox_reticle");

                // Считываем тип элемента (по умолчанию "prop")
                const char* pType = pItem->GetString("type", "prop");

                if (iCurrentColumn == 0 || pRowPanel == nullptr)
                {
                    pRowPanel = new vgui::Panel(m_pMainScrollList, "GridRow");
                    pRowPanel->SetSize(480, iIconSize + iPadding);
                    m_pMainScrollList->AddItem(nullptr, pRowPanel);
                }

                vgui::Button* pIconBtn = new vgui::Button(pRowPanel, "SpawnIconButton", "");
                int xPos = iCurrentColumn * (iIconSize + iPadding);
                pIconBtn->SetBounds(xPos, 0, iIconSize, iIconSize);

                pIconBtn->SetPaintBackgroundEnabled(false);
                pIconBtn->SetPaintBorderEnabled(true);

                // === НАШ НОВЫЙ КОД ДЛЯ ТУЛТИПОВ ===
                // 1. Получаем имя предмета (например, "Деревянный ящик" или "Сварка")
                const char* pItemName = pItem->GetName();

                // 2. Передаем это имя встроенному тултипу кнопки
                if (pItemName && pItemName[0] != '\0')
                {
                    pIconBtn->GetTooltip()->SetText(pItemName);
                    pIconBtn->GetTooltip()->SetEnabled(true);
                }
                // ==================================

                vgui::ImagePanel* pIconImage = new vgui::ImagePanel(pIconBtn, "SpawnIconImage");
                pIconImage->SetBounds(0, 0, iIconSize, iIconSize);
                pIconImage->SetImage(pIconPath);
                pIconImage->SetShouldScaleImage(true);
                pIconImage->SetMouseInputEnabled(false);

                pIconBtn->SetKeyBoardInputEnabled(false);

                // НАДЕЖНАЯ НАСТРОЙКА КОМАНДЫ С ЗАЩИТОЙ ОТ ОШИБОК
                char szCommand[256];
                if (Q_strcmp(pType, "weapon") == 0)
                {
                    const char* pWeaponClass = pItem->GetString("class", "weapon_smg1");
                    Q_snprintf(szCommand, sizeof(szCommand), "spawn_item weapon %s", pWeaponClass);
                }
                else if (Q_strcmp(pType, "npc") == 0)
                {
                    const char* pNPCClass = pItem->GetString("class", "npc_zombie");
                    Q_snprintf(szCommand, sizeof(szCommand), "spawn_item npc %s", pNPCClass);
                }
                else if (Q_strcmp(pType, "vehicle") == 0)
                {
                    const char* pVehicleClass = pItem->GetString("class", "prop_vehicle_airboat");
                    Q_snprintf(szCommand, sizeof(szCommand), "spawn_item vehicle %s", pVehicleClass);
                }
                else if (Q_strcmp(pType, "tool") == 0)
                {
                    const char* pToolMode = pItem->GetString("mode", "0");
                    // Команда сначала выдаст игроку тулган, а затем переключит режим ConVar
                    Q_snprintf(szCommand, sizeof(szCommand), "use_tool %s", pToolMode);
                }
                else
                {
                    const char* pModelPath = pItem->GetString("model", "models/props_junk/wood_crate001a.mdl");
                    Q_snprintf(szCommand, sizeof(szCommand), "spawn_item prop %s", pModelPath);
                }

                pIconBtn->SetCommand(szCommand);
                pIconBtn->AddActionSignalTarget(this);

                iCurrentColumn++;
                if (iCurrentColumn >= iMaxColumns)
                {
                    iCurrentColumn = 0;
                }
            }
        }
    }
    else
    {
        Warning("SpawnMenu: Не удалось загрузить файл scripts/spawn_items.txt!\n");
    }

    pKV->deleteThis();
}

void SpawnMenu_In(const CCommand& args)
{
    if (!g_pSpawnMenu)
    {
        g_pSpawnMenu = new CSpawnMenu(vgui::surface()->GetEmbeddedPanel());
    }

    if (g_pSpawnMenu && !g_pSpawnMenu->IsVisible())
    {
        g_pSpawnMenu->SetVisible(true);
        g_pSpawnMenu->Activate();

        g_pSpawnMenu->SetMouseInputEnabled(true);
        g_pSpawnMenu->SetKeyBoardInputEnabled(true);
        vgui::surface()->SetCursor(vgui::dc_arrow);
    }
}

void SpawnMenu_Out(const CCommand& args)
{
    if (g_pSpawnMenu && g_pSpawnMenu->IsVisible())
    {
        g_pSpawnMenu->SetVisible(false);

        g_pSpawnMenu->SetMouseInputEnabled(false);
        g_pSpawnMenu->SetKeyBoardInputEnabled(false);
    }
}

// 2. А теперь регистрируем их в движке как официальные плюс-минус команды через кавычки
static ConCommand spawnmenu_plus_cmd("+spawnmenu", SpawnMenu_In, "Opens Spawn Menu");
static ConCommand spawnmenu_minus_cmd("-spawnmenu", SpawnMenu_Out, "Closes Spawn Menu");

void CSpawnMenu::OnCommand(const char* command)
{
    if (Q_strnicmp(command, "spawn_item ", 11) == 0)
    {
        // Просто пробрасываем всю строку команды на сервер
        engine->ClientCmd_Unrestricted(command);
    }
    else
    {
        BaseClass::OnCommand(command);
    }
    if (Q_strnicmp(command, "use_tool ", 9) == 0)
    {
        const char* pMode = command + 9;

        // Сначала достаем тулган
        engine->ClientCmd_Unrestricted("give weapon_toolgun");

        // Передаем команду смены режима нашему кастомному серверному обработчику
        char szServerCmd[64];
        Q_snprintf(szServerCmd, sizeof(szServerCmd), "toolgun_mode %s", pMode);
        engine->ClientCmd_Unrestricted(szServerCmd);
    }
}
