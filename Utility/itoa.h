/*
	buffer defines

*/

#pragma once

#include "Utility/settings.h"

#include <stdint.h>
#include <memory.h>

template <typename T> 
T reduce2(T v) {
	/* pre: ((short*)&v)[i] < 100 for all i
		*  post: 
		*     ((char*)&v)[2i] = ((short*)&v)[i] / 10
		*     ((char*)&v)[2i + 1] = ((short*)&v)[i] % 10
		*     
		*     That is, we split each short in v into its ones and tens digits
		*/

	/* t < 100 --> (t * 410) >> 12 == t / 10
		*			&& (t * 410) < 0x10000
		* 
		* For the least order short that's all we need, for the others the
		* shift doesn't drop the remainder so we mask those out
		*/
	T k = ((v * 410) >> 12) & 0x000F000F000F000Full;

	/*
		* Then just subtract out the tens digit to get the ones digit and
		* shift them into the right place
		*/
	return (((v - k * 10) << 8) + k);
}

template <typename T>
T reduce4(T v) {
	/* pre: ((unsigned*)&v)[i] < 10000 for all i
		*
		*  preReduce2: 
		*     ((short*)&v)[2i] = ((unsigned*)&v)[i] / 100
		*     ((short*)&v)[2i + 1] = ((unsigned*)&v)[i] % 100
		*     
		*     That is, we split each int in v into its one/ten and hundred/thousand 
		*     digit pairs. Put them into the corresponding short positions and then
		*     call reduce2 to finish the splitting
		*/

	/* This is basically the same as reduce2 with different constants
		*/
	T k = ((v * 10486) >> 20) & 0xFF000000FFull;
	return reduce2(((v - k * 100) << 16) + (k));
}

typedef unsigned long long ull;
inline ull reduce8(ull v) {
	/* pre: v  < 100000000
		*
		*  preReduce4: 
		*     ((unsigned*)&v)[0] = v / 10000
		*     ((unsigned*)&v)[1] = v % 10000
		*
		*     This should be familiar now, split v into the two 4-digit segments,
		*     put them in the right place, and let reduce4 continue the splitting
		*/

	/* Again, use the same method as reduce4 and reduce2 with correctly tailored constants
		*/
	ull k = ((v * 3518437209u) >> 45);
	return reduce4(((v - k * 10000) << 32) + (k));
}

template <typename T>
size_t itostr(T o, uint8_t *buff) {
	/*
		* Use of this union is not strictly compliant, but, really,
		* who cares? This is just for fun :)
		*
		* Our ones digit will be in str[15]
		*
		* We don't actually need the first 6 bytes, but w/e
		*/
	union {
		char str[16];
		unsigned short u2[8];
		unsigned u4[4];
		unsigned long long u8[2];
	};

	/* Technically should be "... ? unsigned(~0) + 1 ..." to ensure correct behavior I think */
	/* Tends to compile to: v = (o ^ (o >> 31)) - (o >> 31); */
	unsigned v = o < 0 ? ~o + 1 : o;

	/* We want u2[3] = v / 100000000 ... that is, the first 2 bytes of the decimal rep */

	/* This is the same as in reduce8, that is divide by 10000. So u8[0] = v / 10000 */
	u8[0] = (ull(v) * 3518437209u) >> 45;

	/* Now we want u2[3] = u8[0] / 10000.
		* If we added " >> 48 " to the end of the calculation below we would get u8[0] = u8[0] / 10000
		* Note though that in little endian byte ordering u2[3] is the top 2 bytes of u8[0]
		* and 64 - 16 == 48... Aha, We've got what we want, the rest of u8[0] is junk, but we don't care
		*/
	u8[0] = (u8[0] * 28147497672ull);

	/* Then just subtract out those digits from v so that u8[1] now holds
		* the low 8 decimal digits of v
		*/
	u8[1] = v - u2[3] * 100000000;

	/* Split u8[1] into its 8 decimal digits */
	u8[1] = reduce8(u8[1]);

	/* f will point to the high order non-zero char */
	char* f;

	/* branch post: f is at the correct short (but not necessarily the correct byte) */
	if (u2[3]) {
		/* split the top two digits into their respective chars */
		u2[3] = reduce2(u2[3]);
		f = str + 6;
	} else {
		/* a sort of binary search on first non-zero digit */
		unsigned short* k = u4[2] ? u2 + 4 : u2 + 6;
		f = *k ? (char*)k : (char*)(k + 1);
	}
	/* update f to its final position */
	if (!*f) f++;

	/* '0' == 0x30 and i < 10 --> i <= 0xF ... that is, i | 0x30 = 'i' *
		* Note that we could do u8[0] |= ... u8[1] |= ... but the corresponding
		* x86-64 operation cannot use a 64 bit immediate value whereas the
		* 32 bit 'or' can use a 32 bit immediate.
		*/
	u4[1] |= 0x30303030;
	u4[2] |= 0x30303030;
	u4[3] |= 0x30303030;

	/* Add the negative sign... note that o is just the original parameter passed */
	if (o < 0) *--f = '-';

	/* gcc basically forwards this to std::string(f, str + 16)
		* but msvc handles it way more efficiently
		*/
	memcpy ( buff, f, 16 - (f - str) );
	return 16 - (f - str);
}

