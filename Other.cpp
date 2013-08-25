/******************************************************************************
 *  Other.cpp: miscellaneous useful scripts
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

#include "Other.h"
#include <lg/objects.h>
#include <ScriptLib.h>
#include <darkhook.h>
#include "utils.h"



/* KDCarried */

cScr_Carried::cScr_Carried (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_Carried::OnSim (sSimMsg*, cMultiParm&)
{
	// add FrobInert if requested
	if (GetObjectParamBool (ObjId (), "inert_until_dropped", false))
		AddSingleMetaProperty ("FrobInert", ObjId ());

	return S_OK;
}

long
cScr_Carried::OnCreate (sScrMsg*, cMultiParm&)
{
	// only proceed for objects created in-game
	SService<IVersionSrv> pVS (g_pScriptManager);
	if (pVS->IsEditor () == 1) return S_FALSE;

	// add FrobInert if requested
	if (GetObjectParamBool (ObjId (), "inert_until_dropped", false))
		AddSingleMetaProperty ("FrobInert", ObjId ());

	return S_OK;
}

long
cScr_Carried::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!stricmp (pMsg->message, "Drop") ||
	    !stricmp (pMsg->message, "CarrierBrainDead"))
	{
		Drop ();
		return S_OK;
	}

	if (!stricmp (pMsg->message, "CarrierAlerted") &&
	    pMsg->data.type == kMT_Int) // new alert level
	{
		int min_alert = GetObjectParamInt (ObjId (), "drop_on_alert", 0),
			new_alert = int (pMsg->data);
		if (min_alert > 0 && min_alert <= new_alert)
		{
			Drop ();
			return S_OK;
		}
		else
			return S_FALSE;
	}

	if (!stricmp (pMsg->message, "FixPhysics"))
	{
		FixPhysics ();
		return S_OK;
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}

void
cScr_Carried::Drop ()
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	object drop = ObjId ();

	for (LinkIter container (0, ObjId (), "Contains");
	     container; ++container)
		// unlink from Contains carrier
		DestroyLink (container);

	for (LinkIter creature (0, ObjId (), "CreatureAttachment");
	     creature; ++creature)
		// unlink from CreatureAttachment carrier
		DestroyLink (creature);

	LinkIter dattach (ObjId (), 0, "DetailAttachement");
	if (dattach)
	{
		// unlink from DetailAttachement carrier, destroying this object
		object ai = dattach.Destination ();
		DestroyLink (dattach);

		// clone self and add reference link to AI
		pOS->Create (drop, ObjId ());
		CreateLink ("CulpableFor", ai, drop);
	}

	// become physical if not already, ensuring model is appropriate
	SService<IPropertySrv> pPS (g_pScriptManager);
	if (!pPS->Possessed (drop, "PhysType"))
	{
		// create an OBB model to allow check of object dimensions
		pPS->Add (drop, "PhysType");
		pPS->Set (drop, "PhysType", "Type", kPMT_OBB);

		// schedule return to correctly sizes sphere
		SimplePost (drop, drop, "FixPhysics");
	}

	// teleport to own position to avoid winding up at origin (?!)
	cScrVec position; pOS->Position (position, drop);
	cScrVec facing; pOS->Facing (facing, drop);
	pOS->Teleport (drop, position, facing, 0);

	// give self a push to cause drop
	SService<IPhysSrv> pPhS (g_pScriptManager);
	pPhS->SetVelocity (drop, {0.0, 0.0, -0.1});

	// remove FrobInert if requested
	if (GetObjectParamBool (drop, "inert_until_dropped", false))
		RemoveSingleMetaProperty ("FrobInert", drop);

	// turn off the object and others if requested
	if (GetObjectParamBool (drop, "off_when_dropped", false))
	{
		SimpleSend (drop, drop, "TurnOff");
		CDSend ("TurnOff", drop);
	}
}

void
cScr_Carried::FixPhysics ()
{
	// get object dimensions
	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm _dims;
	pPS->Get (_dims, ObjId (), "PhysDims", "Size");

	// switch to sphere model and set appropriate radius
	pPS->Set (ObjId (), "PhysType", "Type", kPMT_Sphere);
	pPS->Set (ObjId (), "PhysType", "# Submodels", 1);
	if (_dims.type == kMT_Vector)
	{
		cScrVec dims = cScrVec (_dims);
		float radius =
			std::max (dims.x, std::max (dims.y, dims.z)) / 2.0;
		pPS->Set (ObjId (), "PhysDims", "Radius 1", radius);
	}
}



