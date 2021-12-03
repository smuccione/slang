#include "Utility/settings.h"

#include <stdint.h>
#include <memory.h>
#include "encodingTables.h"

uint8_t	hexDecodingTable[256];
uint8_t	webBase64DecodingTable[256];
uint8_t	hexEncodingTable[16];
uint8_t	webBase64EncodingTable[64];
uint8_t	caseInsenstiveTable[256];

static hexDecodingTableInit			init1;
static hexEncodingTableInit			init2;
static webBase64DecodingTableInit	init3;
static webBase64EncodingTableInit	init4;
static caseInsenstiveTableInit		init5;

