/******************************************************************************
 *  utils.cpp
 *
 *  Copyright (C) 2012-2013 Kevin Daughtridge <kevin@kdau.com>
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
#include "utils.h"
#include "ScriptModule.h"
#include "ScriptLib.h"

#include <lg/types.h>
#include <lg/scrservices.h>

#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <stdexcept>

static const char* g_pszAbbrevMsgArray[][2] = {
	{"FIB",      "FrobInvBegin"},
	{"FIE",      "FrobInvEnd"},
	{"FTB",      "FrobToolBegin"},
	{"FTE",      "FrobToolEnd"},
	{"FWB",      "FrobWorldBegin"},
	{"FWE",      "FrobWorldEnd"},
	{"IDF",      "InvDeFocus"},
	{"IDS",      "InvDeSelect"},
	{"IF",       "InvFocus"},
	{"IS",       "InvSelect"},
	{"WDF",      "WorldDeFocus"},
	{"WDS",      "WorldDeSelect"},
	{"WF",       "WorldFocus"},
	{"WS",       "WorldSelect"},
	{"CRI",      "CreatureRoomEnter"},
	{"CRO",      "CreatureRoomExit"},
	{"ORI",      "ObjectRoomEnter"},
	{"ORO",      "ObjectRoomExit"},
	{"ORT",      "ObjectRoomTransit"},
	{"PRI",      "PlayerRoomEnter"},
	{"PRO",      "PlayerRoomExit"},
	{"RPRI",     "RemotePlayerRoomEnter"},
	{"RPRO",     "RemotePlayerRoomExit"},
	{"PC",       "PhysCollision"},
	{"PCC",      "PhysContactCreate"},
	{"PCD",      "PhysContactDestroy"},
	{"PI",       "PhysEnter"},
	{"PO",       "PhysExit"},
	{"PFA",      "PhysFellAsleep"},
	{"PMN",      "PhysMadeNonPhysical"},
	{"PMP",      "PhysMadePhysical"},
	{"PWU",      "PhysWokeUp"},
	{"PPA",      "PressurePlateActivating"},
	{"PPD",      "PressurePlateDeactivating"},
	{"PPU",      "PressurePlateInactive"},
	{"PPD",      "PressurePlateActive"},
	{"ME",       "MotionEnd"},
	{"MF",       "MotionFlagReached"},
	{"MS",       "MotionStart"},
	{"MTWP",     "MovingTerrainWaypoint"},
	{"WPR",      "WaypointReached"},
	{"DGMC",     "DarkGameModeChange"},
	{"MT"    ,   "MediumTransition"},
	{"PSC",      "PickStateChange"},
	{NULL, NULL}
};

char* FixupScriptParamsHack(const char* pszData)
{
	char* pszReal = NULL;
	if (pszData[0] == '#')
	{
		object iHackObj = StrToObject(pszData+1);
		if (iHackObj)
			pszReal = GetObjectParams(iHackObj);
	}
	else if (pszData[0] == '.')
	{
		++pszData;
		for (int n = 0; g_pszAbbrevMsgArray[n][0]; ++n)
		{
			if (!strcmp(pszData, g_pszAbbrevMsgArray[n][0]))
			{
				pszReal = reinterpret_cast<char*>(g_pMalloc->Alloc(::strlen(g_pszAbbrevMsgArray[n][1])+1));
				if (pszReal)
					::strcpy(pszReal, g_pszAbbrevMsgArray[n][1]);
				break;
			}
		}
	}
	else if (pszData[0] == '!')
	{
		pszReal = reinterpret_cast<char*>(g_pMalloc->Alloc(::strlen(pszData)+9));
		if (pszReal)
		{
			::strcpy(pszReal, pszData+1);
			::strcat(pszReal, "Stimulus");
		}
	}
	else
	{
		pszReal = reinterpret_cast<char*>(g_pMalloc->Alloc(::strlen(pszData)+1));
		if (pszReal)
			::strcpy(pszReal, pszData);
	}
	return pszReal;
}

void StringToMultiParm(cMultiParm &mp, const char* psz)
{
	switch (psz[0] | 0x20)
	{
	  case 'i':
		mp = ::strtol(psz+1, NULL, 0);
		break;
	  case 'f':
		mp = ::strtod(psz+1, NULL);
		break;
	  case 's':
		mp = psz+1;
		break;
	  case 'v':
	  {
		mxs_vector v;
		v.x = v.y = v.z = 0;
		::sscanf(psz+1, "%f , %f , %f", &v.x, &v.y, &v.z);
		mp = v;
		break;
	  }
	default:
	  {
		char* end = NULL;
		mp = ::strtol(psz, &end, 0);
		if (end && *end != '\0')
			mp = psz;
	  }
	}
}

// 1/e
#define M_1_E   0.3678794411714423216
// 1 - 1/e
#define M_M1_E  0.6321205588285576784
// 1/(e - 1)
#define M_1_ME  0.5819767068693264244

double CalculateCurve(int c, double f, double a, double b)
{
	switch (c)
	{
	// quadratic
	case 1:
		f = f*f;
		break;
	// sqrt
	case 2:
		f = ::sqrt(f);
		break;
	// log
	case 3:
		f = 1.0 + ::log10(f*0.9 + 0.1);
		break;
	// 10^n
	case 4:
		f = (1.0/9.0) * (::pow(10.0,f) - 1);
		break;
	// ln
	case 5:
		f = 1.0 + ::log(f*M_M1_E + M_1_E);
		break;
	// e^n
	case 6:
		f = M_1_ME * (::exp(f) - 1);
		break;
	default:
		break;
	}
	return a + (f * (b - a));
}

double CalculateCurve(double f, double a, double b, object iObj)
{
	return CalculateCurve(GetObjectParamInt(iObj, "curve"), f, a, b);
}

bool PlaySound(object host, const cScrStr& name, const cScrVec& location, eSoundSpecial flags, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlayAtLocation(ret, host, name, location, flags);
	net = net;
#else
	pSound->PlayAtLocation(ret, host, name, location, flags, net);
#endif
	return ret;
}

bool PlaySound(object host, const cScrStr& name, object obj, eSoundSpecial flags, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlayAtObject(ret, host, name, obj, flags);
	net = net;
#else
	pSound->PlayAtObject(ret, host, name, obj, flags, net);
#endif
	return ret;
}

bool PlaySound(object host, const cScrStr& name, eSoundSpecial flags, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->Play(ret, host, name, flags);
	net = net;
#else
	pSound->Play(ret, host, name, flags, net);
#endif
	return ret;
}

bool PlayAmbient(object host, const cScrStr& name, eSoundSpecial flags, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlayAmbient(ret, host, name, flags);
	net = net;
#else
	pSound->PlayAmbient(ret, host, name, flags, net);
#endif
	return ret;
}

bool PlaySchema(object host, object schema, const cScrVec& location, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlaySchemaAtLocation(ret, host, schema, location);
	net = net;
#else
	pSound->PlaySchemaAtLocation(ret, host, schema, location, net);
#endif
	return ret;
}

bool PlaySchema(object host, object schema, object obj, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlaySchemaAtObject(ret, host, schema, obj);
	net = net;
#else
	pSound->PlaySchemaAtObject(ret, host, schema, obj, net);
#endif
	return ret;
}

bool PlaySchema(object host, object schema, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlaySchema(ret, host, schema);
	net = net;
#else
	pSound->PlaySchema(ret, host, schema, net);
#endif
	return ret;
}

bool PlaySchemaAmbient(object host, object schema, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlaySchemaAmbient(ret, host, schema);
	net = net;
#else
	pSound->PlaySchemaAmbient(ret, host, schema, net);
#endif
	return ret;
}

bool PlayEnvSchema(object host,const cScrStr& name, object obj, object subj, eEnvSoundLoc loc, eSoundNetwork net)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
#if (_DARKGAME == 1)
	pSound->PlayEnvSchema(ret, host, name, obj, subj, loc);
	net = net;
#else
	pSound->PlayEnvSchema(ret, host, name, obj, subj, loc, net);
#endif
	return ret;
}

bool PlayVoiceOver(object host, object schema)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
	pSound->PlayVoiceOver(ret, host, schema);
	return ret;
}

int HaltSound(object host ,const cScrStr& name, object obj)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	return pSound->Halt(host, name, obj);
}

bool HaltSchema(object host,const cScrStr& name, object obj)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
	pSound->HaltSchema(ret, host, name, obj);
	return ret;
}

long HaltSpeech(object obj)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	return pSound->HaltSpeech(obj);
}

bool PreLoad(const cScrStr& name)
{
	SService<ISoundScrSrv> pSound(g_pScriptManager);
	true_bool ret;
	pSound->PreLoad(ret, name);
	return ret;
}

cAnsiStr GetBookText(object iObj)
{
	SService<IPropertySrv> pPS(g_pScriptManager);
	cAnsiStr strText;

#if (_DARKGAME == 3)
	if (pPS->Possessed(iObj, "UseMsg"))
	{
		cMultiParm mpBook;
		pPS->Get(mpBook, iObj, "UseMsg", NULL);
		if (!cScrStr(mpBook).IsEmpty())
		{
			SService<IDataSrv> pDS(g_pScriptManager);
			cScrStr strBookText;
			pDS->GetString(strBookText, "UseMsg", mpBook, "", "strings");
			strText = strBookText;
			strBookText.Free();
		}
	}
#else
	if (pPS->Possessed(iObj, "Book"))
	{
		cMultiParm mpBook;
		pPS->Get(mpBook, iObj, "Book", NULL);
		if (!cScrStr((const char*) mpBook).IsEmpty())
		{
			SService<IDataSrv> pDS(g_pScriptManager);
			char* szBookFile = reinterpret_cast<char*>(alloca(10 + strlen(mpBook)));
			sprintf(szBookFile, "..\\books\\%s", static_cast<const char*>(mpBook));
			cScrStr strBookText;
			pDS->GetString(strBookText, szBookFile, "page_0", "", "strings");
			strText = strBookText;
			strBookText.Free();
		}
	}
#endif
	return strText;
}

static const bool stralnumtable[128] = {
	 0,0, 0,0,0,0,0, 0,1,1,1,1,1,1, 0,0,  0,0,0,0,0,0,0,0,0,0, 0, 0, 0, 0,0,0,
	1,0,1,0,0,0,0,1,1,1, 0, 0,1, 0,1,0,  0,0,0,0,0,0,0,0,0,0,1,1, 0, 0,0,0,
	 0,0, 0,0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0,0,  0,0,0,0,0,0,0,0,0,0, 0,1, 0,1,0,0,
	1,0, 0,0,0,0,0, 0, 0, 0, 0, 0, 0, 0, 0,0,  0,0,0,0,0,0,0,0,0,0, 0,1, 0,1,0,1
};

inline bool skipchar(unsigned char x)
{
	return ((x&0x80)==0) && (stralnumtable[x]);
}

int strnalnumcmp(const char* str1, const char* str2, size_t len)
{
	const char* p = str1;
	const char* q = str2;
	const char* end = p + len;
	int d;
	while (skipchar(*p)) ++p;
	while (skipchar(*q)) ++q;
	while (*p && *q)
	{
		d = tolower(*q) - tolower(*p);
		if (d != 0)
			return (d < 0) ? -1 : 1;
		while (skipchar(*++p)) ;
		while (skipchar(*++q)) ;
		if (p >= end)
			return 0;
	}
	d = *q - *p;
	return (d == 0) ? 0 : (d < 0) ? -1 : 1;
}

/* ObjectExists */

