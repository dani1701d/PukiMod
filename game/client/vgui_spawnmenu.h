#pragma once
#ifndef VGUI_SPAWNMENU_H
#define VGUI_SPAWNMENU_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/PanelListPanel.h>

// Класс спавн-меню, унаследованный от базового фрейма VGUI
class CSpawnMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE(CSpawnMenu, vgui::Frame);

public:
    CSpawnMenu(vgui::VPANEL parent);
    ~CSpawnMenu();

    // Перехват кликов по кнопкам иконкам предметов
    virtual void OnCommand(const char* command) override;

    // Загрузка категорий и пропов из текстового конфигурационного файла
    void LoadItemsFromConfig();

private:
    // Главный прокручиваемый вертикальный список, в который мы складываем сетку
    vgui::PanelListPanel* m_pMainScrollList;
};

// Объявляем глобальный указатель, чтобы к нему был доступ из команд +spawnmenu и -spawnmenu
extern CSpawnMenu* g_pSpawnMenu;

#endif // VGUI_SPAWNMENU_H