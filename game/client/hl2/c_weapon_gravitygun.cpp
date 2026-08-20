//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "hud.h"
#include "in_buttons.h"
#include "beamdraw.h"
#include "c_weapon__stubs.h"
#include "clienteffectprecachesystem.h"
#include "iviewrender_beams.h"  // Отвечает за интерфейс 'beams' и структуру BeamInfo_t
#include "const.h"              // Содержит флаги и типы лучей, включая TE_BEAMPOINTS
#include "fx.h"
#include <vgui_controls/Controls.h>
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectGravityGun)
CLIENTEFFECT_MATERIAL("sprites/physbeam")
CLIENTEFFECT_REGISTER_END()

class C_BeamQuadratic : public CDefaultClientRenderable
{
public:
	C_BeamQuadratic();
	void							Update(C_BaseEntity* pOwner);

	// IClientRenderable
	virtual const Vector&			GetRenderOrigin(void) { return m_targetPosition; }
	virtual const QAngle&			GetRenderAngles(void) { return vec3_angle; }
	virtual bool					ShouldDraw(void) { return true; }
	virtual bool					IsTransparent(void) { return true; }
	virtual bool					ShouldReceiveProjectedTextures(int flags) { return false; }
	virtual int						DrawModel(int flags);

	const matrix3x4_t& RenderableToWorldTransform()
	{
		static matrix3x4_t mat;
		SetIdentityMatrix(mat);
		PositionMatrix(GetRenderOrigin(), mat);
		return mat;
	}

	// Returns the bounds relative to the origin (render bounds)
	virtual void	GetRenderBounds(Vector& mins, Vector& maxs)
	{
		// bogus.  But it should draw if you can see the end point
		mins.Init(-32, -32, -32);
		maxs.Init(32, 32, 32);
	}

	C_BaseEntity*			m_pOwner;
	Vector					m_targetPosition;
	Vector					m_worldPosition;
	int						m_active;
	int						m_glueTouching;
	int						m_viewModelIndex;
};


class C_WeaponGravityGun : public C_BaseCombatWeapon
{
	DECLARE_CLASS(C_WeaponGravityGun, C_BaseCombatWeapon);
public:
	C_WeaponGravityGun() {}

	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	int KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
	{
		if (gHUD.m_iKeyBits & IN_ATTACK)
		{
			switch (keynum)
			{
			case MOUSE_WHEEL_UP:
				gHUD.m_iKeyBits |= IN_WEAPON1;
				return 0;

			case MOUSE_WHEEL_DOWN:
				gHUD.m_iKeyBits |= IN_WEAPON2;
				return 0;
			}
		}

		// Allow engine to process
		return BaseClass::KeyInput(down, keynum, pszCurrentBinding);
	}

	void OnDataChanged(DataUpdateType_t updateType)
	{
		BaseClass::OnDataChanged(updateType);
		m_beam.Update(this);

		if (updateType == DATA_UPDATE_CREATED)
		{
			// Разрешаем объекту думать на клиенте ВСЕГДА, 
			// независимо от того, локальный это игрок или чужой
			SetNextClientThink(CLIENT_THINK_ALWAYS);
		}
	}

	float					m_flMouseDeltaX;
	float					m_flMouseDeltaY;
	QAngle					m_vecPlayerLastAngles;
	bool					m_bIsRotating;
	bool					m_bBeamActive;
	Vector					m_vBeamTarget;
	virtual int				DrawModel(int flags);

private:
	C_WeaponGravityGun(const C_WeaponGravityGun&);
	
	C_BeamQuadratic	m_beam;
};

STUB_WEAPON_CLASS_IMPLEMENT(weapon_physgun, C_WeaponGravityGun);