bool
ObjectExists (object target)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	true_bool result;
	pOS->Exists (result, target);
	return result;
}

/* ObjectToStr */

cAnsiStr
ObjectToStr (object target)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	cScrStr name; pOS->GetName (name, target);
	cAnsiStr result = name;
	//FIXME LGMM name.Free ();
	return result;
}

/* FormatObjectName */

cAnsiStr
FormatObjectName (object target)
{
	if (!target) return "None (0)";
	cAnsiStr result, name = ObjectToStr (target);
	if (!name.IsEmpty ())
		result.FmtStr ("%s (%d)", (const char*) name, (int) target);
	else
	{
		SInterface<ITraitManager> pTM (g_pScriptManager);
		name = ObjectToStr (pTM->GetArchetype (target));
		result.FmtStr ("a %s (%d)", (const char*) name, (int) target);
	}
	return result;
}

/* InheritsFrom */

bool
InheritsFrom (object ancestor, object target)
{
	if (!ancestor || !target) return false;
	SService<IObjectSrv> pOS (g_pScriptManager);
	true_bool result;
	pOS->InheritsFrom (result, target, ancestor);
	return result;
}

bool
InheritsFrom (const char* ancestor, object target)
{
	if (!ancestor || !target) return false;
	return InheritsFrom (StrToObject (ancestor), target);
}

