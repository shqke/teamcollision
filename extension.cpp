#include "extension.h"

CTeamCollision g_TeamCollision;
SMEXT_LINK(&g_TeamCollision);

SH_DECL_MANUALHOOK1(CBasePlayer_PlayerSolidMask, 0, 0, 0, unsigned int, bool);
SH_DECL_MANUALHOOK2(CGameMovement_PlayerSolidMask, 0, 0, 0, unsigned int, bool, CBasePlayer*);
SH_DECL_MANUALHOOK2(CBaseEntity_ShouldCollide, 0, 0, 0, bool, int, int);

IBinTools* bintools = NULL;
CGameMovement* g_pGameMovement = NULL;
IServerTools* servertools = NULL;
IServerGameEnts* gameents = NULL;

int CBaseEntity::vtblindex_ShouldCollide = 0;
int CBasePlayer::sendprop_m_fFlags = 0;
int CBasePlayer::sendprop_m_carryVictim = 0;
int CBasePlayer::sendprop_m_jockeyAttacker = 0;
int CBasePlayer::vtblindex_PlayerSolidMask = 0;
ICallWrapper* CBasePlayer::vcall_PlayerSolidMask = NULL;
int CGameMovement::vtblindex_PlayerSolidMask = 0;

CTeamCollision::CTeamCollision()
{
	shookid_CGameMovement_PlayerSolidMask = 0;
	shookid_CEnv_Blocker_ShouldCollide = 0;
	shookid_CEnvPhysicsBlocker_ShouldCollide = 0;
}

unsigned int CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask(bool brushOnly)
{
	CBasePlayer* _this = META_IFACEPTR(CBasePlayer);

	CBasePlayer* pVictim = _this->GetCarryVictim();
	if (pVictim == NULL) {
		pVictim = _this->GetJockeyAttacker();
	}

	if (pVictim == NULL) {
		RETURN_META_VALUE(MRES_IGNORED, 0);
	}

	// rww: recursion guard
	RETURN_META_VALUE(MRES_OVERRIDE, META_RESULT_ORIG_RET(unsigned int) | pVictim->PlayerSolidMask(brushOnly));
}

unsigned int CTeamCollision::Handler_CTerrorGameMovement_PlayerSolidMask(bool brushOnly, CBasePlayer* testPlayer)
{
	if (testPlayer == NULL) {
		testPlayer = META_IFACEPTR(CGameMovement)->player;
	}

	if (testPlayer == NULL) {
		RETURN_META_VALUE(MRES_OVERRIDE, 0);
	}

	RETURN_META_VALUE(MRES_OVERRIDE, META_RESULT_ORIG_RET(unsigned int) | testPlayer->PlayerSolidMask(brushOnly));
}

bool CTeamCollision::Handler_CEnvBlocker_ShouldCollide(int collisionGroup, int contentsMask)
{
	// Add contents flag "alternatives" that original blocker method would recognize
	if ((contentsMask & CONTENTS_PLAYERCLIP) != 0) {
		contentsMask |= CONTENTS_TEAM1;
	}
	
	if ((contentsMask & CONTENTS_MONSTERCLIP) != 0) {
		contentsMask |= CONTENTS_TEAM2;
	}

	RETURN_META_VALUE_MNEWPARAMS(MRES_IGNORED, false, CBaseEntity_ShouldCollide, (collisionGroup, contentsMask));
}

bool CTeamCollision::SetupFromGameConfigs(char* error, int maxlength)
{
	IGameConfig* gc = NULL;
	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &gc, error, maxlength)) {
		return false;
	}

	static const struct {
		const char* key;
		int& offset;
	} s_offsets[] = {
		{ "CBasePlayer::PlayerSolidMask", CBasePlayer::vtblindex_PlayerSolidMask },
		{ "CGameMovement::PlayerSolidMask", CGameMovement::vtblindex_PlayerSolidMask },
	};

	for (auto&& el : s_offsets) {
		if (!gc->GetOffset(el.key, &el.offset)) {
			ke::SafeSprintf(error, maxlength, "Unable to get offset for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			gameconfs->CloseGameConfigFile(gc);

			return false;
		}
	}

	gameconfs->CloseGameConfigFile(gc);

	if (!gameconfs->LoadGameConfigFile("sdkhooks.games", &gc, error, maxlength)) {
		return false;
	}

	if (!gc->GetOffset("ShouldCollide", &CBaseEntity::vtblindex_ShouldCollide)) {
		ke::SafeSprintf(error, maxlength, "Unable to get offset for \"ShouldCollide\" from game config (folder: \"sdkhooks.games\")");

		gameconfs->CloseGameConfigFile(gc);

		return false;
	}

	gameconfs->CloseGameConfigFile(gc);

	return true;
}

void CTeamCollision::OnClientPutInServer(int client)
{
	IGamePlayer* pGamePlayer = playerhelpers->GetGamePlayer(client);
	if (pGamePlayer == NULL) {
		return;
	}

	CBaseEntity* pEntity = gameents->EdictToBaseEntity(pGamePlayer->GetEdict());
	if (pEntity == NULL) {
		return;
	}

	SH_ADD_MANUALHOOK(CBasePlayer_PlayerSolidMask, pEntity, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask), true);
}

void CTeamCollision::OnClientDisconnecting(int client)
{
	IGamePlayer* pGamePlayer = playerhelpers->GetGamePlayer(client);
	if (pGamePlayer == NULL) {
		return;
	}

	CBaseEntity* pEntity = gameents->EdictToBaseEntity(pGamePlayer->GetEdict());
	if (pEntity == NULL) {
		return;
	}

	SH_REMOVE_MANUALHOOK(CBasePlayer_PlayerSolidMask, pEntity, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask), true);
}

