/******************************************************************************
 *  Rendering.cpp: scripts affecting weather, precipitation, and textures
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

#include "Rendering.h"
#include <ScriptLib.h>



/* KDTransitionTrap */

const int
cScr_TransitionTrap::INCREMENT_TIME = 50;

cScr_TransitionTrap::cScr_TransitionTrap (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseTrap (pszName, iHostObjId),
	  // associate to final script's name to avoid collisions
	  timer (pszName, "transition_timer", iHostObjId),
	  time_remaining (pszName, "transition_remaining", iHostObjId)
{}

long
cScr_TransitionTrap::OnSwitch (bool bState, sScrMsg*, cMultiParm&)
{
	if (OnPrepare (bState))
	{
		Begin ();
		return S_OK;
	}
	else
		return S_FALSE;
}

long
cScr_TransitionTrap::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "Increment") &&
	    pMsg->data.type == kMT_String && !strcmp (pMsg->data, Name ()))
	{
		Increment ();
		return S_OK;
	}
	return cBaseTrap::OnTimer (pMsg, mpReply);
}

bool
cScr_TransitionTrap::OnPrepare (bool)
{
	// trap behavior requires an override of this method
	return false;
}

void
cScr_TransitionTrap::Begin ()
{
	if (timer.Valid ()) // stop any previous transition
	{
		KillTimedMessage (timer);
		timer.Clear ();
	}

	time_remaining = GetObjectParamTime (ObjId (), "transition", 0);
	Increment ();
}

bool
cScr_TransitionTrap::OnIncrement ()
{
	DebugString ("Error: OnIncrement unimplemented in script inheriting "
		"from TransitionTrap.");
	return false;
}

float
cScr_TransitionTrap::GetProgress ()
{
	float total = GetObjectParamTime (ObjId (), "transition", 0);
	if (!time_remaining.Valid ())
		return 0.0;
	else if (total == 0.0 || time_remaining == 0)
		return 1.0;
	else
		return (total - float (time_remaining)) / total;
}

float
cScr_TransitionTrap::Interpolate (float start, float end)
{
	return start + GetProgress () * (end - start);
}

ulong
cScr_TransitionTrap::InterpolateColor (ulong start, ulong end)
{
	return AverageColors (start, end, GetProgress ());
}

void
cScr_TransitionTrap::Increment ()
{
	if (OnIncrement () && time_remaining > 0)
	{
		time_remaining =
			std::max (0, time_remaining - INCREMENT_TIME);
		timer = SetTimedMessage ("Increment", INCREMENT_TIME,
			kSTM_OneShot, Name ());
	}
	else
	{
		timer.Clear ();
		time_remaining.Clear ();
	}
}



/* KDTrapEnvMap */

