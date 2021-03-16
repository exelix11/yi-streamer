#include "sysdvrSource.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include "util.h"

static inline int OpenListener(int port, bool LocalOnly)
{
	int err = 0, sock = -1;
	struct sockaddr_in temp;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	temp.sin_family = AF_INET;
	temp.sin_addr.s_addr = LocalOnly ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
	temp.sin_port = htons(port);

	const int optVal = 1;
	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&optVal, sizeof(optVal));
	if (err)
		return -2;

	err = bind(sock, (struct sockaddr*) & temp, sizeof(temp));
	if (err)
		return -3;

	err = listen(sock, 1);
	if (err)
		return -4;

	return sock;
}

static int listener;
static int client;

void CloseClient()
{
	if (client != -1)
		close(client);
	client = -1;

	if (listener != -1)
		close(listener);
	listener = -1;
}

void WaitForConnection()
{
	CloseClient();

	if ((listener = OpenListener(9911, false)) < 0)
		err("listener");

	client = accept(listener, NULL, NULL);
	if (client < 0)
		err("accept");
}

static struct {
	uint32_t magic;
	uint32_t size;
	uint64_t timestamp;
} header = {0xAAAAAAAA, 0, 0};

bool Send(const void* data, uint32_t len, uint32_t ts)
{
	header.size = len;
	header.timestamp = ts;
	send(client, &header, sizeof(header), 0);
	return send(client, data, len, 0) == len;
}