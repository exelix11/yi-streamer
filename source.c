#include "source.h"
#include "util.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define CIRCULAR_SZ 0x12C

#define FIRST_STREAM_BASE 0x9640

#define OFFSET_HIRES 0
#define SIZE_HIRES 0x100000

#define OFFSET_LORES (OFFSET_HIRES + SIZE_HIRES)
#define SIZE_LORES 0x5A000

#define OFFSET_AUDIO (OFFSET_LORES + SIZE_LORES)
#define SIZE_AUDIO 0x10000

static int target_ch = CHANNEL_LOWRES;
static uint32_t target_offset = OFFSET_LORES;

int SourceGetVideoChannel()
{
	return target_ch;
}

void SourceSetVideoChannel(int channel)
{
	switch (channel)
	{
		case CHANNEL_HIRES:
			target_ch = CHANNEL_HIRES;
			target_offset = OFFSET_HIRES;
			break;
		default:
			log("wrong channel, defaulting to lowres");
		case CHANNEL_LOWRES:
			target_ch = CHANNEL_LOWRES;
			target_offset = OFFSET_LORES;
			break;
	}
}

typedef struct {
	char ready;
	char unk1[3];
	uint32_t offset;
	uint32_t length;
	uint32_t ts;
	char nal_unit_type;
	char unk[3];
	uint32_t unk3;
	uint32_t unk4;
	uint32_t unk5;	
} __attribute__((packed)) BufferEntry;

_Static_assert(sizeof(BufferEntry) == 0x20, "");

typedef struct
{
	struct {
		uint16_t start;
		uint16_t end;
		uint32_t newestIFrameTs;
		uint32_t unk; // Something related to size, maybe free space
		uint32_t newestFrameTs;
	} Header;

	BufferEntry entries[CIRCULAR_SZ];

} __attribute__((packed)) BufferHeader;

_Static_assert(sizeof(BufferHeader) == 0x10 + CIRCULAR_SZ * 0x20, "");
_Static_assert(offsetof(BufferHeader, entries) == 0x10, "");

typedef struct
{
	BufferHeader Buffer[4];
	char data[];
} __attribute__((packed)) ViewFile;

_Static_assert(offsetof(ViewFile, data) == FIRST_STREAM_BASE, "");

static volatile ViewFile* view;
static struct stat viewStat;

void SourceInit()
{
	int viewFileHandle = open("/tmp/view", 2);
  	if (viewFileHandle < 0)
		err("open view file");
	log("opened\n");

	fstat(viewFileHandle, &viewStat);
  	log("stat %lx\n", viewStat.st_size);

	view = (volatile ViewFile*)mmap(0, viewStat.st_size, 3, 1, viewFileHandle, 0);
	if (view == MAP_FAILED)
		err("map failed");
	log("mapped\n");
}

void SourceExit()
{
	munmap((void*)view, viewStat.st_size);
	view = NULL;
}

static char* SPS;
static int SPS_Len;

static char* PPS;
static int PPS_Len;


