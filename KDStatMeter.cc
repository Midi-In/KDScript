/******************************************************************************
 *  KDStatMeter.cc
 *
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
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

#include "KDStatMeter.hh"



namespace Thief {

THIEF_ENUM_CODING (KDStatMeter::Style, CODE, CODE,
	THIEF_ENUM_VALUE (PROGRESS, "progress"),
	THIEF_ENUM_VALUE (UNITS, "units"),
	THIEF_ENUM_VALUE (GEM, "gem"),
)

THIEF_ENUM_CODING (KDStatMeter::Position, CODE, CODE,
	THIEF_ENUM_VALUE (NW, "nw"),
	THIEF_ENUM_VALUE (NORTH, "north", "n"),
	THIEF_ENUM_VALUE (NE, "ne"),
	THIEF_ENUM_VALUE (WEST, "west", "w"),
	THIEF_ENUM_VALUE (CENTER, "center", "c"),
	THIEF_ENUM_VALUE (EAST, "east", "e"),
	THIEF_ENUM_VALUE (SW, "sw"),
	THIEF_ENUM_VALUE (SOUTH, "south", "s"),
	THIEF_ENUM_VALUE (SE, "se"),
)

THIEF_ENUM_CODING (KDStatMeter::Orient, CODE, CODE,
	THIEF_ENUM_VALUE (HORIZ, "horiz"),
	THIEF_ENUM_VALUE (VERT, "vert"),
)

THIEF_ENUM_CODING (KDStatMeter::Component, CODE, CODE,
	THIEF_ENUM_VALUE (NONE),
	THIEF_ENUM_VALUE (X, "x"),
	THIEF_ENUM_VALUE (Y, "y"),
	THIEF_ENUM_VALUE (Z, "z"),
)

} // namespace Thief



const HUD::ZIndex
KDStatMeter::PRIORITY = 0;

const int
KDStatMeter::MARGIN = 16;



KDStatMeter::KDStatMeter (const String& _name, const Object& _host)
	: KDHUDElement (_name, _host, PRIORITY),

	  PERSISTENT (enabled),
	  PARAMETER_ (style, "stat_meter_style", Style::PROGRESS),
	  PARAMETER_ (image, "stat_meter_image", Symbol::NONE, true, false),
	  PARAMETER_ (spacing, "stat_meter_spacing", 8),
	  PARAMETER_ (_text, "stat_meter_text"),

	  PARAMETER_ (position, "stat_meter_position", Position::NW),
	  PARAMETER_ (offset_x, "stat_meter_offset_x", 0),
	  PARAMETER_ (offset_y, "stat_meter_offset_y", 0),
	  PARAMETER_ (orient, "stat_meter_orient", Orient::HORIZ),
	  PARAMETER_ (request_w, "stat_meter_width", -1),
	  PARAMETER_ (request_h, "stat_meter_height", -1),

	  PARAMETER_ (quest_var, "stat_source_qvar"),
	  PARAMETER_ (prop_name, "stat_source_property"),
	  PARAMETER_ (prop_field, "stat_source_field"),
	  PARAMETER_ (prop_comp, "stat_source_component", Component::NONE),
	  PARAMETER_ (prop_obj, "stat_source_object", host ()),

	  PARAMETER_ (_min, "stat_range_min", 0.0f),
	  PARAMETER_ (_max, "stat_range_max", 1.0f),
	  min (0.0f), max (1.0f),
	  PARAMETER_ (low, "stat_range_low", 25),
	  PARAMETER_ (high, "stat_range_high", 75),

	  PARAMETER_ (color_bg, "stat_color_bg", Color (0, 0, 0)),
	  PARAMETER_ (color_low, "stat_color_low", Color (255, 0, 0)),
	  PARAMETER_ (color_med, "stat_color_med", Color (255, 255, 0)),
	  PARAMETER_ (color_high, "stat_color_high", Color (0, 255, 0)),

	  value (0.0f),
	  value_pct (0.0f),
	  value_int (0),
	  value_tier (Tier::MEDIUM)
{
	listen_message ("PostSim", &KDStatMeter::on_post_sim);
	listen_message ("StatMeterOn", &KDStatMeter::on_on);
	listen_message ("StatMeterOff", &KDStatMeter::on_off);
	listen_message ("PropertyChange", &KDStatMeter::on_property_change);
}



void
KDStatMeter::initialize ()
{
	KDHUDElement::initialize ();

	enabled.init (Parameter<bool> (host (), "stat_meter", true));

	Property (host (), "DesignNote").subscribe (Object::SELF);
	update_text ();
	update_range ();
}

bool
KDStatMeter::prepare ()
{
	if (!enabled) return false;

	// Get the current value.
	QuestVar qvar (quest_var->data ());
	Property property (prop_obj, prop_name);
	if (qvar.exists ())
		value = qvar.get ();
	else if (property.exists ())
	{
		bool have_value = false;

		try
		{
			value = property.get_field<int> (prop_field);
			have_value = true;
		}
		catch (...) {} // not an int

		if (!have_value)
		try
		{
			value = property.get_field<float> (prop_field);
			have_value = true;
		}
		catch (...) {} // not a float

		if (!have_value && prop_comp != Component::NONE)
		try
		{
			Vector _value = property.get_field<Vector> (prop_field);
			switch (prop_comp)
			{
			case Component::X: value = _value.x; break;
			case Component::Y: value = _value.y; break;
			case Component::Z: value = _value.z; break;
			default: return false;
			}
			have_value = true;
		}
		catch (...) {} // not a vector

		if (!have_value) return false;
	}
	else
		return false;

	// Clamp the value and calculate derived versions.
	value = std::min (max, std::max (min, value));
	value_pct = (max != min) ? (value - min) / (max - min) : 0.0f;
	value_int = std::lround (value);
	if (value_pct * 100.0f <= low)
		value_tier = Tier::LOW;
	else if (value_pct * 100.0f >= high)
		value_tier = Tier::HIGH;
	else
		value_tier = Tier::MEDIUM;

	// Get the sizes of the canvas and the text.
	CanvasSize canvas = Engine::get_canvas_size (),
		text_size = get_text_size (text);

	// Calculate the size of the meter.
	CanvasSize request_size = get_request_size (),
		meter_size = request_size;
	if (style == Style::UNITS)
	{
		if (orient == Orient::HORIZ)
			meter_size.w = value_int * request_size.w +
				(value_int - 1) * spacing;
		else // Orient::VERT
			meter_size.h = value_int * request_size.h +
				(value_int - 1) * spacing;
	}
	else if (orient == Orient::VERT)
	{
		meter_size.w = request_size.h;
		meter_size.h = request_size.w;
	}

	// Calculate the size of the element.
	CanvasSize elem_size;
	bool show_text;
	if (orient == Orient::HORIZ && !text.empty ())
	{
		show_text = true;
		elem_size.w = std::max (meter_size.w, text_size.w);
		elem_size.h = meter_size.h + spacing + text_size.h;
	}
	else // Orient::VERT and/or empty text
	{
		show_text = false;
		elem_size = meter_size;
	}

	// Calculate the element position and relative positions of meter/text.
	CanvasPoint elem_pos;
	switch (position)
	{
	case Position::NW: case Position::WEST: case Position::SW: default:
		elem_pos.x = MARGIN;
		meter_pos.x = text_pos.x = 0;
		break;
	case Position::NORTH: case Position::CENTER: case Position::SOUTH:
		elem_pos.x = (canvas.w - elem_size.w) / 2;
		meter_pos.x = std::max (0, (text_size.w - meter_size.w) / 2);
		text_pos.x = std::max (0, (meter_size.w - text_size.w) / 2);
		break;
	case Position::NE: case Position::EAST: case Position::SE:
		elem_pos.x = canvas.w - MARGIN - elem_size.w;
		meter_pos.x = std::max (0, text_size.w - meter_size.w);
		text_pos.x = std::max (0, meter_size.w - text_size.w);
		break;
	}
	switch (position)
	{
	case Position::NW: case Position::NORTH: case Position::NE: default:
		elem_pos.y = MARGIN;
		meter_pos.y = 0;
		text_pos.y = meter_size.h + spacing; // text below
		break;
	case Position::WEST: case Position::CENTER: case Position::EAST:
		elem_pos.y = (canvas.h - elem_size.h) / 2;
		meter_pos.y = 0;
		text_pos.y = meter_size.h + spacing; // text below
		break;
	case Position::SW: case Position::SOUTH: case Position::SE:
		elem_pos.y = canvas.h - MARGIN - elem_size.h;
		meter_pos.y = show_text ? (text_size.h + spacing) : 0;
		text_pos.y = 0; // text above
		break;
	}

	set_position (elem_pos + CanvasPoint (offset_x, offset_y));
	set_size (elem_size);
	// If this script ever becomes an overlay, it won't be able to redraw
	// every frame. It would need to subscribe to the qvar/property.
	schedule_redraw ();
	return true;
}

void
KDStatMeter::redraw ()
{
	// Calculate value-sensitive colors.
	Color tier_color, color_blend = (value_pct < 0.5f)
		? Thief::interpolate (color_low, color_med, value_pct * 2.0f)
		: Thief::interpolate (color_med, color_high,
			(value_pct - 0.5f) * 2.0f);
	switch (value_tier)
	{
	case Tier::LOW: tier_color = color_low; break;
	case Tier::HIGH: tier_color = color_high; break;
	case Tier::MEDIUM: default: tier_color = color_med; break;
	}

	// Draw the text, if any.
	if (orient == Orient::HORIZ && !text.empty ())
	{
		set_drawing_color
			((style == Style::GEM) ? color_blend : tier_color);
		draw_text_shadowed (text, text_pos);
		set_drawing_offset (meter_pos);
	}

	CanvasSize request_size = get_request_size ();

	// Draw a progress bar meter.
	if (style == Style::PROGRESS)
	{
		CanvasRect area (request_size);
		if (orient == Orient::VERT) // invert dimensions
		{
			area.w = request_size.h;
			area.h = request_size.w;
		}

		set_drawing_color (color_bg);
		fill_area (area);

		set_drawing_color (tier_color);
		draw_box (area);

		if (orient == Orient::HORIZ)
		{
			area.w = std::lround (area.w * value_pct);
			if (position == Position::NE ||
			    position == Position::EAST ||
			    position == Position::SE) // right to left
				area.x = request_size.w - area.w;
		}
		else // Orient::VERT
		{
			area.h = std::lround (area.h * value_pct);
			if (position != Position::NW &&
			    position != Position::NORTH &&
			    position != Position::NE) // bottom to top
				area.y = request_size.w - area.h;
		}
		fill_area (area);
	}

	// Draw a meter with individual units.
	else if (style == Style::UNITS)
	{
		set_drawing_color (tier_color);
		CanvasPoint unit;
		for (int i = 1; i <= value_int; ++i)
		{
			if (image->bitmap)
				draw_bitmap
					(image->bitmap, HUDBitmap::STATIC, unit);
			else
				draw_symbol ((image->symbol != Symbol::NONE)
					? image->symbol : Symbol::SQUARE,
					request_size, unit);

			if (orient == Orient::HORIZ)
				unit.x += request_size.w + spacing;
			else // Orient::VERT
				unit.y += request_size.h + spacing;
		}
	}

	// Draw a solid gem meter.
	else if (style == Style::GEM)
	{
		if (image->bitmap)
			draw_bitmap (image->bitmap, std::lround (value_pct *
				(image->bitmap->count_frames () - 1)));
		else
		{
			CanvasRect area (request_size);
			if (orient == Orient::VERT)
			{
				area.w = request_size.h;
				area.h = request_size.w;
			}
			set_drawing_color (color_blend);
			fill_area (area);
			set_drawing_color (color_bg);
			draw_box (area);
		}
	}

	set_drawing_offset ();
}



CanvasSize
KDStatMeter::get_request_size () const
{
	CanvasSize request_size (128, 32);
	if (image->bitmap)
		request_size = image->bitmap->get_size ();
	else
	{
		if (style == Style::UNITS) request_size.w = 32;
		if (request_w > 0) request_size.w = request_w;
		if (request_h > 0) request_size.h = request_h;
	}
	return request_size;
}



Message::Result
KDStatMeter::on_post_sim (GenericMessage&)
{
	// Update anything based on an object that may have been absent.

	schedule_redraw ();
	prop_obj.reparse ();
	update_text ();
	update_range ();

	// Issue any one-time warnings on configuration.

	if (style == Style::PROGRESS && image->bitmap)
		mono () << "Warning: Bitmap image \"" << image.get_raw ()
			<< "\" will be ignored for a progress-style meter."
			<< std::endl;

	if (style != Style::UNITS && image->symbol != Symbol::NONE)
		mono () << "Warning: Symbol \"" << image.get_raw () << "\" will "
			"be ignored for a non-units-style meter." << std::endl;

	if (!quest_var->empty () && !prop_name->empty ())
		mono () << "Warning: Both a quest variable and a property were "
			"specified; will use the quest variable and ignore the "
			"property." << std::endl;

	if (quest_var->empty () && prop_name->empty ())
		mono () << "Warning: Neither a quest variable nor a property "
			"was specified; the stat meter will not be displayed."
			<< std::endl;

	if (min > max)
		mono () << "Warning: Minimum value " << min << " is greater "
			"than maximum value " << max << "." << std::endl;

	if (low < 0 || low > 100)
		mono () << "Warning: Low bracket " << low << "% is out of the "
			"range [0,100]." << std::endl;

	if (high < 0 || high > 100)
		mono () << "Warning: High bracket " << high << "% is out of the "
			"range [0,100]." << std::endl;

	if (low >= high)
		mono () << "Warning: Low bracket " << low << "% is greater than "
			"or equal to high bracket " << high << "%." << std::endl;

	return Message::CONTINUE;
}



Message::Result
KDStatMeter::on_on (GenericMessage&)
{
	enabled = true;
	return Message::CONTINUE;
}


Message::Result
KDStatMeter::on_off (GenericMessage&)
{
	enabled = false;
	return Message::CONTINUE;
}



Message::Result
KDStatMeter::on_property_change (PropertyChangeMessage& message)
{
	if (message.get_prop_name () == "DesignNote")
	{
		// Too many to check, so just assume the meter is affected.
		schedule_redraw ();

		update_text ();
		update_range ();
	}
	return Message::CONTINUE;
}

void
KDStatMeter::update_text ()
{
	text.clear ();

	if (_text->empty () || _text == "@none")
		{}

	else if (_text->front () != '@')
		text = Mission::get_text ("strings", "hud", _text);

	else if (_text == "@name")
	{
		if (prop_obj != Object::NONE)
			text = prop_obj->get_display_name ();
		else
			text = Mission::get_text ("strings", "hud", quest_var);
	}

	else if (_text == "@description")
	{
		if (prop_obj != Object::NONE)
			text = prop_obj->get_description ();
		else
			mono () << "Warning: \"@description\" is not a valid "
				"stat meter text source for a quest variable "
				"statistic." << std::endl;
	}

	else
		mono () << "Warning: \"" << _text << "\" is not a valid stat "
			"meter text source." << std::endl;
}

void
KDStatMeter::update_range ()
{
	min = _min;
	max = _max;

	// Provide special min/max defaults if neither is set.
	if (!_min.exists () && !_max.exists () && quest_var->empty ())
	{
		if (prop_name == "AI_Visibility" && prop_field == "Level")
		{
			min = 0.0f;
			max = 100.0f;
		}
		else if (prop_name == "HitPoints")
		{
			Property max_hp (prop_obj, "MAX_HP");
			if (max_hp.exists ())
			{
				min = 0.0f;
				max = max_hp.get<int> ();
			}
		}
	}
}

