#pragma once

#include <stdint.h>

extern	uint8_t	hexDecodingTable[(size_t)256];
extern	uint8_t	webBase64DecodingTable[(size_t)256];
extern	uint8_t	hexEncodingTable[16];
extern	uint8_t	webBase64EncodingTable[64];
extern	uint8_t	caseInsenstiveTable[256];

class hexDecodingTableInit {
	public:
	hexDecodingTableInit () noexcept
	{
		memset ( hexDecodingTable, 0xFF, sizeof ( hexDecodingTable ) );
		hexDecodingTable[(size_t)'0'] = 0;
		hexDecodingTable[(size_t)'1'] = 1;
		hexDecodingTable[(size_t)'2'] = 2;
		hexDecodingTable[(size_t)'3'] = 3;
		hexDecodingTable[(size_t)'4'] = 4;
		hexDecodingTable[(size_t)'5'] = 5;
		hexDecodingTable[(size_t)'6'] = 6;
		hexDecodingTable[(size_t)'7'] = 7;
		hexDecodingTable[(size_t)'8'] = 8;
		hexDecodingTable[(size_t)'9'] = 9;
		hexDecodingTable[(size_t)'a'] = 10;
		hexDecodingTable[(size_t)'b'] = 11;
		hexDecodingTable[(size_t)'c'] = 12;
		hexDecodingTable[(size_t)'d'] = 13;
		hexDecodingTable[(size_t)'e'] = 14;
		hexDecodingTable[(size_t)'f'] = 15;
		hexDecodingTable[(size_t)'A'] = 10;
		hexDecodingTable[(size_t)'B'] = 11;
		hexDecodingTable[(size_t)'C'] = 12;
		hexDecodingTable[(size_t)'D'] = 13;
		hexDecodingTable[(size_t)'E'] = 14;
		hexDecodingTable[(size_t)'F'] = 15;
	}
};

class webBase64DecodingTableInit {
	public:
	webBase64DecodingTableInit () noexcept
	{
		memset ( webBase64DecodingTable, 0, sizeof ( webBase64DecodingTable ) );
		webBase64DecodingTable[(size_t)'A'] = 0;
		webBase64DecodingTable[(size_t)'B'] = 1;
		webBase64DecodingTable[(size_t)'C'] = 2;
		webBase64DecodingTable[(size_t)'D'] = 3;
		webBase64DecodingTable[(size_t)'E'] = 4;
		webBase64DecodingTable[(size_t)'F'] = 5;
		webBase64DecodingTable[(size_t)'G'] = 6;
		webBase64DecodingTable[(size_t)'H'] = 7;
		webBase64DecodingTable[(size_t)'I'] = 8;
		webBase64DecodingTable[(size_t)'J'] = 9;
		webBase64DecodingTable[(size_t)'K'] = 10;
		webBase64DecodingTable[(size_t)'L'] = 11;
		webBase64DecodingTable[(size_t)'M'] = 12;
		webBase64DecodingTable[(size_t)'N'] = 13;
		webBase64DecodingTable[(size_t)'O'] = 14;
		webBase64DecodingTable[(size_t)'P'] = 15;
		webBase64DecodingTable[(size_t)'Q'] = 16;
		webBase64DecodingTable[(size_t)'R'] = 17;
		webBase64DecodingTable[(size_t)'S'] = 18;
		webBase64DecodingTable[(size_t)'T'] = 19;
		webBase64DecodingTable[(size_t)'U'] = 20;
		webBase64DecodingTable[(size_t)'V'] = 21;
		webBase64DecodingTable[(size_t)'W'] = 22;
		webBase64DecodingTable[(size_t)'X'] = 23;
		webBase64DecodingTable[(size_t)'Y'] = 24;
		webBase64DecodingTable[(size_t)'Z'] = 25;
		webBase64DecodingTable[(size_t)'a'] = 26;
		webBase64DecodingTable[(size_t)'b'] = 27;
		webBase64DecodingTable[(size_t)'c'] = 28;
		webBase64DecodingTable[(size_t)'d'] = 29;
		webBase64DecodingTable[(size_t)'e'] = 30;
		webBase64DecodingTable[(size_t)'f'] = 31;
		webBase64DecodingTable[(size_t)'g'] = 32;
		webBase64DecodingTable[(size_t)'h'] = 33;
		webBase64DecodingTable[(size_t)'i'] = 34;
		webBase64DecodingTable[(size_t)'j'] = 35;
		webBase64DecodingTable[(size_t)'k'] = 36;
		webBase64DecodingTable[(size_t)'l'] = 37;
		webBase64DecodingTable[(size_t)'m'] = 38;
		webBase64DecodingTable[(size_t)'n'] = 39;
		webBase64DecodingTable[(size_t)'o'] = 40;
		webBase64DecodingTable[(size_t)'p'] = 41;
		webBase64DecodingTable[(size_t)'q'] = 42;
		webBase64DecodingTable[(size_t)'r'] = 43;
		webBase64DecodingTable[(size_t)'s'] = 44;
		webBase64DecodingTable[(size_t)'t'] = 45;
		webBase64DecodingTable[(size_t)'u'] = 46;
		webBase64DecodingTable[(size_t)'v'] = 47;
		webBase64DecodingTable[(size_t)'w'] = 48;
		webBase64DecodingTable[(size_t)'x'] = 49;
		webBase64DecodingTable[(size_t)'y'] = 50;
		webBase64DecodingTable[(size_t)'z'] = 51;
		webBase64DecodingTable[(size_t)'0'] = 52;
		webBase64DecodingTable[(size_t)'1'] = 53;
		webBase64DecodingTable[(size_t)'2'] = 54;
		webBase64DecodingTable[(size_t)'3'] = 55;
		webBase64DecodingTable[(size_t)'4'] = 56;
		webBase64DecodingTable[(size_t)'5'] = 57;
		webBase64DecodingTable[(size_t)'6'] = 58;
		webBase64DecodingTable[(size_t)'7'] = 59;
		webBase64DecodingTable[(size_t)'8'] = 60;
		webBase64DecodingTable[(size_t)'9'] = 61;
		webBase64DecodingTable[(size_t)'+'] = 62;
		webBase64DecodingTable[(size_t)'/'] = 63;
		webBase64DecodingTable[(size_t)'='] = 64;
	}
};