cScr_TrapEnvMap::cScr_TrapEnvMap (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseTrap (pszName, iHostObjId)
{}

long
cScr_TrapEnvMap::OnSwitch (bool bState, sScrMsg*, cMultiParm&)
{
	int zone = GetObjectParamInt (ObjId (), "env_map_zone", GLOBAL_ZONE);
	char* texture = GetObjectParamString (ObjId (),
		bState ? "env_map_on" : "env_map_off", NULL);

	if (zone < MIN_ZONE || zone > MAX_ZONE || !texture) return S_FALSE;

	if (!CheckEngineVersion (1, 20))
	{
		DebugPrintf ("This script cannot be used with this version of "
			"the Dark Engine. Upgrade to version 1.20 or higher.");
		return S_FALSE;
	}

	SService<IEngineSrv> pES (g_pScriptManager);
	pES->SetEnvMapZone (zone, texture);
	g_pMalloc->Free (texture);

	return S_OK;
}



/* DarkFog */

ulong
DarkFog::GetColor (int zone)
{
	int r = 0, g = 0, b = 0; float d = 0.0;
	SService<IEngineSrv> pES (g_pScriptManager);
	if (zone == GLOBAL_ZONE)
		pES->GetFog (r, g, b, d);
	else if (zone >= MIN_ZONE && zone <= MAX_ZONE)
		pES->GetFogZone (zone, r, g, b, d);
	return makecolor (r, g, b);
}

float
DarkFog::GetDistance (int zone)
{
	int r = 0, g = 0, b = 0; float d = 0.0;
	SService<IEngineSrv> pES (g_pScriptManager);
	if (zone == GLOBAL_ZONE)
		pES->GetFog (r, g, b, d);
	else if (zone >= MIN_ZONE && zone <= MAX_ZONE)
		pES->GetFogZone (zone, r, g, b, d);
	return d;
}

void
DarkFog::Set (int zone, ulong color, float distance)
{
	int r = getred (color), b = getblue (color), g = getgreen (color);
	SService<IEngineSrv> pES (g_pScriptManager);
	if (zone == GLOBAL_ZONE)
		pES->SetFog (r, g, b, distance);
	else if (zone >= MIN_ZONE && zone <= MAX_ZONE)
		pES->SetFogZone (zone, r, g, b, distance);
}

float
DarkFog::FakeDistance (float distance, bool final)
{
	return (distance == 0.0 && !final) ? 10000.0 : distance;
}



/* KDTrapFog */

cScr_TrapFog::cScr_TrapFog (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseTrap (pszName, iHostObjId),
	  cScr_TransitionTrap (pszName, iHostObjId),
	  SCRIPT_VAROBJ (TrapFog, zone, iHostObjId),
	  SCRIPT_VAROBJ (TrapFog, start_color, iHostObjId),
	  SCRIPT_VAROBJ (TrapFog, end_color, iHostObjId),
	  SCRIPT_VAROBJ (TrapFog, start_dist, iHostObjId),
	  SCRIPT_VAROBJ (TrapFog, end_dist, iHostObjId)
{}

bool
cScr_TrapFog::OnPrepare (bool state)
{
	int _zone = GetObjectParamInt (ObjId (), "fog_zone", GLOBAL_ZONE);
	ulong _end_color = GetObjectParamColor (ObjId (),
		state ? "fog_color_on" : "fog_color_off", 0);
	float _end_dist = GetObjectParamFloat (ObjId (),
		state ? "fog_dist_on" : "fog_dist_off", -1.0);

	if (_zone < GLOBAL_ZONE || _zone > MAX_ZONE ||
	    (_end_color == 0 && _end_dist < 0.0))
		return false;

	zone = _zone;
	start_color = GetColor (zone);
	start_dist = GetDistance (zone);
	end_color = (_end_color > 0) ? _end_color : start_color;
	end_dist = (_end_dist >= 0.0) ? _end_dist : start_dist;

	// notify SyncGlobalFog if present
	object player = StrToObject ("Player");
	if (player)
		SimpleSend (ObjId (), player, "FogZoneChanging", (int) zone,
			(int) end_color, (float) end_dist);

	return true;
}

bool
cScr_TrapFog::OnIncrement ()
{
	bool final = (GetProgress () >= 1.0);
	ulong color = InterpolateColor (start_color, end_color);
	float distance = Interpolate (FakeDistance (start_dist, final),
		FakeDistance (end_dist, final));
	Set (zone, color, distance);
	return true;
}



/* KDSyncGlobalFog */

cScr_SyncGlobalFog::cScr_SyncGlobalFog (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseTrap (pszName, iHostObjId),
	  cScr_TransitionTrap (pszName, iHostObjId),
	  SCRIPT_VAROBJ (SyncGlobalFog, last_room_zone, iHostObjId),
	  SCRIPT_VAROBJ (SyncGlobalFog, start_color, iHostObjId),
	  SCRIPT_VAROBJ (SyncGlobalFog, end_color, iHostObjId),
	  SCRIPT_VAROBJ (SyncGlobalFog, start_dist, iHostObjId),
	  SCRIPT_VAROBJ (SyncGlobalFog, end_dist, iHostObjId)
{}

long
cScr_SyncGlobalFog::OnObjRoomTransit (sRoomMsg* pMsg, cMultiParm&)
{
	if (pMsg->ObjType != sRoomMsg::kPlayer)
		return S_FALSE; // we're not the player yet

	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm _new_zone;
	pPS->Get (_new_zone, pMsg->ToObjId, "Weather", "fog");
	if (_new_zone.type != kMT_Int) return S_FALSE;
	// disabled = -1, global = 1, zone_1 =2, etc.
	int new_zone = int (_new_zone) - 1;

	if (last_room_zone.Valid () && last_room_zone == new_zone)
		return S_FALSE; // no change

	ulong color; float distance; bool sync_color;
	if (new_zone == FOG_DISABLED) // disabled
	{
		color = 0;
		distance = 0.0;
		sync_color = false;
	}
	else if (new_zone >= MIN_ZONE && new_zone <= MAX_ZONE)
	{
		color = GetColor (new_zone);
		distance = GetDistance (new_zone);
		sync_color = true;
	}
	else
		return S_FALSE;

	last_room_zone = new_zone;
	Sync (color, distance, sync_color);
	return S_OK;
}

long
cScr_SyncGlobalFog::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "FogZoneChanging") &&
	    last_room_zone.Valid () &&
	    pMsg->data.type == kMT_Int && last_room_zone == int (pMsg->data) &&
	    pMsg->data2.type == kMT_Int && pMsg->data3.type == kMT_Float)
	{
		Sync ((int) pMsg->data2, (float) pMsg->data3);
		return S_OK;
	}
	return cBaseScript::OnMessage (pMsg, mpReply);
}

