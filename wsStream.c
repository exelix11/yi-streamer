#include "wsStream.h"
#include "source.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>
#include <string.h>

#include <pthread.h>

static pthread_mutex_t clientsLock;
static int connectedClients = 0;
static int clients[MAX_CLIENTS];

static bool addClient(int fd)
{
	pthread_mutex_lock(&clientsLock);
	if (connectedClients >= MAX_CLIENTS)
	{
		pthread_mutex_unlock(&clientsLock);
		return false;
	}

	clients[connectedClients] = fd;
	connectedClients++;
	pthread_mutex_unlock(&clientsLock);

	return true;
}

static void removeClient(int fd)
{
	pthread_mutex_lock(&clientsLock);
	int i = 0;
	for (; i < connectedClients && clients[i] != fd; i++) ;

	if (i == connectedClients)
	{
		pthread_mutex_unlock(&clientsLock);
		return;
	}

	memmove(clients + i, clients + i + 1, connectedClients - i - 1);
	connectedClients--;
	pthread_mutex_unlock(&clientsLock);
}

static pthread_mutex_t sourceReadLock;
static pthread_t senderTHread;
static bool senderShouldResync = true;
static void* SenderThread(void* _arg)
{
	(void)_arg;

	ReadBlock* video = NULL;

	InitBlock(&video);
	
	while (true) {
		if (connectedClients == 0)
		{
			senderShouldResync = true;
			sleep(1);
		}
		else
		{
			pthread_mutex_lock(&sourceReadLock);
			if (senderShouldResync)
			{
				senderShouldResync = false;
				video->payload.channel = SourceGetVideoChannel();
				VideoResync(video);
			}
			SourceReadVideo(&video);
			pthread_mutex_unlock(&sourceReadLock);
			
			pthread_mutex_lock(&clientsLock);
			int clin = connectedClients;
			for (int i = 0; i < clin; i++)
				ws_sendframe_bin(
					clients[i], 
					(char*)&video->payload, 
					video->size + sizeof(video->payload), 
					false
				);			
			pthread_mutex_unlock(&clientsLock);
		}
	}
	
	return NULL;
}

void onopen(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
	log("Connection opened, client: %d | addr: %s\n", fd, cli);
	free(cli);

	addClient(fd);
}

void onclose(int fd)
{
	removeClient(fd);
}

void onmessage(int fd, const unsigned char *msg, uint64_t size, int type)
{
	if (size <= 0)
		return;

	log("Switching streams %c\n", *msg);

	pthread_mutex_lock(&sourceReadLock);
	pthread_mutex_lock(&clientsLock);

	SourceSetVideoChannel(*msg == 'L' ? CHANNEL_LOWRES : CHANNEL_HIRES);
	senderShouldResync = true;

	pthread_mutex_unlock(&clientsLock);
	pthread_mutex_unlock(&sourceReadLock);
}

void BeginWebsocketStream()
{
	pthread_mutex_init(&clientsLock, NULL);
	pthread_mutex_init(&sourceReadLock, NULL);
	pthread_create(&senderTHread, NULL, SenderThread, NULL);

	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;

	ws_socket(&evs, 8080); 
}