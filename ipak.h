#pragma once

#include <cstdint>

#define SECTION_TYPE_INFO 1
#define SECTION_TYPE_DATA 2

#define IPAK_SECTION_ENTRY 1
#define IPAK_SECTION_DATA 2
#define IPAK_SECTION_METADATA 3

#define FORMAT_DXT1 0
#define FORMAT_DXT3 1
#define FORMAT_DXT5 2
#define FORMAT_A8R8G8B8 3

struct IPakHeader
{
	uint32_t magic;
	uint32_t version;
	uint32_t size;
	uint32_t sectionCount;
};

struct IPakSection
{
	uint32_t type;
	uint32_t offset;
	uint32_t size;
	uint32_t itemCount;
};

struct IPakDataChunkHeader
{
	uint32_t countAndOffset;
	uint32_t commands[31];
};

struct IPakIndexEntry
{
	uint64_t key;
	uint32_t offset;
	uint32_t size;
};

struct IPAKDataChunkMetaData
{
	uint64_t key;
	char name[60];
	int format;
	int offset;
	int size;
	int width;
	int height;
	int levels;
	int mip;
};