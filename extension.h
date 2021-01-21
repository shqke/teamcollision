#ifndef _INCLUDE_EXTENSION_PROPER_H_
#define _INCLUDE_EXTENSION_PROPER_H_

#include "smsdk_ext.h"
#include "helpers.h"

#include <sourcehook.h>
#include <extensions/IBinTools.h>
#include <amtl/am-string.h>

// iclientrenderable.h (included in toolframework/itoolentity.h) has explicit NULL-dereference in CDefaultClientRenderable::GetRefEHandle
// Don't need to add compiler option -Wnull-dereference if can just ignore this file alone
#define ICLIENTRENDERABLE_H
class IClientRenderable;
typedef unsigned short ClientShadowHandle_t;
#include <toolframework/itoolentity.h>

#define TEAM_SURVIVOR           2
#define TEAM_INFECTED           3
#define TEAM_L4D1SURVIVOR       4

class CTeamCollision :
	public SDKExtension,
	public IClientListener
{
public:
	CTeamCollision();

public: // SourceHook callbacks
	unsigned int Handler_CTerrorPlayer_PlayerSolidMask(bool brushOnly);
	unsigned int Handler_CTerrorGameMovement_PlayerSolidMask(bool brushOnly, CBasePlayer* testPlayer);
	bool Handler_CEnvBlocker_ShouldCollide(int collisionGroup, int contentsMask);

protected:
	bool SetupFromGameConfigs(char* error, int maxlength);

protected:
	int shookid_CGameMovement_PlayerSolidMask;
	int shookid_CEnv_Blocker_ShouldCollide;
	int shookid_CEnvPhysicsBlocker_ShouldCollide;

public:
	/**
	 * @brief Called when a client is put in server.
	 *
	 * @param client		Index of the client.
	 */
	void OnClientPutInServer(int client) override;

	/**
	 * @brief Called when a client is disconnecting (not fully disconnected yet).
	 *
	 * @param client		Index of the client.
	 */
	void OnClientDisconnecting(int client) override;

	/**
	 * @brief Called when the server is activated.
	 */
	void OnServerActivated(int max_clients) override;

public:
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlength	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	bool SDK_OnLoad(char* error, size_t maxlength, bool late) override;

	/**
	 * @brief This is called once the extension unloading process begins.
	 */
	void SDK_OnUnload() override;

	/**
	 * @brief This is called once all known extensions have been loaded.
	 */
	void SDK_OnAllLoaded() override;

	/**
	 * @brief Asks the extension whether it's safe to remove an external
	 * interface it's using.  If it's not safe, return false, and the
	 * extension will be unloaded afterwards.
	 *
	 * NOTE: It is important to also hook NotifyInterfaceDrop() in order to clean
	 * up resources.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 *							pointer of known type.
	 * @return					True to continue, false to unload this
	 * 							extension afterwards.
	 */
	bool QueryInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Notifies the extension that an external interface it uses is being removed.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 */
	void NotifyInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Return false to tell Core that your extension should be considered unusable.
	 *
	 * @param error				Error buffer.
	 * @param maxlength			Size of error buffer.
	 * @return					True on success, false otherwise.
	 */
	bool QueryRunning(char* error, size_t maxlength) override;

public:
	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlength, bool late) override;
};

#endif // _INCLUDE_EXTENSION_PROPER_H_
