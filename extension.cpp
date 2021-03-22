#include "extension.h"
#include "wrappers.h"

#include <CDetour/detours.h>

CTeamCollision g_TeamCollision;
SMEXT_LINK(&g_TeamCollision);

SH_DECL_MANUALHOOK1(CBasePlayer_PlayerSolidMask, 0, 0, 0, unsigned int, bool);
SH_DECL_MANUALHOOK2(CGameMovement_PlayerSolidMask, 0, 0, 0, unsigned int, bool, CBasePlayer*);

IBinTools* bintools = NULL;
CGameMovement* g_pGameMovement = NULL;
IServerGameEnts* gameents = NULL;

int CBasePlayer::sendprop_m_fFlags = 0;
int CTerrorPlayer::sendprop_m_carryVictim = 0;
int CTerrorPlayer::sendprop_m_jockeyAttacker = 0;
int CBasePlayer::vtblindex_PlayerSolidMask = 0;
int CGameMovement::vtblindex_PlayerSolidMask = 0;
ICallWrapper* CBasePlayer::vcall_PlayerSolidMask = NULL;
void* CEnv_Blocker::pfn_ShouldCollide = 0;
void* CEnvPhysicsBlocker::pfn_ShouldCollide = 0;
CDetour* CEnv_Blocker::detour_ShouldCollide = 0;
CDetour* CEnvPhysicsBlocker::detour_ShouldCollide = NULL;
int CGameMovement::shookid_PlayerSolidMask = 0;

FORCEINLINE void Handle_ShouldCollide(int& contentsMask, int collisionGroup)
{
	if (collisionGroup != COLLISION_GROUP_PLAYER_MOVEMENT) {
		return;
	}

	if ((contentsMask & CONTENTS_PLAYERCLIP) != 0) {
		contentsMask |= CONTENTS_TEAM1;
	}

	if ((contentsMask & CONTENTS_MONSTERCLIP) != 0) {
		contentsMask |= CONTENTS_TEAM2;
	}
}

DETOUR_DECL_MEMBER2(Handler_CEnv_Blocker_ShouldCollide, bool, int, collisionGroup, int, contentsMask)
{
	Handle_ShouldCollide(contentsMask, collisionGroup);

	return DETOUR_MEMBER_CALL(Handler_CEnv_Blocker_ShouldCollide)(collisionGroup, contentsMask);
}

DETOUR_DECL_MEMBER2(Handler_CEnvPhysicsBlocker_ShouldCollide, void, int, collisionGroup, int, contentsMask)
{
	Handle_ShouldCollide(contentsMask, collisionGroup);

	return DETOUR_MEMBER_CALL(Handler_CEnvPhysicsBlocker_ShouldCollide)(collisionGroup, contentsMask);
}