class hexEncodingTableInit {
	public:
	hexEncodingTableInit () noexcept
	{
		hexEncodingTable[0] = '0';
		hexEncodingTable[1] = '1';
		hexEncodingTable[2] = '2';
		hexEncodingTable[3] = '3';
		hexEncodingTable[4] = '4';
		hexEncodingTable[5] = '5';
		hexEncodingTable[6] = '6';
		hexEncodingTable[7] = '7';
		hexEncodingTable[8] = '8';
		hexEncodingTable[9] = '9';
		hexEncodingTable[10] = 'a';
		hexEncodingTable[11] = 'b';
		hexEncodingTable[12] = 'c';
		hexEncodingTable[13] = 'd';
		hexEncodingTable[14] = 'e';
		hexEncodingTable[15] = 'f';
	}
};

class webBase64EncodingTableInit {
	public:
	webBase64EncodingTableInit () noexcept
	{
		webBase64EncodingTable[0] = 'A';
		webBase64EncodingTable[1] = 'B';
		webBase64EncodingTable[2] = 'C';
		webBase64EncodingTable[3] = 'D';
		webBase64EncodingTable[4] = 'E';
		webBase64EncodingTable[5] = 'F';
		webBase64EncodingTable[6] = 'G';
		webBase64EncodingTable[7] = 'H';
		webBase64EncodingTable[8] = 'I';
		webBase64EncodingTable[9] = 'J';
		webBase64EncodingTable[10] = 'K';
		webBase64EncodingTable[11] = 'L';
		webBase64EncodingTable[12] = 'M';
		webBase64EncodingTable[13] = 'N';
		webBase64EncodingTable[14] = 'O';
		webBase64EncodingTable[15] = 'P';
		webBase64EncodingTable[16] = 'Q';
		webBase64EncodingTable[17] = 'R';
		webBase64EncodingTable[18] = 'S';
		webBase64EncodingTable[19] = 'T';
		webBase64EncodingTable[20] = 'U';
		webBase64EncodingTable[21] = 'V';
		webBase64EncodingTable[22] = 'W';
		webBase64EncodingTable[23] = 'X';
		webBase64EncodingTable[24] = 'Y';
		webBase64EncodingTable[25] = 'Z';
		webBase64EncodingTable[26] = 'a';
		webBase64EncodingTable[27] = 'b';
		webBase64EncodingTable[28] = 'c';
		webBase64EncodingTable[29] = 'd';
		webBase64EncodingTable[30] = 'e';
		webBase64EncodingTable[31] = 'f';
		webBase64EncodingTable[32] = 'g';
		webBase64EncodingTable[33] = 'h';
		webBase64EncodingTable[34] = 'i';
		webBase64EncodingTable[35] = 'j';
		webBase64EncodingTable[36] = 'k';
		webBase64EncodingTable[37] = 'l';
		webBase64EncodingTable[38] = 'm';
		webBase64EncodingTable[39] = 'n';
		webBase64EncodingTable[40] = 'o';
		webBase64EncodingTable[41] = 'p';
		webBase64EncodingTable[42] = 'q';
		webBase64EncodingTable[43] = 'r';
		webBase64EncodingTable[44] = 's';
		webBase64EncodingTable[45] = 't';
		webBase64EncodingTable[46] = 'u';
		webBase64EncodingTable[47] = 'v';
		webBase64EncodingTable[48] = 'w';
		webBase64EncodingTable[49] = 'x';
		webBase64EncodingTable[50] = 'y';
		webBase64EncodingTable[51] = 'z';
		webBase64EncodingTable[52] = '0';
		webBase64EncodingTable[53] = '1';
		webBase64EncodingTable[54] = '2';
		webBase64EncodingTable[55] = '3';
		webBase64EncodingTable[56] = '4';
		webBase64EncodingTable[57] = '5';
		webBase64EncodingTable[58] = '6';
		webBase64EncodingTable[59] = '7';
		webBase64EncodingTable[60] = '8';
		webBase64EncodingTable[61] = '9';
		webBase64EncodingTable[62] = '+';
		webBase64EncodingTable[63] = '/';
	}
};

class caseInsenstiveTableInit {
	public:
	caseInsenstiveTableInit () noexcept
	{
		size_t	loop;
		for ( loop = 0; loop < sizeof ( caseInsenstiveTable ); loop++ )
		{
			caseInsenstiveTable[loop] = (uint8_t)loop;
		}
		for ( loop = 'a'; loop <= 'z'; loop++ )
		{
			caseInsenstiveTable[loop] = (uint8_t)(loop - 'a' + 'A');
		}
	}
};