void VideoResync(ReadBlock* block)
{
try_again: // Try to sync again if we didn't find all the info we need, in practice doesn't seem to happen
	if (SPS)
	{
		free(SPS);
		SPS = NULL;
		SPS_Len = 0; 
	}

	if (PPS)
	{
		free(PPS);
		PPS = NULL;
		PPS_Len = 0; 
	}

	volatile BufferHeader *h = &view->Buffer[target_ch];
	char* d = (char*)view->data + target_offset;

	uint16_t sendIndex = 0;

	uint16_t index = h->Header.start + 10;
	index = index % CIRCULAR_SZ;
	while (index != h->Header.end)
	{
		if (SPS && PPS && sendIndex != -1)
			break; // Found all we needed

		if (!h->entries[index].ready)
		{
			puts("Buffer is not ready, this shouldn't happen.");
			goto try_again;
		}	

		if (h->entries[index].ts == h->Header.newestIFrameTs)
			sendIndex = index;

		//printhex((char*)&h->entries[index], sizeof(h->entries[index]));
		//log("OFF %x SIZE %x\n", h->entries[index].offset, h->entries[index].length);
		//fflush(stdout);
		char seq[5];
		memcpy(seq, d + h->entries[index].offset, 5);
		if (seq[0] || seq[1] || seq[2] || seq[3] != 1 || !seq[4])
		{
			log("Wrong nal header: %x %x %x %x %x\n", seq[0], seq[1], seq[2], seq[3], seq[4]);
		}
		else
		{
			char t = seq[4] & 0x1F;
			log("type at %x %d ", index, t); 

			char** dest = NULL;
			int* sz = 0;

			if (t == 7 && !SPS)
			{
				dest = &SPS;
				sz = &SPS_Len;
				log("SPS !\n");
			}
			else if (t == 8 && !PPS)
			{
				dest = &PPS;
				sz = &PPS_Len;
				log("PPS !\n");
			}
			else log("\n");

			if (dest && sz && !*dest)
			{
				*dest = malloc(h->entries[index].length);
				*sz = h->entries[index].length;
				memcpy(*dest, d + h->entries[index].offset, *sz);
			}
		}

		if (++index >= CIRCULAR_SZ)
			index = 0;
	}

	if (PPS)
	{
		log("Found PPS\n");
		printhex(PPS, PPS_Len);
	}
	else 
	{
		log("PPS Not found !\n");
		goto try_again;
	}

	if (SPS)
	{
		log("Found SPS\n");
		printhex(SPS, SPS_Len);
	}
	else 
	{
		log("SPS Not found !\n");
		goto try_again;
	}

	if (sendIndex != -1)
	{
		log("Found I frame %d\n", sendIndex);
		// To calculate the duration at the first read
		block->ts = h->entries[sendIndex].ts;
		block->index = sendIndex;
	}
	else 
	{
		log("I frame Not found !\n");
		goto try_again;
	}
}
 
void AudioResync(ReadBlock* block)
{
	// Nothing to do here, just add last index
	uint16_t index = view->Buffer[CHANNEL_AUDIO].Header.end;
	
	block->index = index;
	block->ts = view->Buffer[CHANNEL_AUDIO].entries[index].ts;
}

static inline void EnsureBlockSize(ReadBlock** block, uint32_t size)
{
	if ((*block)->allocated < size)
	{
		log("[%d] Reallocating %d\n", (*block)->payload.channel, size + sizeof(ReadBlock));
		*block = realloc(*block, size + sizeof(ReadBlock));
		(*block)->allocated = size;
	}
}

static inline void DoReadToBlock(ReadBlock** target, volatile BufferEntry* entry, const char* data)
{
	uint32_t ts = entry->ts;
	uint32_t size = entry->length;

	EnsureBlockSize(target, size);

	memcpy((*target)->payload.data, data + entry->offset, size);
	(*target)->size = size;
	(*target)->payload.tsdiff = ts - (*target)->ts;
	(*target)->ts = ts;

	if (++(*target)->index >= CIRCULAR_SZ)
		(*target)->index = 0;
}

// Should work but couldn't manage to play it in the browser
static inline bool ReadAudio(ReadBlock** target)
{
	if (!view->Buffer[CHANNEL_AUDIO].entries[(*target)->index].ready)
		return false;

	DoReadToBlock(
		target, 
		&view->Buffer[CHANNEL_AUDIO].entries[(*target)->index],
		(char*)view->data + OFFSET_AUDIO	
	);

	return true;
}

static inline bool ReadVideo(ReadBlock** target)
{
	if (!view->Buffer[target_ch].entries[(*target)->index].ready)
		return false;

	DoReadToBlock(
		target, 
		&view->Buffer[target_ch].entries[(*target)->index],
		(char*)view->data + target_offset
	);

	return true;
}

void SourceReadVideo(ReadBlock** target)
{
	while (!ReadVideo(target));
}

void SourceReadAudio(ReadBlock** target)
{
	while (!ReadAudio(target));
}

// Same as ReadAudio
ReadBlock* SourceReadAV(ReadBlock** video, ReadBlock** audio)
{
	while (1)
	{
		if (ReadVideo(video))
			return *video;
		if (ReadAudio(audio))
			return *audio;
	}
}