IMPLEMENT_CLIENTCLASS_DT(C_WeaponGravityGun, DT_WeaponGravityGun, CWeaponGravityGun)
RecvPropVector(RECVINFO_NAME(m_beam.m_targetPosition, m_targetPosition)),
RecvPropVector(RECVINFO_NAME(m_beam.m_worldPosition, m_worldPosition)),
RecvPropInt(RECVINFO_NAME(m_beam.m_active, m_active)),
RecvPropInt(RECVINFO_NAME(m_beam.m_glueTouching, m_glueTouching)),
RecvPropInt(RECVINFO_NAME(m_beam.m_viewModelIndex, m_viewModelIndex)),
RecvPropBool(RECVINFO(m_bBeamActive)),
RecvPropVector(RECVINFO(m_vBeamTarget)),
RecvPropBool(RECVINFO(m_bIsRotating)),
RecvPropQAngles(RECVINFO(m_vecPlayerLastAngles)),
END_RECV_TABLE()


C_BeamQuadratic::C_BeamQuadratic()
{
	m_pOwner = NULL;
	m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE;
}

void C_BeamQuadratic::Update(C_BaseEntity* pOwner)
{
	m_pOwner = pOwner;
	if (m_active)
	{
		if (m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE)
		{
			ClientLeafSystem()->AddRenderable(this, RENDER_GROUP_TRANSLUCENT_ENTITY);
		}
		else
		{
			ClientLeafSystem()->RenderableChanged(m_hRenderHandle);
		}
	}
	else if (!m_active && m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE)
	{
		ClientLeafSystem()->RemoveRenderable(m_hRenderHandle);
		m_hRenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	}
}


int	C_BeamQuadratic::DrawModel(int)
{
	Vector points[3];
	QAngle tmpAngle;

	if (!m_active)
		return 0;

	C_BaseEntity* pEnt = cl_entitylist->GetEnt(m_viewModelIndex);
	if (!pEnt)
		return 0;
	pEnt->GetAttachment(1, points[0], tmpAngle);

	points[1] = 0.5 * (m_targetPosition + points[0]);

	// a little noise 11t & 13t should be somewhat non-periodic looking
	//points[1].z += 4*sin( gpGlobals->curtime*11 ) + 5*cos( gpGlobals->curtime*13 );
	points[2] = m_worldPosition;

	IMaterial* pMat = materials->FindMaterial("sprites/physbeam", TEXTURE_GROUP_CLIENT_EFFECTS);
	Vector color;
	if (m_glueTouching)
	{
		color.Init(1, 0, 0);
	}
	else
	{
		color.Init(1, 1, 1);
	}

	float scrollOffset = gpGlobals->curtime - (int)gpGlobals->curtime;
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->Bind(pMat);
	DrawBeamQuadratic(points[0], points[1], points[2], 13, color, scrollOffset);
	return 1;
}

int C_WeaponGravityGun::DrawModel(int flags)
{
	// Сначала даем нарисовать саму модель пушки
	int removed = BaseClass::DrawModel(flags);

	// ЖЕСТКИЙ ТЕСТ: Рисуем столб света каждую секунду от позиции оружия вверх
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_nModelIndex = modelinfo->GetModelIndex("sprites/physbeam.vmt");
	beamInfo.m_nHaloIndex = -1;
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.001f; // Пусть живет целую секунду
	beamInfo.m_flWidth = 5.0f;
	beamInfo.m_flEndWidth = 5.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 0.0f;
	beamInfo.m_flBrightness = 255.0f;
	beamInfo.m_flSpeed = 0.0f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.0f;
	beamInfo.m_flRed = 150;
	beamInfo.m_flGreen = 150;
	beamInfo.m_flBlue = 255; // Белый цвет
	beamInfo.m_nSegments = -1;
	beamInfo.m_bRenderable = true;

	beamInfo.m_vecStart = GetAbsOrigin() + Vector(-40, -6, 0);
	beamInfo.m_vecEnd = m_vBeamTarget;

	beams->CreateBeamPoints(beamInfo);

	beams->ShutdownBeams();

	return removed;
}