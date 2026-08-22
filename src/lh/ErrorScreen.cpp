/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "ErrorScreen.h"

#include "common/Exception.h"
#include "common/Module.h"
#include "common/Object.h"
#include "common/Matrix.h"
#include "modules/event/Event.h"
#include "modules/font/Font.h"
#include "modules/graphics/Graphics.h"
#include "modules/mouse/Mouse.h"
#include "modules/timer/Timer.h"
#include "modules/window/Window.h"

#include <vector>

namespace love
{
namespace lh
{

bool showErrorScreen(const std::string &message)
{
	auto window = Module::getInstance<love::window::Window>(Module::M_WINDOW);
	auto graphics = Module::getInstance<love::graphics::Graphics>(Module::M_GRAPHICS);
	auto event = Module::getInstance<love::event::Event>(Module::M_EVENT);
	auto fontmodule = Module::getInstance<love::font::Font>(Module::M_FONT);
	auto timer = Module::getInstance<love::timer::Timer>(Module::M_TIMER);
	auto mouse = Module::getInstance<love::mouse::Mouse>(Module::M_MOUSE);

	if (window == nullptr)
		return false;

	try
	{
		if (graphics == nullptr || event == nullptr || !graphics->isCreated() || !window->isOpen())
		{
			bool opened = false;
			if (graphics != nullptr && event != nullptr)
			{
				love::window::WindowSettings settings;
				opened = window->setWindow(800, 600, &settings);
			}
			if (!opened)
			{
				window->showMessageBox("Initialization error", message, love::window::Window::MESSAGEBOX_ERROR, false);
				return true;
			}
		}

		if (mouse != nullptr)
			mouse->setVisible(true);

		graphics->reset();
		if (fontmodule != nullptr)
		{
			love::font::TrueTypeRasterizer::Settings settings;
			StrongRef<love::font::Rasterizer> rasterizer(fontmodule->newTrueTypeRasterizer(15, settings), Acquire::NORETAIN);
			StrongRef<love::graphics::Font> font(graphics->newFont(rasterizer.get()), Acquire::NORETAIN);
			graphics->setFont(font.get());
		}
		graphics->setColor(Colorf(1.0f, 1.0f, 1.0f, 1.0f));
		graphics->origin();

		std::vector<love::font::ColoredString> text;
		text.push_back({"Error\n\n" + message + "\n\n(Press Escape to close)", Colorf(1.0f, 1.0f, 1.0f, 1.0f)});

		while (true)
		{
			event->pump();
			love::event::Message *m = nullptr;
			while (event->poll(m) && m != nullptr)
			{
				StrongRef<love::event::Message> msg(m, Acquire::NORETAIN);
				if (msg->name == "quit")
					return true;
				if (msg->name == "keypressed" && !msg->args.empty())
				{
					const Variant &key = msg->args[0];
					bool escape = false;
					if (key.getType() == Variant::SMALLSTRING)
						escape = std::string(key.getData().smallstring.str, key.getData().smallstring.len) == "escape";
					else if (key.getType() == Variant::STRING)
						escape = std::string(key.getData().string->str, key.getData().string->len) == "escape";
					if (escape)
						return true;
				}
			}

			if (graphics->isActive())
			{
				float pos = 60.0f;
				love::graphics::OptionalColorD color(ColorD(89.0 / 255.0, 157.0 / 255.0, 220.0 / 255.0, 1.0));
				graphics->clear(color, OptionalInt(0), OptionalDouble(1.0));
				graphics->printf(text, (float) graphics->getWidth() - pos * 2.0f, love::graphics::Font::ALIGN_LEFT, Matrix4(pos, pos, 0, 1, 1, 0, 0, 0, 0));
				graphics->present(nullptr);
			}

			if (timer != nullptr)
				timer->sleep(0.1);
		}
	}
	catch (const love::Exception &)
	{
		return false;
	}
}

} // lh
} // love