unsigned int CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask(bool brushOnly)
{
	CTerrorPlayer* _this = META_IFACEPTR(CTerrorPlayer);

	CTerrorPlayer* pVictim = _this->GetCarryVictim();
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

bool CTeamCollision::SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength)
{
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

			return false;
		}
	}

	static const struct {
		const char* key;
		void*& address;
	} s_sigs[] = {
		{ "CEnv_Blocker::ShouldCollide", CEnv_Blocker::pfn_ShouldCollide },
		{ "CEnvPhysicsBlocker::ShouldCollide", CEnvPhysicsBlocker::pfn_ShouldCollide },
	};

	for (auto&& el : s_sigs) {
		if (!gc->GetMemSig(el.key, &el.address)) {
			ke::SafeSprintf(error, maxlength, "Unable to find signature for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}

		if (el.address == NULL) {
			ke::SafeSprintf(error, maxlength, "Sigscan for \"%s\" failed (game config file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}
	}

	return true;
}

void CTeamCollision::OnClientPutInServer(int client)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(client);
	if (pPlayer == NULL) {
		return;
	}

	SH_ADD_MANUALHOOK(CBasePlayer_PlayerSolidMask, pPlayer, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask), true);
}

void CTeamCollision::OnClientDisconnecting(int client)
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(client);
	if (pPlayer == NULL) {
		return;
	}

	SH_REMOVE_MANUALHOOK(CBasePlayer_PlayerSolidMask, pPlayer, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorPlayer_PlayerSolidMask), true);
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

	if (!gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_carryVictim", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CTerrorPlayer::m_carryVictim\"");

		return false;
	}

	CTerrorPlayer::sendprop_m_carryVictim = info.actual_offset;

	if (!gamehelpers->FindSendPropInfo("CTerrorPlayer", "m_jockeyAttacker", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CTerrorPlayer::m_jockeyAttacker\"");

		return false;
	}

	CTerrorPlayer::sendprop_m_jockeyAttacker = info.actual_offset;

	IGameConfig* gc = NULL;
	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &gc, error, maxlength) || gc == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to load a gamedata file \"" GAMEDATA_FILE ".txt\"");

		return false;
	}

	if (!SetupFromGameConfig(gc, error, maxlength)) {
		gameconfs->CloseGameConfigFile(gc);

		return false;
	}

	gameconfs->CloseGameConfigFile(gc);

	SH_MANUALHOOK_RECONFIGURE(CBasePlayer_PlayerSolidMask, CBasePlayer::vtblindex_PlayerSolidMask, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(CGameMovement_PlayerSolidMask, CGameMovement::vtblindex_PlayerSolidMask, 0, 0);

	// Game config is never used by detour class to handle errors ourselves
	CDetourManager::Init(smutils->GetScriptingEngine(), NULL);

	CEnv_Blocker::detour_ShouldCollide = DETOUR_CREATE_MEMBER(Handler_CEnv_Blocker_ShouldCollide, CEnv_Blocker::pfn_ShouldCollide);
	if (CEnv_Blocker::detour_ShouldCollide == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CEnv_Blocker::ShouldCollide\"");

		return false;
	}

	CEnvPhysicsBlocker::detour_ShouldCollide = DETOUR_CREATE_MEMBER(Handler_CEnvPhysicsBlocker_ShouldCollide, CEnvPhysicsBlocker::pfn_ShouldCollide);
	if (CEnvPhysicsBlocker::detour_ShouldCollide == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CEnvPhysicsBlocker::ShouldCollide\"");

		return false;
	}

	sharesys->AddDependency(myself, "bintools.ext", true, true);

	return true;
}

void CTeamCollision::SDK_OnUnload()
{
	if (CBasePlayer::vcall_PlayerSolidMask != NULL) {
		CBasePlayer::vcall_PlayerSolidMask->Destroy();
		CBasePlayer::vcall_PlayerSolidMask = NULL;
	}

	if (CEnv_Blocker::detour_ShouldCollide != NULL) {
		CEnv_Blocker::detour_ShouldCollide->Destroy();
		CEnv_Blocker::detour_ShouldCollide = NULL;
	}

	if (CEnvPhysicsBlocker::detour_ShouldCollide != NULL) {
		CEnvPhysicsBlocker::detour_ShouldCollide->Destroy();
		CEnvPhysicsBlocker::detour_ShouldCollide = NULL;
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

	SH_REMOVE_HOOK_ID(CGameMovement::shookid_PlayerSolidMask);
	CGameMovement::shookid_PlayerSolidMask = 0;
}

void CTeamCollision::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(BINTOOLS, bintools);
	if (bintools == NULL) {
		return;
	}

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

	CGameMovement::shookid_PlayerSolidMask = SH_ADD_MANUALHOOK(CGameMovement_PlayerSolidMask, g_pGameMovement, SH_MEMBER(this, &CTeamCollision::Handler_CTerrorGameMovement_PlayerSolidMask), true);

	CEnv_Blocker::detour_ShouldCollide->EnableDetour();
	CEnvPhysicsBlocker::detour_ShouldCollide->EnableDetour();

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
	GET_V_IFACE_CURRENT(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

	return true;
}