void CTeamCollision::OnServerActivated(int maxClients)
{
	for (int i = 1; i <= maxClients; i++) {
		IGamePlayer* pGamePlayer = playerhelpers->GetGamePlayer(i);
		if (pGamePlayer == NULL) {
			continue;
		}

		if (!pGamePlayer->IsInGame()) {
			continue;
		}

		OnClientPutInServer(i);
	}
}

bool CTeamCollision::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
	sm_sendprop_info_t info;
	if (!gamehelpers->FindSendPropInfo("CBasePlayer", "m_fFlags", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CBasePlayer::m_fFlags\"");

		return false;
	}

	CBasePlayer::sendprop_m_fFlags = info.actual_offset;

	if (!gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_isGhost", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CTerrorPlayer::m_isGhost\"");

		return false;
	}

	CTerrorPlayer::sendprop_m_carryVictim = info.actual_offset;

	if (!gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_jockeyAttacker", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CTerrorPlayer::m_jockeyAttacker\"");

		return false;
	}

	CTerrorPlayer::sendprop_m_jockeyAttacker = info.actual_offset;

	if (!SetupFromGameConfigs(error, maxlength)) {
		return false;
	}

	SH_MANUALHOOK_RECONFIGURE(CBasePlayer_PlayerSolidMask, CBasePlayer::vtblindex_PlayerSolidMask, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(CGameMovement_PlayerSolidMask, CGameMovement::vtblindex_PlayerSolidMask, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(CBaseEntity_ShouldCollide, CBaseEntity::vtblindex_ShouldCollide, 0, 0);

	sharesys->AddDependency(myself, "bintools.ext", true, true);

	return true;
}

void CTeamCollision::SDK_OnUnload()
{
	if (CBasePlayer::vcall_PlayerSolidMask != NULL) {
		CBasePlayer::vcall_PlayerSolidMask->Destroy();
		CBasePlayer::vcall_PlayerSolidMask = NULL;
	}

	int maxClients = playerhelpers->GetMaxClients();
	for (int i = 1; i <= maxClients; i++) {
		IGamePlayer* pGamePlayer = playerhelpers->GetGamePlayer(i);
		if (pGamePlayer == NULL) {
			continue;
		}

		if (!pGamePlayer->IsInGame()) {
			continue;
		}

		OnClientDisconnecting(i);
	}

	playerhelpers->RemoveClientListener(this);

	SH_REMOVE_HOOK_ID(shookid_CGameMovement_PlayerSolidMask);
	shookid_CGameMovement_PlayerSolidMask = 0;

	SH_REMOVE_HOOK_ID(shookid_CEnv_Blocker_ShouldCollide);
	shookid_CEnv_Blocker_ShouldCollide = 0;

	SH_REMOVE_HOOK_ID(shookid_CEnvPhysicsBlocker_ShouldCollide);
	shookid_CEnvPhysicsBlocker_ShouldCollide = 0;
}

void CTeamCollision::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);

	SourceMod::PassInfo params[] = {
#if SMINTERFACE_BINTOOLS_VERSION == 4
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(unsigned int), NULL, 0 },
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0 },
#else
		// sm1.9- support
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(unsigned int) },
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int) },
#endif
	};

	CBasePlayer::vcall_PlayerSolidMask = bintools->CreateVCall(CBasePlayer::vtblindex_PlayerSolidMask, 0, 0, &params[0], &params[1], 1);
	if (CBasePlayer::vcall_PlayerSolidMask == NULL) {
		smutils->LogError(myself, "Unable to create ICallWrapper for \"CBasePlayer::PlayerSolidMask\"!");

		return;
	}

	// Create global hooks for blocker entities
	static const struct {
		const char* className;
		int& hookId;
	} s_entities[] = {
		{ "env_player_blocker", shookid_CEnv_Blocker_ShouldCollide },
		{ "env_physics_blocker", shookid_CEnvPhysicsBlocker_ShouldCollide },
	};

	for (auto&& el : s_entities) {
		CBaseEntity* pEntity = static_cast<CBaseEntity*>(servertools->CreateEntityByName(el.className));
		if (pEntity == NULL) {
			smutils->LogError(myself, "Unable to create an entity \"%s\" to hook \"ShouldCollide\" on!", el.className);

			return;
		}

		el.hookId = SH_ADD_MANUALVPHOOK(CBaseEntity_ShouldCollide, pEntity, SH_MEMBER(this, &CTeamCollision::Handler_CEnvBlocker_ShouldCollide), false);
		
		edict_t* pEdict = gameents->BaseEntityToEdict(pEntity);
		if (pEdict != NULL) {
			engine->RemoveEdict(pEdict);
		}
	}

	shookid_CGameMovement_PlayerSolidMask = SH_ADD_MANUALHOOK(CGameMovement_PlayerSolidMask, g_pGameMovement, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorGameMovement_PlayerSolidMask), true);

	playerhelpers->AddClientListener(this);

	if (playerhelpers->IsServerActivated()) {
		OnServerActivated(playerhelpers->GetMaxClients());
	}
}

bool CTeamCollision::QueryInterfaceDrop(SMInterface* pInterface)
{
	return pInterface != bintools;
}

void CTeamCollision::NotifyInterfaceDrop(SMInterface* pInterface)
{
	SDK_OnUnload();
}

bool CTeamCollision::QueryRunning(char* error, size_t maxlength)
{
	SM_CHECK_IFACE(BINTOOLS, bintools);

	return true;
}

bool CTeamCollision::SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetServerFactory, g_pGameMovement, CGameMovement, INTERFACENAME_GAMEMOVEMENT);
	GET_V_IFACE_CURRENT(GetServerFactory, servertools, IServerTools, VSERVERTOOLS_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

	return true;
}