/* DestroyObject */

void
DestroyObject (object destroy)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	if (destroy)
		pOS->Destroy (destroy);
}

/* CreateLink */

link
CreateLink (const char* _flavor, object source, object destination,
	const void* data)
{
	SService<ILinkSrv> pLS (g_pScriptManager);
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	SInterface<ILinkManager> pLM (g_pScriptManager);

	linkkind flavor = _flavor ? pLTS->LinkKindNamed (_flavor) : 0;
	if (!flavor || !source || !destination) return link ();

	link the_link;
	pLS->Create (the_link, flavor, source, destination);

	if (data)
		pLM->SetData (the_link, const_cast<void*> (data));

	return the_link;
}

/* DestroyLink */

void
DestroyLink (link destroy)
{
	if (!destroy) return;
	SService<ILinkSrv> pLS (g_pScriptManager);
	pLS->Destroy (destroy);
}

/* DebugLinks */

void
DebugLinks (object target)
{
	if (!target) return;
	DebugPrintf ("Dumping links for object %s...",
		(const char*) FormatObjectName (target));
	for (LinkIter link (target, Any, NULL); link; ++link)
		DebugPrintf ("...%s link to %s.",
			(const char*) link.FlavorName (),
			(const char*) FormatObjectName (link.Destination ()));
}

