/******************************************************************************
 *  KDSubtitled.hh: HUDSubtitle, KDSubtitled, KDSubtitledAI, KDSubtitledVO
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

#ifndef KDSUBTITLED_HH
#define KDSUBTITLED_HH

#include <Thief/Thief.hh>
using namespace Thief;



class HUDSubtitle : public HUDElement
{
public:
	HUDSubtitle (const Object& speaker, const Object& schema,
		const String& text, Color color);
	virtual ~HUDSubtitle ();

	Object get_schema () const { return schema; }

private:
	virtual bool prepare ();
	virtual void redraw ();

	static const HUD::ZIndex PRIORITY;
	static const int BORDER, PADDING;
	static const Color BACKGROUND_COLOR;

	Object speaker;
	Object schema;
	String text;
	Color color;
};



class KDSubtitled : public Script
{
public:
	virtual ~KDSubtitled ();

protected:
	KDSubtitled (const String& name, const Object& host);

	static const float EARSHOT;
	static const Color DEFAULT_COLOR;

	bool start_subtitle (const Object& speaker, const Object& schema);
	void finish_subtitle (const Object& schema = Object::ANY);

private:
	Message::Result on_subtitle (GenericMessage&);
	Message::Result on_finish_subtitle (TimerMessage&);
	Message::Result on_end_script (GenericMessage&);

	HUDSubtitle* element;
};



class KDSubtitledAI : public KDSubtitled
{
public:
	KDSubtitledAI (const String& name, const Object& host);

private:
	virtual void initialize ();
	Message::Result on_property_change (PropertyChangeMessage&);
};



class KDSubtitledVO : public KDSubtitled
{
public:
	KDSubtitledVO (const String& name, const Object& host);

private:
	Message::Result on_turn_on (GenericMessage&);
};



#endif // KDSUBTITLED_HH