/* KDCarrier */

cScr_Carrier::cScr_Carrier (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseAIScript (pszName, iHostObjId)
{}

long
cScr_Carrier::OnSim (sSimMsg* pMsg, cMultiParm&)
{
	if (pMsg->fStarting) CreateAttachments ();
	return S_OK;
}

long
cScr_Carrier::OnCreate (sScrMsg*, cMultiParm&)
{
	CreateAttachments ();
	return S_OK;
}

long
cScr_Carrier::OnAIModeChange (sAIModeChangeMsg* pMsg, cMultiParm&)
{
	if (pMsg->mode == kAIM_Dead) // slain or knocked out
		NotifyCarried ("CarrierBrainDead");
	return S_OK;
}

long
cScr_Carrier::OnAlertness (sAIAlertnessMsg* pMsg, cMultiParm&)
{
	if (pMsg->level > 0)
		NotifyCarried ("CarrierAlerted", pMsg->level);
	return S_OK;
}

void
cScr_Carrier::NotifyCarried (const char* message, const cMultiParm& data)
{
	// notify Contains objects
	for (LinkIter contained (ObjId (), 0, "Contains");
	     contained; ++contained)
		SimpleSend (ObjId (), contained.Destination (),
			message, data);

	// notify CreatureAttachment objects
	for (LinkIter attachment (ObjId (), 0, "CreatureAttachment");
	     attachment; ++attachment)
		SimpleSend (ObjId (), attachment.Destination (),
			message, data);

	// notify ~DetailAttachement objects
	for (LinkIter attachment (0, ObjId (), "DetailAttachement");
	     attachment; ++attachment)
		SimpleSend (ObjId (), attachment.Source (),
			message, data);
}

void
cScr_Carrier::CreateAttachments ()
{
	if (!GetObjectParamBool (ObjId (), "create_attachments", true)) return;

	SInterface<ITraitManager> pTM (g_pScriptManager);
	SInterface<IObjectQuery> tree;

	tree = pTM->Query (ObjId (), kTraitQueryMetaProps | kTraitQueryFull);
	if (!tree) return;

	for (; ! tree->Done (); tree->Next ())
		for (LinkIter archetypes (tree->Object (), 0,
			"CreatureAttachment"); archetypes; ++archetypes)
		{
			cMultiParm joint;
			archetypes.GetDataField ("Joint", joint);
			CreateAttachment (archetypes.Destination (), joint);
		}
}

void
cScr_Carrier::CreateAttachment (object archetype, int joint)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);

	if (GetOneLinkByData ("CreatureAttachment", ObjId (), 0,
			NULL, &joint, sizeof (joint)) != 0)
		return; // don't attach a second object to the same joint

	DEBUG_PRINTF ("attaching %s to joint %d",
		(const char*) FormatObjectName (archetype), joint);

	object attachment;
	pOS->Create (attachment, archetype);
	if (!attachment) return;

	link attach_link =
		CreateLink ("CreatureAttachment", ObjId (), attachment);
	if (attach_link)
		pLTS->LinkSetData (attach_link, "Joint", joint);
}



/* KDShortText */

cScr_ShortText::cScr_ShortText (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_ShortText::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	if (GetObjectParamBool (ObjId (), "text_on_frob", true))
		DisplayMessage ();
	return S_OK;
}

long
cScr_ShortText::OnWorldSelect (sScrMsg*, cMultiParm&)
{
	if (GetObjectParamBool (ObjId (), "text_on_focus", true))
		DisplayMessage ();
	return S_OK;
}

