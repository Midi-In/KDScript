/******************************************************************************
 *  NewDark.h: scripts exposing NewDark script-only features
 *
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *  Adapted in part from Public Scripts
 *  Copyright (C) 2005-2011 Tom N Harris <telliamed@whoopdedo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef NEWDARK_H
#define NEWDARK_H

#if !SCR_GENSCRIPTS
#include "BaseScript.h"
#include "BaseTrap.h"
#include "scriptvars.h"
#endif // SCR_GENSCRIPTS



/**
 * AbstractScript: KDTransitionTrap
 * Inherits: BaseTrap
 * Parameter: transition (time) - Length of the transition, in milliseconds.
 *
 * Abstract base script for traps that create smooth transitions between two
 * states. The length of the transition in milliseconds is specified in the
 * "transition" parameter. If that parameter is undefined or set to zero, the
 * transition will be instantaneous. The usual trap control flags, locking, and
 * timing (before the start of the transition, not its length) are respected.
 *
 * A concrete script inheriting from TransitionTrap should implement OnPrepare
 * and OnIncrement, making preliminary calculations for the transition in the
 * former and actually performing the change of state in the latter. OnPrepare
 * has an argument "state" which will indicate the trap's new on/off state.
 * The GetProgress method returns a float value ranging linearly from 0.0 at the
 * start of the transition to 1.0 at its end. The GetIncrementDelta method can
 * be overridden to change the time between increments in milliseconds.
 */
#if !SCR_GENSCRIPTS
class cScr_TransitionTrap : public virtual cBaseTrap
{
public:
	cScr_TransitionTrap (const char* pszName, int iHostObjId);

protected:
	virtual long OnSwitch (bool bState, sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);

	virtual bool OnPrepare (bool state) = 0;
	virtual bool OnIncrement () = 0;

	float GetProgress ();
	float Interpolate (float start, float end)
		{ return start + GetProgress () * (end - start); }

	virtual int GetIncrementDelta () const { return 50; }

private:
	void Increment ();

	script_handle<tScrTimer> timer;
	script_int time_total, time_remaining;
};
#endif // !SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
class cScr_GetInfo : public virtual cBaseScript
{
public:
	cScr_GetInfo (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnDarkGameModeChange (sDarkGameModeScrMsg* pMsg,
		cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);

private:
	void UpdateVariables ();
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("KDGetInfo","BaseScript",cScr_GetInfo)
#endif // SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
class cScr_TrapEnvMapTexture : public virtual cBaseTrap
{
public:
	cScr_TrapEnvMapTexture (const char* pszName, int iHostObjId);

protected:
	virtual long OnSwitch (bool bState, sScrMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("KDTrapEnvMapTexture","BaseTrap",cScr_TrapEnvMapTexture)
#endif // SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
class cScr_TrapFog : public cScr_TransitionTrap
{
public:
	cScr_TrapFog (const char* pszName, int iHostObjId);

protected:
	virtual bool OnPrepare (bool state);
	virtual bool OnIncrement ();

private:
	script_int zone,
		start_red, start_green, start_blue,
		end_red, end_green, end_blue;
	script_float start_dist, end_dist;
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("KDTrapFog","KDTransitionTrap",cScr_TrapFog)
#endif // SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
class cScr_TrapNextMission : public virtual cBaseTrap
{
public:
	cScr_TrapNextMission (const char* pszName, int iHostObjId);

protected:
	virtual long OnSwitch (bool bState, sScrMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("KDTrapNextMission","BaseTrap",cScr_TrapNextMission)
#endif // SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
class cScr_TrapWeather : public cScr_TransitionTrap
{
public:
	cScr_TrapWeather (const char* pszName, int iHostObjId);

protected:
	virtual bool OnPrepare (bool state);
	virtual bool OnIncrement ();

private:
	script_float start_freq, end_freq,
		start_speed, end_speed;
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("KDTrapWeather","KDTransitionTrap",cScr_TrapWeather)
#endif // SCR_GENSCRIPTS



#endif // NEWDARK_H

