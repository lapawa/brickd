/*
 * brickd
 * Copyright (C) 2012 Matthias Bolte <matthias@tinkerforge.com>
 *
 * network.c: Network specific functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <string.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

#include "network.h"

#include "event.h"
#include "log.h"
#include "packet.h"
#include "socket.h"
#include "usb.h" // FIXME
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_NETWORK

static int _port = 4223;
static Array _clients = ARRAY_INITIALIZER;
static EventHandle _server_socket = INVALID_EVENT_HANDLE;

static void network_handle_accept(void *opaque) {
	EventHandle client_socket;
	Client *client;

	(void)opaque;

	// accept new client socket
	if (socket_accept(_server_socket, &client_socket, NULL, NULL) < 0) {
		if (!errno_would_block() && !errno_interrupted()) {
			log_error("Could not accept new socket: %s (%d)",
			          get_errno_name(errno), errno);
		}

		// FIXME: does this work in case of blocking or interrupted
		// need to retry on interrupted and blocking should never happend
		return;
	}

	// enable non-blocking
	if (socket_set_non_blocking(client_socket, 1) < 0) {
		log_error("Could not enable non-blocking mode for socket (handle: %d): %s (%d)",
		          client_socket, get_errno_name(errno), errno);

		socket_destroy(client_socket);

		return;
	}

	// append to client array
	client = array_append(&_clients);

	if (client == NULL) {
		log_error("Could not append to client array: %s (%d)",
		          get_errno_name(errno), errno);

		socket_destroy(client_socket);

		return;
	}

	if (client_create(client, client_socket) < 0) {
		array_remove(&_clients, _clients.count - 1, NULL);
		socket_destroy(client_socket);

		return;
	}

	log_info("Added new client (socket: %d)", client->socket);
}

int network_init(void) {
	struct sockaddr_in server_address;

	log_debug("Initializing network subsystem");

	if (array_create(&_clients, 32, sizeof(Client)) < 0) {
		log_error("Could not create client array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	if (socket_create(&_server_socket, AF_INET, SOCK_STREAM, 0) < 0) {
		log_error("Could not create server socket: %s (%d)",
		          get_errno_name(errno), errno);

		// FIXME: free client array
		return -1;
	}

	if (socket_set_address_reuse(_server_socket, 1) < 0) {
		// FIXME: close socket
		log_error("Could not enable address-reuse mode for server socket: %s (%d)",
		          get_errno_name(errno), errno);
		// FIXME: free client array
		return -1;
	}

	memset(&server_address, 0, sizeof(server_address));

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(_port);

	if (socket_bind(_server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
		// FIXME: close socket
		log_error("Could not bind server socket to port %d: %s (%d)",
		          _port, get_errno_name(errno), errno);

		// FIXME: free client array
		return -1;
	}

	if (socket_listen(_server_socket, 10) < 0) {
		// FIXME: close socket
		log_error("Could not listen to server socket on port %d: %s (%d)",
		          _port, get_errno_name(errno), errno);

		// FIXME: free client array
		return -1;
	}

	if (socket_set_non_blocking(_server_socket, 1) < 0) {
		// FIXME: close socket
		log_error("Could not enable non-blocking mode for server socket: %s (%d)",
		          get_errno_name(errno), errno);
		// FIXME: free client array
		return -1;
	}

	if (event_add_source(_server_socket, EVENT_READ, network_handle_accept, NULL) < 0) {
		// FIXME: close socket
		// FIXME: free client array
		return -1;
	}

	return 0;
}

void network_exit(void) {
	log_debug("Shutting down network subsystem");

	// FIXME

	array_destroy(&_clients, (FreeFunction)client_destroy);

	socket_destroy(_server_socket);
}

void network_client_disconnected(Client *client) {
	int i = array_find(&_clients, client);

	if (i < 0) {
		log_error("Client (socket: %d) not found in client array", client->socket);
	} else {
		array_remove(&_clients, i, (FreeFunction)client_destroy);
	}
}

void network_dispatch_packet(Packet *packet) {
	int i;
	Client *client;
	int rc;
	int dispatched = 0;

	log_debug("Dispatching response (U: %u, L: %u, F: %u, S: %u, E: %u) to %d client(s)",
	          packet->header.uid,
	          packet->header.length,
	          packet->header.function_id,
	          packet->header.sequence_number,
	          packet->header.error_code,
	          _clients.count);

	for (i = 0; i < _clients.count; ++i) {
		client = array_get(&_clients, i);

		rc = client_dispatch_packet(client, packet, 0);

		if (rc < 0) {
			continue;
		} else if (rc > 0) {
			dispatched = 1;
		}
	}

	if (dispatched) {
		return;
	}

	log_debug("Broadcasting response because no client had a pending request matching it");

	for (i = 0; i < _clients.count; ++i) {
		client = array_get(&_clients, i);

		client_dispatch_packet(client, packet, 1);
	}
}