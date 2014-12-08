/******************************************************************************
 *  KDHUDElement.hh
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

#ifndef KDHUDELEMENT_HH
#define KDHUDELEMENT_HH

#include <Thief/Thief.hh>
using namespace Thief;

class KDHUDElement : public HUDElement
{
public:
	enum class Position
	{
		NW,   NORTH,  NE,
		WEST, CENTER, EAST,
		SW,   SOUTH,  SE
	};

	enum class Symbol
	{
		NONE,
		ARROW,
		CROSSHAIRS,
		RETICULE,
		SQUARE
	};

	enum class Direction
	{
		NONE,
		LEFT,
		RIGHT,
	};

	struct Image
	{
		Image (Symbol symbol = Symbol::NONE,
			HUDBitmap::Ptr bitmap = nullptr);
		Symbol symbol;
		HUDBitmap::Ptr bitmap;
	};

protected:
	KDHUDElement (ZIndex priority);

	virtual void initialize ();
	virtual void deinitialize ();

	static const int MARGIN;

	CanvasPoint calculate_position (Position type, const CanvasSize& element,
		const CanvasPoint& offset = CanvasPoint (), int margin = MARGIN);

	static const Color SHADOW_COLOR;
	static const CanvasPoint SHADOW_OFFSET;

	void draw_text_shadowed (const String& text, CanvasPoint position);

	void draw_symbol (Symbol symbol, CanvasSize size,
		CanvasPoint position = CanvasPoint::ORIGIN,
		Direction direction = Direction::NONE,
		bool shadowed = false);

	CanvasPoint get_symbol_hotspot (Symbol symbol, CanvasSize size,
		Direction direction = Direction::NONE) const;

private:
	ZIndex priority;
};

namespace Thief {

template<>
struct ParameterConfig<KDHUDElement::Image> : public ParameterBase::Config
{
	ParameterConfig (KDHUDElement::Symbol _default_value
				= KDHUDElement::Symbol::NONE,
			bool _animated = false, bool _directional = false,
			bool _symbolic = true, bool _inheritable = true)
		: ParameterBase::Config (_inheritable),
		  default_value (_default_value),
		  animated (_animated),
		  directional (_directional),
		  symbolic (_symbolic)
	{}

	KDHUDElement::Symbol default_value;
	bool animated;
	bool directional;
	bool symbolic;
};

template<> bool Parameter<KDHUDElement::Image>::decode (const String& raw) const;
template<> String Parameter<KDHUDElement::Image>::encode () const; // undefined

} // namespace Thief

#endif // KDHUDELEMENT_HH

