//========= Copyright Valve Corporation, All rights reserved. ============//
// Purpose: Minecraft x Source 2013 passthrough - server plugin skeleton
//===========================================================================//

#include <stdio.h>
#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "eiface.h"
#include "igameevents.h"
#include "convar.h"
#include "Color.h"
#include "vstdlib/random.h"
#include "engine/IEngineTrace.h"
#include "tier2/tier2.h"
#include "game/server/pluginvariant.h"
#include "game/server/iplayerinfo.h"
#include "game/server/ientityinfo.h"
#include "game/server/igameinfo.h"
#include "tier0/memdbgon.h"

IVEngineServer *engine = NULL;
IGameEventManager *gameeventmanager = NULL;
IPlayerInfoManager *playerinfomanager = NULL;
IEntityInfoManager *entityinfomanager = NULL;
IGameInfoManager *gameinfomanager = NULL;
IBotManager *botmanager = NULL;
IServerPluginHelpers *helpers = NULL;
IUniformRandomStream *randomStr = NULL;
IEngineTrace *enginetrace = NULL;
CGlobalVars *gpGlobals = NULL;

class CMc2SourceServerPlugin : public IServerPluginCallbacks, public IGameEventListener
{
public:
CMc2SourceServerPlugin();
virtual ~CMc2SourceServerPlugin();

virtual bool Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
virtual void Unload( void );
virtual void Pause( void );
virtual void UnPause( void );
virtual const char *GetPluginDescription( void );
virtual void LevelInit( char const *pMapName );
virtual void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
virtual void GameFrame( bool simulating );
virtual void LevelShutdown( void );
virtual void ClientActive( edict_t *pEntity );
virtual void ClientDisconnect( edict_t *pEntity );
virtual void ClientPutInServer( edict_t *pEntity, char const *playername );
virtual void SetCommandClient( int index );
virtual void ClientSettingsChanged( edict_t *pEdict );
virtual PLUGIN_RESULT ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
virtual PLUGIN_RESULT ClientCommand( edict_t *pEntity, const CCommand &args );
virtual PLUGIN_RESULT NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
};

CMc2SourceServerPlugin g_Mc2SourceServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CMc2SourceServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_Mc2SourceServerPlugin );

CMc2SourceServerPlugin::CMc2SourceServerPlugin() {}
CMc2SourceServerPlugin::~CMc2SourceServerPlugin() {}

bool CMc2SourceServerPlugin::Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
ConnectTier1Libraries( &interfaceFactory, 1 );
ConnectTier2Libraries( &interfaceFactory, 1 );
entityinfomanager = (IEntityInfoManager *)gameServerFactory(INTERFACEVERSION_ENTITYINFOMANAGER,NULL);
playerinfomanager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER,NULL);
botmanager = (IBotManager *)gameServerFactory(INTERFACEVERSION_PLAYERBOTMANAGER, NULL);
gameinfomanager = (IGameInfoManager *)gameServerFactory(INTERFACEVERSION_GAMEINFOMANAGER, NULL);
engine = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);
gameeventmanager = (IGameEventManager *)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER,NULL);
helpers = (IServerPluginHelpers*)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, NULL);
enginetrace = (IEngineTrace *)interfaceFactory(INTERFACEVERSION_ENGINETRACE_SERVER,NULL);
randomStr = (IUniformRandomStream *)interfaceFactory(VENGINE_SERVER_RANDOM_INTERFACE_VERSION, NULL);
if( ! ( engine && gameeventmanager && g_pFullFileSystem && helpers && enginetrace && randomStr ) )
return false;
if ( playerinfomanager )
gpGlobals = playerinfomanager->GetGlobalVars();
MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
ConVar_Register( 0 );
Msg("[mc2source] server plugin loaded\n");
return true;
}

void CMc2SourceServerPlugin::Unload( void )
{
gameeventmanager->RemoveListener( this );
ConVar_Unregister( );
DisconnectTier2Libraries( );
DisconnectTier1Libraries( );
}

void CMc2SourceServerPlugin::Pause( void ) {}
void CMc2SourceServerPlugin::UnPause( void ) {}
const char *CMc2SourceServerPlugin::GetPluginDescription( void ) { return "MC2Source server plugin"; }
void CMc2SourceServerPlugin::LevelInit( char const *pMapName ) { Msg("[mc2source] level %s\n", pMapName); }
void CMc2SourceServerPlugin::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) {}
void CMc2SourceServerPlugin::GameFrame( bool simulating )
{
if ( simulating )
{
// TODO: accept Minecraft camera JSON over localhost WebSocket and sync camera/entities
}
}
void CMc2SourceServerPlugin::LevelShutdown( void ) { gameeventmanager->RemoveListener( this ); }
void CMc2SourceServerPlugin::ClientActive( edict_t *pEntity ) {}
void CMc2SourceServerPlugin::ClientDisconnect( edict_t *pEntity ) {}
void CMc2SourceServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername ) {}
void CMc2SourceServerPlugin::SetCommandClient( int index ) {}
void CMc2SourceServerPlugin::ClientSettingsChanged( edict_t *pEdict ) {}
PLUGIN_RESULT CMc2SourceServerPlugin::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CMc2SourceServerPlugin::ClientCommand( edict_t *pEntity, const CCommand &args ) { return PLUGIN_CONTINUE; }
PLUGIN_RESULT CMc2SourceServerPlugin::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID ) { return PLUGIN_CONTINUE; }
