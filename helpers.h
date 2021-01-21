#ifndef _INCLUDE_HELPERS_H_
#define _INCLUDE_HELPERS_H_

#include "smsdk_ext.h"
#include <extensions/IBinTools.h>

class CBaseEntity
{
public:
	static int sendprop_m_iTeamNum;
	static int vtblindex_ShouldCollide;

	int GetTeamNumber()
	{
		return *(int*)((byte*)(this) + CBaseEntity::sendprop_m_iTeamNum);
	}
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

		IServerUnknown* pUnknown = pEdict->GetUnknown();
		if (pUnknown == NULL) {
			return NULL;
		}

		return (CBasePlayer*)pUnknown->GetBaseEntity();
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

		IServerUnknown* pUnknown = pEdict->GetUnknown();
		if (pUnknown == NULL) {
			return NULL;
		}

		return (CBasePlayer*)pUnknown->GetBaseEntity();
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