void
cScr_SyncGlobalFog::Sync (ulong color, float dist, bool sync_color)
{
	sync_color &= GetObjectParamBool (ObjId (), "sync_fog_color", true);
	bool sync_dist = (dist >= 0.0) &&
		GetObjectParamBool (ObjId (), "sync_fog_dist", true);

	start_color = GetColor (GLOBAL_ZONE);
	start_dist = GetDistance (GLOBAL_ZONE);
	end_color = sync_color ? color : start_color;

	float mult = GetObjectParamFloat (ObjId (), "fog_dist_mult", 1.0),
		add = GetObjectParamFloat (ObjId (), "fog_dist_add", 0.0);
	end_dist = (sync_dist || start_dist == 0.0 || dist == 0.0)
		? (dist == 0.0 ? dist : dist * mult + add) : start_dist;

	Begin ();
}

bool
cScr_SyncGlobalFog::OnIncrement ()
{
	bool final = (GetProgress () >= 1.0);
	ulong color = InterpolateColor (start_color, end_color);
	float dist = Interpolate (FakeDistance (start_dist, final),
		FakeDistance (end_dist, final));
	Set (GLOBAL_ZONE, color, dist);
	return true;
}



/* DarkWeather */

DarkWeather::DarkWeather ()
	: precip_type (PRECIP_SNOW), precip_freq (0.0), precip_speed (0.0),
	  vis_dist (0.0), rend_radius (0.0), alpha (0.0), brightness (0.0),
	  snow_jitter (0.0), rain_length (0.0), splash_freq (0.0),
	  splash_radius (0.0), splash_height (0.0), splash_duration (0.0),
	  texture (), wind (0.0, 0.0, 0.0)
{
	GetFromMission ();
}

void
DarkWeather::GetFromMission ()
{
	SService<IEngineSrv> pES (g_pScriptManager);
	int _precip_type;
	const char* _texture = NULL;
	pES->GetWeather (_precip_type, precip_freq, precip_speed, vis_dist,
		rend_radius, alpha, brightness, snow_jitter, rain_length,
		splash_freq, splash_radius, splash_height, splash_duration,
		_texture, wind);
	precip_type = (PrecipType) _precip_type;
	texture = _texture;
}

void
DarkWeather::SetInMission () const
{
	SService<IEngineSrv> pES (g_pScriptManager);
	pES->SetWeather (precip_type, precip_freq, precip_speed, vis_dist,
		rend_radius, alpha, brightness, snow_jitter, rain_length,
		splash_freq, splash_radius, splash_height, splash_duration,
		texture, wind);
}



/* KDTrapWeather */

cScr_TrapWeather::cScr_TrapWeather (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseTrap (pszName, iHostObjId),
	  cScr_TransitionTrap (pszName, iHostObjId),
	  SCRIPT_VAROBJ (TrapWeather, start_freq, iHostObjId),
	  SCRIPT_VAROBJ (TrapWeather, end_freq, iHostObjId),
	  SCRIPT_VAROBJ (TrapWeather, start_speed, iHostObjId),
	  SCRIPT_VAROBJ (TrapWeather, end_speed, iHostObjId)
{}

bool
cScr_TrapWeather::OnPrepare (bool state)
{
	float _freq = GetObjectParamFloat (ObjId (),
		state ? "precip_freq_on" : "precip_freq_off", -1.0);
	float _speed = GetObjectParamFloat (ObjId (),
		state ? "precip_speed_on" : "precip_speed_off", -1.0);

	if (_freq < 0.0 && _speed < 0.0) return false;

	DarkWeather current;
	start_freq = current.precip_freq;
	start_speed = current.precip_speed;

	if (_freq >= 0.0)
		end_freq = _freq;
	else
		end_freq = start_freq;

	if (_speed >= 0.0)
		end_speed = _speed;
	else
		end_speed = start_speed;

	return true;
}

bool
cScr_TrapWeather::OnIncrement ()
{
	DarkWeather weather;
	weather.precip_freq = Interpolate (start_freq, end_freq);
	weather.precip_speed = Interpolate (start_speed, end_speed);
	weather.SetInMission ();
	return true;
}

