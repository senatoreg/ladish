/* This file is part of Machina.
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 * 
 * Machina is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Machina is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <cstdlib>
#include "raul/SharedPtr.h"
#include "machina/Machine.hpp"
#include "machina/Node.hpp"
#include "machina/Edge.hpp"
#include "machina/MidiAction.hpp"

using namespace std;

namespace Machina {


Machine::Machine()
	: _is_activated(false)
	, _is_finished(false)
	, _time(0)
{
}


Machine::~Machine()
{
}


/** Set the MIDI sink to be used for executing MIDI actions.
 *
 * MIDI actions will silently do nothing unless this call is passed an
 * existing Raul::MIDISink before running.
 */
void
Machine::set_sink(SharedPtr<Raul::MIDISink> sink)
{
	_sink = sink;
}


void
Machine::add_node(SharedPtr<Node> node)
{
	//cerr << "ADDING NODE " << node.get() << endl;
	assert(_nodes.find(node) == _nodes.end());
	_nodes.push_back(node);
}


void
Machine::remove_node(SharedPtr<Node> node)
{
	_nodes.erase(_nodes.find(node));
}


/** Exit all active states and reset time to 0.
 */
void
Machine::reset()
{
	if (!_is_finished) {
		for (Nodes::const_iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
			const SharedPtr<Node> node = (*n);

			if (node->is_active())
				node->exit(_sink.lock(), _time);

			assert(! node->is_active());
		}
	}
	
	for (size_t i=0; i < MAX_ACTIVE_NODES; ++i) {
		_active_nodes[i].reset();
	}

	_time = 0;
	_is_finished = false;
}


/** Return the active Node with the earliest exit time.
 */
SharedPtr<Node>
Machine::earliest_node() const
{	
	SharedPtr<Node> earliest;
	
	for (size_t i=0; i < MAX_ACTIVE_NODES; ++i) {
		const SharedPtr<Node> node = _active_nodes[i];
		
		if (node) {
			assert(node->is_active());
			if (!earliest || node->exit_time() < earliest->exit_time()) {
				earliest = node;
			}
		}
	}
	/*for (Nodes::const_iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
		const SharedPtr<Node> node = (*n);
		
		if (node->is_active())
			if (!earliest || node->exit_time() < earliest->exit_time())
				earliest = node;
	}*/

	return earliest;
}


/** Enter a state at the current _time.
 * 
 * Returns true if node was entered, or false if the maximum active nodes has been reached.
 */
bool
Machine::enter_node(const SharedPtr<Raul::MIDISink> sink, const SharedPtr<Node> node)
{
	assert(!node->is_active());

	/* FIXME: Would be best to use the MIDI note here as a hash key, at least
	 * while all actions are still MIDI notes... */
	size_t index = (rand() % MAX_ACTIVE_NODES);
	for (size_t i=0; i < MAX_ACTIVE_NODES; ++i) {
		if (_active_nodes[index] == NULL) {
			node->enter(sink, _time);
			assert(node->is_active());
			_active_nodes[index] = node;
			return true;
		}
		index = (index + 1) % MAX_ACTIVE_NODES;
	}

	// If we get here, ran out of active node spots.  Don't enter node
	return false;
}



/** Exit an active node at the current _time.
 */
void
Machine::exit_node(SharedPtr<Raul::MIDISink> sink, const SharedPtr<Node> node)
{
	node->exit(sink, _time);
	assert(!node->is_active());

	for (size_t i=0; i < MAX_ACTIVE_NODES; ++i) {
		if (_active_nodes[i] == node) {
			_active_nodes[i].reset();
		}
	}

	// Activate all successors to this node
	// (that aren't aready active right now)
	for (Node::Edges::const_iterator s = node->outgoing_edges().begin();
			s != node->outgoing_edges().end(); ++s) {
		
		assert((*s)->head() != node); // no loops

		const double rand_normal = rand() / (double)RAND_MAX; // [0, 1]
		
		if (rand_normal <= (*s)->probability()) {
			SharedPtr<Node> head = (*s)->head();

			if (!head->is_active())
				enter_node(sink, head);
		}
	}
}


