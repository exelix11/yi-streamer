#include <stdio.h>
#include <signal.h>

#include "source.h"
#include "wsStream.h"

int main()
{
	signal(SIGPIPE, SIG_IGN);
	
	SourceInit();
	SourceSetVideoChannel(CHANNEL_LOWRES);

	BeginWebsocketStream();
	
	SourceExit();
	return 0;
}