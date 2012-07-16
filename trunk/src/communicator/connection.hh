#ifndef __CONNECTION_HH__
#define __CONNECTION_HH__

#include <string>
#include "../common/enums.hh"

#include <stdint.h>

using namespace std;

/**
 * Handle connections to components
 */

class Connection {
public:

	/**
	 * Constructor
	 */

	Connection();

	/**
	 * Constructor (connect immediately)
	 * @param ip Destination IP
	 * @param port Destination Port
	 * @param connectionType Destination Type
	 */

	Connection(string ip, uint16_t port, ComponentType connectionType);

	/**
	 * Establish connection with a component
	 * @param ip Destination IP
	 * @param port Destination Port
	 * @param connectionType Destination Type
	 * @return Socket Descriptor
	 */

	uint32_t doConnect(string ip, uint16_t port, ComponentType connectionType);

	/**
	 * Disconnect from component
	 */

	void disconnect();

	/**
	 * Send a message to the connection
	 * @param msg Pointer to message to send
	 * @return Bytes sent
	 */

	uint32_t sendMessage (Message *message);

	/**
	 * Receive a message from the connection
	 * @return Pointer to message received
	 */

	char* recvMessage ();

	/**
	 * Get back socket descriptor
	 * @return socket descriptor of this connection
	 */

	uint32_t getSockfd();

private:
	uint32_t sendn (int sd, const void* buf, int buf_len);
	uint32_t recvn (int sd, const void* buf, int buf_len);

	uint32_t _sockfd;
	string _ip;
	uint16_t _port;
	ComponentType _connectionType;
};

#endif