/** Run the machine for @a nframes frames.
 *
 * Returns the duration of time the machine actually ran (from 0 to nframes).
 * 
 * Caller can check is_finished() to determine if the machine still has any
 * active states.  If not, time() will return the exact time stamp the
 * machine actually finished on (so it can be restarted immediately
 * with sample accuracy if necessary).
 */
BeatCount
Machine::run(const Raul::TimeSlice& time)
{
	using namespace std;
	if (_is_finished) {
		return 0;
	}

	const SharedPtr<Raul::MIDISink> sink = _sink.lock();

	const BeatCount cycle_end = _time + time.length_beats();

	assert(_is_activated);

	// Initial run, enter all initial states
	if (_time == 0) {
		bool entered = false;
		if ( ! _nodes.empty()) {
			for (Nodes::const_iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
				
				assert( ! (*n)->is_active());
				
				if ((*n)->is_initial()) {
					if (enter_node(sink, (*n)))
						entered = true;
				}

			}
		}
		if (!entered) {
			_is_finished = true;
			return 0;
		}
	}
	
	BeatCount this_time = 0;

	while (true) {

		SharedPtr<Node> earliest = earliest_node();

		// No more active states, machine is finished
		if (!earliest) {
			_is_finished = true;
			break;

		// Earliest active state ends this cycle
		// Must do comparison in ticks here to avoid rounding up and executing
		// an event outside the current cycle
		} else if (time.beats_to_ticks(earliest->exit_time())
				<= time.beats_to_ticks(cycle_end)) {
			this_time += earliest->exit_time() - _time;
			_time = earliest->exit_time();
			exit_node(sink, earliest);

		// Earliest active state ends in the future, done this cycle
		} else {
			_time = cycle_end;
			this_time = time.length_beats(); // ran the entire cycle
			break;
		}

	}

	assert(this_time <= time.length_beats());
	return this_time;
}


/** Push a node onto the learn stack.
 *
 * NOT realtime (actions are allocated here).
 */
void
Machine::learn(SharedPtr<LearnRequest> learn)
{
	std::cerr << "Learn" << std::endl;

	/*LearnRequest request(node,
		SharedPtr<MidiAction>(new MidiAction(4, NULL)),
		SharedPtr<MidiAction>(new MidiAction(4, NULL)));*/

	//_pending_learns.push_back(learn);
	_pending_learn = learn;
}


void
Machine::write_state(Raul::RDFWriter& writer)
{
	using Raul::RdfId;

	writer.add_prefix("machina", "http://drobilla.net/ns/machina#");

	writer.write(RdfId(RdfId::RESOURCE, ""),
			RdfId(RdfId::RESOURCE, "rdf:type"),
			RdfId(RdfId::RESOURCE, "machina:Machine"));

	size_t count = 0;

	for (Nodes::const_iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
	
		//cerr << "Writing node " << count++ << " state." << endl;

		(*n)->write_state(writer);

		if ((*n)->is_initial()) {
			writer.write(RdfId(RdfId::RESOURCE, ""),
					RdfId(RdfId::RESOURCE, "machina:initialNode"),
					(*n)->id());
		} else {
			writer.write(RdfId(RdfId::RESOURCE, ""),
					RdfId(RdfId::RESOURCE, "machina:node"),
					(*n)->id());
		}
	}

	count = 0;

	for (Nodes::const_iterator n = _nodes.begin(); n != _nodes.end(); ++n) {
		
		//cerr << "Writing node " << count++ << " edges: ";
	
		for (Node::Edges::const_iterator e = (*n)->outgoing_edges().begin();
			e != (*n)->outgoing_edges().end(); ++e) {
			
			//cerr << ".";
			
			(*e)->write_state(writer);
		
			writer.write(RdfId(RdfId::RESOURCE, ""),
				RdfId(RdfId::RESOURCE, "machina:edge"),
				(*e)->id());
		}

		//cerr << endl;

	}
}


} // namespace Machina

