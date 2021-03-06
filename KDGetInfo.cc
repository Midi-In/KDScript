/******************************************************************************
 *  KDGetInfo.cc
 *
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include "KDGetInfo.hh"

KDGetInfo::KDGetInfo (const String& _name, const Object& _host)
	: Script (_name, _host)
{
	listen_message ("BeginScript", &KDGetInfo::on_begin_script);
	listen_message ("DarkGameModeChange", &KDGetInfo::on_mode_change);
	listen_message ("UpdateVariables", &KDGetInfo::on_update_variables);
	listen_message ("EndScript", &KDGetInfo::on_end_script);
}

Message::Result
KDGetInfo::on_begin_script (Message&)
{
	// Some settings aren't available yet, so wait for next message cycle.
	GenericMessage ("UpdateVariables").post (host (), host ());
	return Message::HALT;
}

Message::Result
KDGetInfo::on_mode_change (GameModeMessage& message)
{
	if (message.event == GameModeMessage::RESUME)
		GenericMessage ("UpdateVariables").send (host (), host ());
	return Message::HALT;
}

Message::Result
KDGetInfo::on_update_variables (Message&)
{
	QuestVar ("info_directx_version") = Engine::get_directx_version ();

	CanvasSize canvas = Engine::get_canvas_size ();
	QuestVar ("info_display_height") = canvas.h;
	QuestVar ("info_display_width") = canvas.w;

	if (Engine::has_config ("sfx_eax"))
		QuestVar ("info_has_eax") = Engine::get_config<int> ("sfx_eax");

	if (Engine::has_config ("fogging"))
		QuestVar ("info_has_fog") = Engine::get_config<int> ("fogging");

	if (Engine::has_config ("game_hardware"))
		QuestVar ("info_has_hw3d") =
			Engine::get_config<int> ("game_hardware");

	if (Engine::has_config ("enhanced_sky"))
		QuestVar ("info_has_sky") =
			Engine::get_config<int> ("enhanced_sky");

	if (Engine::has_config ("render_weather"))
		QuestVar ("info_has_weather") =
			Engine::get_config<int> ("render_weather");

#ifdef IS_THIEF2
	if (Engine::get_mode () == Engine::Mode::GAME)
		QuestVar ("info_mission") = Mission::get_number ();
#endif // IS_THIEF2

	QuestVar ("info_mode") = (Engine::get_mode () == Engine::Mode::EDIT)
		? 1 : Engine::is_editor () ? 2 : 0;

	Version version = Engine::get_version ();
	QuestVar ("info_version_major") = version.major;
	QuestVar ("info_version_minor") = version.minor;

	return Message::HALT;
}

Message::Result
KDGetInfo::on_end_script (Message&)
{
	QuestVar ("info_directx_version").clear ();
	QuestVar ("info_display_height").clear ();
	QuestVar ("info_display_width").clear ();
	QuestVar ("info_has_eax").clear ();
	QuestVar ("info_has_fog").clear ();
	QuestVar ("info_has_hw3d").clear ();
	QuestVar ("info_has_sky").clear ();
	QuestVar ("info_has_weather").clear ();
	QuestVar ("info_mission").clear ();
	QuestVar ("info_mode").clear ();
	QuestVar ("info_version_major").clear ();
	QuestVar ("info_version_minor").clear ();
	return Message::HALT;
}

