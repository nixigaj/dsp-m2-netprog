// Example code: A simple server side code, which echoes back the received message.
// Handle multiple socket connections with select and fd_set on Linux

#include <stdio.h>
#include <string.h>  // strlen
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>  // close
#include <arpa/inet.h>  // close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>  // FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define MAX_CLIENTS 30

static volatile int shutdown_server = FALSE;

static void send_all_client(const int *clients_fd, const char *msg);
static void release_client(int sd, struct sockaddr_in *address, int addrlen);
static void shutdown_handler(int s);

int main(void) {
	signal(SIGINT, shutdown_handler);
	signal(SIGTERM, shutdown_handler);

	int opt = TRUE;
	int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], i, valread, sd;
	struct sockaddr_in address;

	char buffer[1025];  // data buffer of 1K

	// set of socket descriptors
	fd_set readfds;

	// initialise all client_socket[] to 0 so not checked
	for (i = 0; i < MAX_CLIENTS; i++) {
		client_socket[i] = 0;
	}

	// create a master socket
	if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// set master socket to allow multiple connections,
	// this is just a good habit, it will work without this
	if (setsockopt(
		master_socket,
		SOL_SOCKET,
		SO_REUSEADDR,
		(char *) &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// type of socket created
	address.sin_family = AF_INET; // IPv4
	// Will accept incoming connections from any IP as
	// long as the correct port is open in the firewall
	address.sin_addr.s_addr = htonl(0x00000000); // 0.0.0.0
	address.sin_port = htons(PORT);

	// bind the socket to the address
	if (bind(master_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	printf("Listener on port %d \n", PORT);

	// try to specify maximum of 3 pending connections for the master socket
	if (listen(master_socket, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// accept the incoming connection
	addrlen = sizeof(address);
	puts("Waiting for connections...");

	while (TRUE) {
		// clear the socket set
		FD_ZERO(&readfds);

		// add master socket to set
		FD_SET(master_socket, &readfds);
		int max_sd = master_socket;

		// add child sockets to set
		for (i = 0; i < MAX_CLIENTS; i++) {
			// socket descriptor
			sd = client_socket[i];

			// if valid socket descriptor then add to read list
			if (sd > 0) {
				FD_SET(sd, &readfds);
			}

			// highest file descriptor number, need it for the select function
			if (sd > max_sd) {
				max_sd = sd;
			}
		}

		// wait for an activity on one of the sockets, timeout is NULL,
		// so wait indefinitely
		const int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if (activity < 0 && errno != EINTR) {
			printf("select error\n");
		}

		// The select function above will stop blocking when it encounters a SIGINT or SIGTERM
		if (shutdown_server) {
			for (i = 0; i < MAX_CLIENTS; i++) {
				// socket descriptor
				sd = client_socket[i];

				// if valid socket descriptor then send exit message and close
				if (sd > 0) {
					const char *exit_msg = "shutting down, goodbye\n";
					send(sd, exit_msg, strlen(exit_msg), 0);
					close(sd);
				}
			}
			close(master_socket);
			exit(0);
		}

		// If something happened on the master socket,
		// then it is an incoming connection
		if (FD_ISSET(master_socket, &readfds)) {
			// Introductory message
			const char *message = "ECHO Daemon v1.0 \r\n";

			if ((new_socket = accept(master_socket,
				(struct sockaddr *) &address,
				(socklen_t*) &addrlen)) < 0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			// inform user of socket number - used in send and receive commands
			printf("New connection, socket fd is %d, ip is: %s, port: %d\n",
				new_socket,
				inet_ntoa(address.sin_addr),
				ntohs(address.sin_port));

			// send new connection greeting message
			if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
				perror("send");
			}

			puts("Welcome message sent successfully");

			// add new socket to array of sockets
			for (i = 0; i < MAX_CLIENTS; i++) {
				// if position is empty
				if (client_socket[i] == 0) {
					client_socket[i] = new_socket;
					printf("Adding to list of sockets as %d\n", i);

					break;
				}
			}
		}

		// else its some IO operation on some other socket
		for (i = 0; i < MAX_CLIENTS; i++) {
			sd = client_socket[i];

			// Check if the client socket can be read from
			if (!(FD_ISSET(sd, &readfds))) {
				continue;
			}

			// Check if it was for closing, and also read the
			// incoming message
			if ((valread = read(sd, buffer, 1024)) == 0) {
				// Somebody disconnected, close socket
				release_client(sd, &address, addrlen);
				client_socket[i] = 0;

				continue;
			}

			// Handle message
			// set the string terminating NULL byte on the end
			// of the data read
			buffer[valread] = '\0';

			// It is not a control command
			if (buffer[0] != '#') {
				// Echo back the message that came in
				send(sd, buffer, strlen(buffer), 0);
				continue;
			}

			if (strcmp(buffer, "#quit\n") == 0 ||
				strcmp(buffer, "#quit\r\n") == 0)
			{
				// Command to disconnect, close socket
				release_client(sd, &address, addrlen);
				client_socket[i] = 0;
				continue;
			}

			if (strncmp(buffer, "#say ", 5) == 0) {
				send_all_client(client_socket, buffer + 5);
				continue;
			}

			// Invalid command
			const char *invalid_command_msg = "unknown control command\n";
			send(sd, invalid_command_msg, strlen(invalid_command_msg), 0);
		}
	}
}

static void send_all_client(const int *clients_fd, const char *msg) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		// socket descriptor
		const int sd = clients_fd[i];

		// if valid socket descriptor then send message
		if (sd > 0) {
			send(sd, msg, strlen(msg), 0);
		}
	}
}

static void release_client(const int sd, struct sockaddr_in *address, int addrlen) {
	// Get socket details and print
	getpeername(sd, (struct sockaddr*) address,
		(socklen_t*) &addrlen);
	printf("Host disconnected, ip %s, port %d \n",
		inet_ntoa(address->sin_addr), ntohs(address->sin_port));

	// Close the socket and mark as 0 in list for reuse
	close(sd);
}

static void shutdown_handler(const int s) {
	printf("Recieved exit signal %d, exiting...\n", s);
	shutdown_server = TRUE;
}
