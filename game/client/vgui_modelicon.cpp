#include "cbase.h"
#include "vgui_modelicon.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include "c_baseanimating.h"
#include "view_shared.h"
#include "ivrenderview.h"
#include "cdll_client_int.h"
#include "engine/ivmodelinfo.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "model_types.h"

// Клиентская "невидимая" сущность: существует только ради того, чтобы нарисовать модель в панели
class C_ModelIconEntity : public C_BaseAnimating
{
    DECLARE_CLASS(C_ModelIconEntity, C_BaseAnimating);
public:
    virtual bool ShouldDraw() { return false; }   // в мире не рисуем
};

CModelIconPanel::CModelIconPanel(vgui::Panel* pParent, const char* pName, const char* pModelName)
    : BaseClass(pParent, pName)
{
    Q_strncpy(m_szModel, pModelName ? pModelName : "", sizeof(m_szModel));
    m_pModel  = NULL;
    m_bFailed = false;
    m_flYaw   = 210.0f;   // три четверти, чтобы модель не смотрела строго в профиль
    m_flFOV   = 30.0f;

    SetPaintBackgroundEnabled(false);
    SetMouseInputEnabled(false);       // клики должны попадать в кнопку под панелью
    SetKeyBoardInputEnabled(false);
}

CModelIconPanel::~CModelIconPanel()
{
    if (m_pModel)
    {
        m_pModel->Remove();
        m_pModel = NULL;
    }
}

bool CModelIconPanel::IsInsideClip()
{
    vgui::Panel* pClip = m_hClip.Get();
    if (!pClip)
        return true;

    int x = 0, y = 0, w, h;
    LocalToScreen(x, y);
    GetSize(w, h);

    int cx = 0, cy = 0, cw, ch;
    pClip->LocalToScreen(cx, cy);
    pClip->GetSize(cw, ch);

    return x >= cx && y >= cy && (x + w) <= (cx + cw) && (y + h) <= (cy + ch);
}

bool CModelIconPanel::CreateModel()
{
    if (m_szModel[0] == '\0')
        return false;

    // FindOrLoadModel работает и для моделей, которые клиент не прекешировал
    const model_t* pModel = modelinfo->FindOrLoadModel(m_szModel);
    if (!pModel)
        return false;

    C_ModelIconEntity* pEnt = new C_ModelIconEntity;
    if (!pEnt->InitializeAsClientEntity(NULL, RENDER_GROUP_OPAQUE_ENTITY))
    {
        pEnt->Release();
        return false;
    }

    pEnt->SetModelPointer(pModel);
    pEnt->AddEffects(EF_NODRAW);   // чтобы рендерер не рисовал её в мире сам
    pEnt->SetSequence(0);

    // --- Автоподбор расстояния и позиции по габаритам модели ---
    Vector mins, maxs;
    modelinfo->GetModelRenderBounds(pModel, mins, maxs);

    Vector center = (mins + maxs) * 0.5f;
    float  radius = (maxs - mins).Length() * 0.5f;
    if (radius < 1.0f)
        radius = 1.0f;

    // Камера в (0,0,0) смотрит вдоль +X; сфера радиуса r должна влезать в конус обзора
    float flDist = radius / sinf(DEG2RAD(m_flFOV * 0.5f)) * 1.05f;

    QAngle ang(0.0f, m_flYaw, 0.0f);
    matrix3x4_t mat;
    AngleMatrix(ang, mat);

    Vector rotCenter;
    VectorRotate(center, mat, rotCenter);

    pEnt->SetAbsAngles(ang);
    pEnt->SetAbsOrigin(Vector(flDist, 0.0f, 0.0f) - rotCenter);

    m_pModel = pEnt;
    return true;
}

void CModelIconPanel::OnThink()
{
    BaseClass::OnThink();

    // OnThink вызывается только у видимых панелей, поэтому закрытое меню ничего не стоит
    if (!m_pModel && !m_bFailed && IsInsideClip())
    {
        if (!CreateModel())
            m_bFailed = true;
    }
}

void CModelIconPanel::Paint()
{
    if (!m_pModel || !IsInsideClip())
        return;

    int x = 0, y = 0, w, h;
    LocalToScreen(x, y);
    GetSize(w, h);
    if (w <= 0 || h <= 0)
        return;

    CMatRenderContextPtr pRenderContext(materials);

    int vpX, vpY, vpW, vpH;
    pRenderContext->GetViewport(vpX, vpY, vpW, vpH);

    CViewSetup view;
    view.x = x + vpX;
    view.y = y + vpY;
    view.width  = w;
    view.height = h;
    view.m_bOrtho = false;
    view.fov = m_flFOV;
    view.origin = vec3_origin;
    view.angles = vec3_angle;
    view.m_flAspectRatio = (float)w / (float)h;
    view.zNear = 1.0f;
    view.zFar  = 4000.0f;

    Frustum dummyFrustum;
    render->Push3DView(view, VIEW_CLEAR_DEPTH, NULL, dummyFrustum);

    // ровный белый свет, иначе модель будет чёрной
    modelrender->SuppressEngineLighting(true);
    float color[3] = { 1.0f, 1.0f, 1.0f };
    render->SetColorModulation(color);
    render->SetBlend(1.0f);

    Vector white[6] =
    {
        Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f),
        Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f), Vector(1.0f, 1.0f, 1.0f),
    };
    g_pStudioRender->SetAmbientLightColors(white);
    g_pStudioRender->SetLocalLights(0, NULL);

    m_pModel->DrawModel(STUDIO_RENDER);

    modelrender->SuppressEngineLighting(false);
    render->PopView(dummyFrustum);
}
