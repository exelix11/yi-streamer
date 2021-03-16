#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define CHANNEL_HIRES 0
#define CHANNEL_LOWRES 1
#define CHANNEL_AUDIO 2

void SourceInit();
void SourceExit();

void SourceSetVideoChannel(int channel);
int SourceGetVideoChannel();

typedef struct {
// Internal control info
	uint32_t allocated; // Tracks how much is allocated for data
	uint32_t size; // Actual data size in this block
	uint32_t ts; // Timestamp of this block
	uint16_t index; // Last read index
	uint16_t padding_unused;
// Data sent through websocket
	struct {
		uint32_t tsdiff; // Difference of Timestamp from previous block
		uint8_t channel; // Channel index
		char data[];
	} __attribute__((packed)) payload;
} ReadBlock;

void VideoResync(ReadBlock* block);
void AudioResync(ReadBlock* block);

static inline void FreeBlock(ReadBlock** block)
{
	if (*block)
		free(*block);
	*block = NULL;
}

static inline void InitBlock(ReadBlock** block)
{
	*block = malloc(sizeof(ReadBlock));
	memset(*block, 0, sizeof(ReadBlock));	
}
 
void SourceRead(ReadBlock** block);

void SourceReadVideo(ReadBlock** target);

void SourceReadAudio(ReadBlock** target);

ReadBlock* SourceReadAV(ReadBlock** video, ReadBlock** audio);
