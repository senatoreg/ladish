/* This file is part of Ingen.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "RequestAllObjectsEvent.hpp"
#include "interface/Responder.hpp"
#include "Engine.hpp"
#include "ObjectSender.hpp"
#include "ClientBroadcaster.hpp"
#include "ObjectStore.hpp"

namespace Ingen {


RequestAllObjectsEvent::RequestAllObjectsEvent(Engine& engine, SharedPtr<Shared::Responder> responder, SampleCount timestamp)
: QueuedEvent(engine, responder, timestamp)
{
}


void
RequestAllObjectsEvent::pre_process()
{
	_client = _engine.broadcaster()->client(_responder->client_uri());
	
	QueuedEvent::pre_process();
}


void
RequestAllObjectsEvent::post_process()
{
	if (_client) {
		_responder->respond_ok();

		// Everything is a child of the root patch, so this sends it all
		Patch* root = _engine.object_store()->find_patch("/");
		if (root)
			ObjectSender::send_patch(_client.get(), root, true);

	} else {
		_responder->respond_error("Unable to find client to send all objects");
	}
}


} // namespace Ingen