void
cScr_ShortText::DisplayMessage ()
{
	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm msgid;
	if (pPS->Possessed (ObjId (), "Book"))
		pPS->Get (msgid, ObjId (), "Book", NULL);
	else if (char* _msgid = GetObjectParamString (ObjId (), "text"))
	{
		msgid = _msgid;
		g_pMalloc->Free (_msgid);
	}

	if (msgid.type != kMT_String) return;

	SService<IDataSrv> pDS (g_pScriptManager);
	cScrStr msgstr;
	pDS->GetString (msgstr, "short", msgid, "", "strings");
	if (msgstr.IsEmpty ()) return;

	ShowString (msgstr,
		GetObjectParamTime (ObjId (), "text_time", CalcTextTime (msgstr)),
		GetObjectParamColor (ObjId (), "text_color", 0));
}



/* HUDSubtitle */

const int
HUDSubtitle::BORDER = 1;

const int
HUDSubtitle::PADDING = 8;

HUDSubtitle::HUDSubtitle (object host, const char* _text, ulong _color)
	: HUDElement (host), text (_text), color (_color)
{
	Initialize ();
}

HUDSubtitle::~HUDSubtitle ()
{}

bool
HUDSubtitle::Prepare ()
{
	// get canvas and text size and calculate element size
	CanvasSize canvas = GetCanvasSize (),
		text_size = GetTextSize (text),
		elem_size;
	elem_size.w = 2*BORDER + 2*PADDING + text_size.w;
	elem_size.h = 2*BORDER + 2*PADDING + text_size.h;

	// get host's position in canvas coordinates
	CanvasPoint host_pos;
	if (GetHost () == StrToObject ("Player"))
		host_pos = CanvasPoint (canvas) / 2;
	else
	{
		host_pos = ObjectCentroidToCanvas (GetHost ());
		if (host_pos == OFFSCREEN) return false;
	}

	// calculate element position
	CanvasPoint elem_pos;
	elem_pos.x = std::max (0, std::min (canvas.w - elem_size.w,
		host_pos.x - elem_size.w / 2));
	elem_pos.y = std::max (0, std::min (canvas.h - elem_size.h,
		host_pos.y + PADDING));

	SetPosition (elem_pos);
	if (NeedsRedraw ()) SetSize (elem_size);
	return true;
}

void
HUDSubtitle::Redraw ()
{
	CanvasSize elem_size = GetSize ();

	// draw background
	SetDrawingColor (0x000000);
	FillArea ();

	// draw border
	SetDrawingColor (color);
	DrawLine (ORIGIN, CanvasPoint (0, elem_size.h));
	DrawLine (ORIGIN, CanvasPoint (elem_size.w, 0));
	DrawLine (CanvasPoint (0, elem_size.h), CanvasPoint (elem_size));
	DrawLine (CanvasPoint (elem_size.w, 0), CanvasPoint (elem_size));

	// draw text
	DrawText (text, CanvasPoint (BORDER+PADDING, BORDER+PADDING));
}



/* KDSubtitled */

cScr_Subtitled::cScr_Subtitled (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  SCRIPT_VAROBJ (Subtitled, last_schema, iHostObjId),
	  element (NULL)
{}

cScr_Subtitled::~cScr_Subtitled ()
{
	EndSubtitle (0);
}

long
cScr_Subtitled::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!stricmp (pMsg->message, "Subtitle") &&
	    pMsg->data.type == kMT_Int) // schema
	{
		object host = (pMsg->data2.type == kMT_Int)
			? int (pMsg->data2) : pMsg->from;
		Subtitle (host, int (pMsg->data));
		return S_OK;
	}
	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_Subtitled::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "EndSubtitle") &&
	    pMsg->data.type == kMT_Int) // schema
	{
		EndSubtitle (int (pMsg->data));
		return S_OK;
	}
	return cBaseScript::OnTimer (pMsg, mpReply);
}

long
cScr_Subtitled::OnEndScript (sScrMsg*, cMultiParm&)
{
	EndSubtitle (0);
	return S_OK;
}