/* GetObjectParamColor */

ulong
GetObjectParamColor (object target, const char* param, ulong Default)
{
	ulong result = Default;
	char* param_value = GetObjectParamString (target, param);
	if (param_value)
	{
		result = strtocolor (param_value);
		g_pMalloc->Free (param_value);
	}
	return result;
}

/* AverageColors */

ulong
AverageColors (ulong color1, ulong color2, float weight)
{
	weight = std::max (0.0f, std::min (1.0f, weight));
	unsigned char r1 = getred (color1), r2 = getred (color2),
		b1 = getblue (color1), b2 = getblue (color2),
		g1 = getgreen (color1), g2 = getgreen (color2),
		r = r1 + weight * (r2 - r1),
		b = b1 + weight * (b2 - b1),
		g = g1 + weight * (g2 - g1);
	return makecolor (r, g, b);
}

/* CheckEngineVersion */

bool
CheckEngineVersion (int min_major, int min_minor)
{
	SService<IVersionSrv> pVS (g_pScriptManager);
	if (!pVS) return false;

	int major = 0, minor = 0;
	pVS->GetVersion (major, minor);

	return (major == min_major)
		? (minor >= min_minor) : (major > min_major);
}

/* HasPlayerTouched */

bool
HasPlayerTouched (object target, bool historic)
{
	if (!target) return false;

	object candidate = LinkIter (Any, target, "Contains").Source ();
	if (candidate && (InheritsFrom ("Avatar", candidate) || // player
	    LinkIter (candidate, Any, "PlayerFactory"))) // starting point
		return true;

	if (historic)
	{
		candidate = LinkIter (Any, target, "CulpableFor").Source ();
		if (InheritsFrom ("Avatar", candidate)) return true;
	}

	SService<IPropertySrv> pPS (g_pScriptManager);
	candidate = LinkIter (Any, target, "CreatureAttachment").Source ();
	if (candidate && pPS->Possessed (candidate, "Creature"))
	{
		cMultiParm type;
		pPS->Get (type, candidate, "Creature", NULL);
		if (type.type == kMT_Int &&
		    (int (type) == 1 || // PlayerArm
		     int (type) == 2)) // PlayerBowArm
			return true;
	}

	return false;
}

