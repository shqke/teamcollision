#ifndef _INCLUDE_HELPERS_H_
#define _INCLUDE_HELPERS_H_

#include "smsdk_ext.h"
#include <extensions/IBinTools.h>

// iclientrenderable.h (included in toolframework/itoolentity.h) has explicit NULL-dereference in CDefaultClientRenderable::GetRefEHandle
// Don't need to add compiler option -Wnull-dereference if can just ignore this file alone
#define ICLIENTRENDERABLE_H
class IClientRenderable;
typedef unsigned short ClientShadowHandle_t;
#include <toolframework/itoolentity.h>

#define TEAM_SURVIVOR           2
#define TEAM_INFECTED           3
#define TEAM_L4D1SURVIVOR       4

extern IServerGameEnts* gameents;

class CBaseEntity
{
public:
	static int vtblindex_ShouldCollide;
};

class CBasePlayer :
	public CBaseEntity
{
public:
	static int sendprop_m_fFlags;

	bool IsBot()
	{
		return (*(int*)((byte*)(this) + CBasePlayer::sendprop_m_fFlags) & FL_FAKECLIENT) != 0;
	}

	static int sendprop_m_isGhost;

	bool IsGhost()
	{
		return *(bool*)((byte*)(this) + CBasePlayer::sendprop_m_isGhost);
	}

	static int sendprop_m_carryVictim;

	CBasePlayer* GetCarryVictim()
	{
		edict_t* pEdict = gamehelpers->GetHandleEntity(*(CBaseHandle*)((byte*)(this) + CBasePlayer::sendprop_m_carryVictim));
		if (pEdict == NULL) {
			return NULL;
		}

		// Make sure it's a player
		if (engine->GetPlayerUserId(pEdict) == -1) {
			return NULL;
		}

		return (CBasePlayer*)gameents->EdictToBaseEntity(pEdict);
	}

	static int sendprop_m_jockeyAttacker;

	CBasePlayer* GetJockeyAttacker()
	{
		edict_t* pEdict = gamehelpers->GetHandleEntity(*(CBaseHandle*)((byte*)(this) + CBasePlayer::sendprop_m_jockeyAttacker));
		if (pEdict == NULL) {
			return NULL;
		}

		// Make sure it's a player
		if (engine->GetPlayerUserId(pEdict) == -1) {
			return NULL;
		}

		return (CBasePlayer*)gameents->EdictToBaseEntity(pEdict);
	}

	static int vtblindex_PlayerSolidMask;
	static ICallWrapper* vcall_PlayerSolidMask;

	unsigned int PlayerSolidMask(bool brushOnly)
	{
		struct {
			CBasePlayer* pPlayer;
			int brushOnly;
		} stack{ this, brushOnly };

		unsigned int ret;
		vcall_PlayerSolidMask->Execute(&stack, &ret);

		return ret;
	}
};

class CTerrorPlayer :
	public CBasePlayer
{
public:
};

class CGameMovement
{
public:
	static int vtblindex_PlayerSolidMask;

	virtual unsigned int PlayerSolidMask(bool brushOnly = false, CBasePlayer* testPlayer = NULL) const;	///< returns the solid mask for the given player, so bots can have a more-restrictive set
	CBasePlayer* player;
};

#define INTERFACENAME_GAMEMOVEMENT	"GameMovement001"

#endif // _INCLUDE_HELPERS_H_