void
cScr_Subtitled::Subtitle (object host, object schema)
{
	// confirm host and schema objects are valid
	if (!ObjectExists (host) || !ObjectExists (schema) ||
	    !InheritsFrom ("Schema", schema))
	{
		DebugPrintf ("Warning: can't subtitle invalid host/schema pair "
			"%d/%d.", int (host), int (schema));
		return;
	}

	// get subtitle text
	SService<IDataSrv> pDS (g_pScriptManager);
	cScrStr text;
	pDS->GetString (text, "subtitles", ObjectToStr (schema), "", "strings");
	if (text.IsEmpty ()) return;

	// end any previous subtitle on this object
	EndSubtitle (0);

	// get or calculate schema duration
	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm _duration;
	if (pPS->Possessed (schema, "ScriptTiming"))
		pPS->Get (_duration, schema, "ScriptTiming", NULL);
	int duration = (_duration.type == kMT_Int)
		? int (_duration) : CalcTextTime (text, 700);

	// get subtitle color
	ulong color = GetObjectParamColor (schema, "subtitle_color",
		GetObjectParamColor (host, "subtitle_color", 0xffffff));

	try
	{
		// check whether to use HUD
		SService<IQuestSrv> pQS (g_pScriptManager);
		if (pQS->Get ("subtitles_use_hud") != 1)
			throw std::runtime_error ("nevermind");

		// create a HUD subtitle element
		element = new HUDSubtitle (host, text, color);

		// schedule its deletion
		last_schema = schema;
		SetTimedMessage ("EndSubtitle", duration, kSTM_OneShot, schema);
	}
	catch (...) // go the old-fashioned way
	{
		element = NULL;
		ShowString (text, duration, color);
	}
}

void
cScr_Subtitled::EndSubtitle (object schema)
{
	// try to prevent early end of later subtitle
	if (schema)
	{
		if (!last_schema.Valid () || last_schema != schema) return;
		last_schema.Clear ();
	}

	// clear HUD subtitle element, if any
	if (element)
	{
		delete element;
		element = NULL;
	}
}



/* KDSubtitledAI */

cScr_SubtitledAI::cScr_SubtitledAI (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cScr_Subtitled (pszName, iHostObjId)
{
	DarkHookInitializeService (g_pScriptManager, g_pMalloc);
}

long
cScr_SubtitledAI::OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply)
{
	try
	{
		SService<IDarkHookScriptService> pDHS (g_pScriptManager);
		pDHS->InstallPropHook (ObjId (), kDHNotifyDefault,
			"Speech", ObjId ());
	}
	catch (no_interface&)
	{
		DebugString ("The DarkHook service could not be located. "
			"This AI's speech will not be subtitled.");
	}
	return cScr_Subtitled::OnBeginScript (pMsg, mpReply);
}

long
cScr_SubtitledAI::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	long result = cScr_Subtitled::OnMessage (pMsg, mpReply);
	if (!!stricmp (pMsg->message, "DHNotify")) return result;

	auto dh = static_cast<sDHNotifyMsg*> (pMsg);
	if (dh->typeDH != kDH_Property) return result;
	if (!!strcmp (dh->sProp.pszPropName, "Speech")) return result;

	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm schema, flags;

	pPS->Get (schema, ObjId (), "Speech", "schemaID");
	if (schema.type != kMT_Int || int (schema) == 0)
		return result; // not a valid schema

	pPS->Get (flags, ObjId (), "Speech", "flags");
	if (flags.type != kMT_Int || int (flags) != 1)
		return result; // not a start-of-schema change

	// confirm speech is in (estimated) earshot of player
	SService<IObjectSrv> pOS (g_pScriptManager);
	cScrVec host_pos; pOS->Position (host_pos, ObjId ());
	cScrVec player_pos; pOS->Position (player_pos, StrToObject ("Player"));
	if (host_pos.Distance (player_pos) >= 80.0)
		return result; // too far away

	// display the subtitle
	Subtitle (ObjId (), int (schema));
	return S_OK;
}



/* KDSubtitledVO */

cScr_SubtitledVO::cScr_SubtitledVO (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cScr_Subtitled (pszName, iHostObjId),
	  SCRIPT_VAROBJ (SubtitledVO, played, iHostObjId)
{}

long
cScr_SubtitledVO::OnTurnOn (sScrMsg*, cMultiParm&)
{
	// get one SoundDescription-linked schema
	object schema =
		LinkIter (ObjId (), 0, "SoundDescription").Destination ();
	if (!schema) return S_FALSE;

	// only display subtitle once (per object)
	if (played.Valid () && bool (played))
		return S_FALSE;
	else
		played = true;

	// display the subtitle
	Subtitle (StrToObject ("Player"), schema);
	return S_OK;
}