/* ExecuteCommand */

void
ExecuteCommand (const char* command)
{
	SService<IDebugScrSrv> pDbS (g_pScriptManager);
	pDbS->Command (command, "", "", "", "", "", "", "");
}



/* LinkIter */

LinkIter::LinkIter (object source, object dest, const char* flavor)
{
	SService<ILinkSrv> pLS (g_pScriptManager);
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	pLS->GetAll (links, flavor ? pLTS->LinkKindNamed (flavor) : 0,
		source, dest);
}

LinkIter::~LinkIter () noexcept
{}

LinkIter::operator bool () const
{
	return links.AnyLinksLeft ();
}

LinkIter&
LinkIter::operator++ ()
{
	links.NextLink ();
	AdvanceToMatch ();
	return *this;
}

LinkIter::operator link () const
{
	return links.AnyLinksLeft () ? links.Link () : link ();
}

linkkind
LinkIter::Flavor () const
{
	return links.AnyLinksLeft () ? links.Get ().flavor : linkkind ();
}

cAnsiStr
LinkIter::FlavorName () const
{
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	cScrStr name; pLTS->LinkKindName (name, Flavor ());
	cAnsiStr result = name;
	//FIXME LGMM name.Free ();
	return result;
}

object
LinkIter::Source () const
{
	return links.AnyLinksLeft () ? links.Get ().source : None;
}

object
LinkIter::Destination () const
{
	return links.AnyLinksLeft () ? links.Get ().dest : None;
}

const void*
LinkIter::GetData () const
{
	return links.AnyLinksLeft () ? links.Data () : NULL;
}

void
LinkIter::GetDataField (const char* field, cMultiParm& value) const
{
	if (!field)
		throw std::invalid_argument ("invalid link data field");
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	pLTS->LinkGetData (value, links.Link (), field);
}

void
LinkIter::AdvanceToMatch ()
{
	while (links.AnyLinksLeft () && !Matches ())
		links.NextLink ();
}



/* ScriptParamsIter */

ScriptParamsIter::ScriptParamsIter (object source, const char* _data,
                                    object destination)
	: LinkIter (source, destination, "ScriptParams"),
	  data (_data), only ()
{
	if (!source && !destination)
		throw std::invalid_argument ("invalid source/destination");
	if (_data && !strcmp (_data, "Self"))
		only = source;
	else if (_data && !strcmp (_data, "Player"))
		only = StrToObject ("Player");
	AdvanceToMatch ();
}

ScriptParamsIter::~ScriptParamsIter () noexcept
{}

ScriptParamsIter::operator bool () const
{
	return only ? true : LinkIter::operator bool ();
}

ScriptParamsIter&
ScriptParamsIter::operator++ ()
{
	if (only)
		only = 0;
	else
		LinkIter::operator++ ();
	return *this;
}

ScriptParamsIter::operator object () const
{
	return only ? only : Destination ();
}

bool
ScriptParamsIter::Matches ()
{
	if (!LinkIter::operator bool ())
		return false;
	else if (data)
		return !strnicmp (data, (const char*) GetData (),
			data.GetLength () + 1);
	else
		return true;
}

