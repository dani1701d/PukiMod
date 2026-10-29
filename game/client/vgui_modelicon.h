#ifndef VGUI_MODELICON_H
#define VGUI_MODELICON_H
#pragma once

#include <vgui_controls/Panel.h>

class C_ModelIconEntity;

// Панель, которая рисует 3D-модель (.mdl) вместо готовой текстуры-иконки.
// Модель создаётся лениво, только когда панель реально видна в списке.
class CModelIconPanel : public vgui::Panel
{
    DECLARE_CLASS_SIMPLE(CModelIconPanel, vgui::Panel);

public:
    CModelIconPanel(vgui::Panel* pParent, const char* pName, const char* pModelName);
    virtual ~CModelIconPanel();

    // Панель-"окошко" прокрутки: иконка рисуется, только если целиком внутри неё
    // (3D-вид не обрезается вгуевским клипом, поэтому частично видимые иконки пропускаем)
    void SetClipPanel(vgui::Panel* pClip) { m_hClip.Set(pClip); }

    // Угол, под которым модель повёрнута к камере (в градусах)
    void SetViewYaw(float flYaw) { m_flYaw = flYaw; }

    virtual void OnThink() override;
    virtual void Paint() override;

private:
    bool CreateModel();
    bool IsInsideClip();

    char               m_szModel[MAX_PATH];
    C_ModelIconEntity* m_pModel;
    vgui::PHandle      m_hClip;
    bool               m_bFailed;
    float              m_flYaw;
    float              m_flFOV;
};

#endif // VGUI_MODELICON_H
