/*
   DOC ME HERE ASWELL!.
   version 0.1, december, 2012

   Copyright (C) 2012- Fredrik Kihlander

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.

   Fredrik Kihlander
*/

#include <utf8_lookup/utf8_lookup.h>

#include "memtracked_unordered_map.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <map>

#if defined(__GNUC__)
	inline uint64_t cpu_tick()
	{
		timespec start;
		clock_gettime( CLOCK_MONOTONIC, &start );

		return (uint64_t)start.tv_sec * (uint64_t)1000000000 + (uint64_t)start.tv_nsec;
	}

	inline uint64_t cpu_freq() { return 1000000000; }
#elif defined(_MSC_VER)
    #include <windows.h>

	inline uint64_t cpu_tick()
	{
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return (uint64_t)t.QuadPart;
	}

	inline uint64_t cpu_freq()
	{
		static uint64_t freq = 0;
		if ( freq == 0 )
		{
			LARGE_INTEGER t;
			QueryPerformanceFrequency(&t);
			freq = (uint64_t) t.QuadPart;
		}
		return freq;
	}
#else
	#error Unknown compiler
#endif

inline float cpu_ticks_to_ms ( uint64_t ticks ) { return (float)ticks / (float)(cpu_freq() / 1000 ); }
inline float cpu_ticks_to_sec( uint64_t ticks ) { return cpu_ticks_to_ms(ticks) * ( 1.0f / 1000.0f ); }

const uint8_t* TEST_STRING = (const uint8_t*)"!\"#$%&\'()*+,-./0123456789:;?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]"
										     "^_`abcdefghijklmnopqrstuvwxyz{|}~\xc2\xa1\xc2\xa2\xc2\xa3\xc2"
										     "\xa4\xc2\xa5\xc2\xa6\xc2\xa7\xc2\xa8\xc2\xa9\xc2\xaa\xc2\xab"
										     "\xc2\xac\xc2\xad\xc2\xae\xc2\xaf\xc2\xb0\xc2\xb1\xc2\xb2\xc2"
										     "\xb3\xc2\xb4\xc2\xb5\xc2\xb6\xc2\xb7\xc2\xb8\xc2\xb9\xc2\xba"
									         "\xc2\xbb\xc2\xbc\xc2\xbd\xc2\xbe\xc2\xbf\xc3\x80\xc3\x81\xc3"
									         "\x82\xc3\x83\xc3\x84\xc3\x85\xc3\x86\xc3\x87\xc3\x88\xc3\x89"
									         "\xc3\x8a\xc3\x8b\xc3\x8c\xc3\x8d\xc3\x8e\xc3\x8f\xc3\x90\xc3"
									         "\x91\xc3\x92\xc3\x93\xc3\x94\xc3\x95\xc3\x96\xc3\x97\xc3\x98"
									         "\xc3\x99\xc3\x9a\xc3\x9b\xc3\x9c\xc3\x9d\xc3\x9e\xc3\x9f\xc3"
									         "\xa0\xc3\xa1\xc3\xa2\xc3\xa3\xc3\xa4\xc3\xa5\xc3\xa6\xc3\xa7"
									         "\xc3\xa8\xc3\xa9\xc3\xaa\xc3\xab\xc3\xac\xc3\xad\xc3\xae\xc3"
									         "\xaf\xc3\xb0\xc3\xb1\xc3\xb2\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xb6"
									         "\xc3\xb7\xc3\xb8\xc3\xb9\xc3\xba\xc3\xbb\xc3\xbc\xc3\xbd\xc3"
									         "\xbe\xc3\xbf\n";

unsigned int chars[] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
						 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
						 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
						 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 161, 162, 163, 164, 165, 166, 167, 168, 169,
						 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
						 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
						 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235,
						 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255 };

tracked_unordered_map g_compare_map;

#define ARRAY_LENGTH( arr ) ( sizeof( arr )/sizeof( arr[0] ) )

/**
 * return utf8 value of first char in str and increases str to after that char
 */
static unsigned int utf8_to_unicode_codepoint( const uint8_t** str )
{
	// TODO: Optimize me!

	// read one char from string and return the unicode codepoint
	const uint8_t* fb = *str;

	if( fb[0] <= 128 )
	{
		*str = fb + 1;
		return fb[0];
	}
	else if( fb[0] <= 0xDF )
	{
		*str = fb + 2;
		return ( fb[1] & 0x3F ) | ( ( fb[0] & 0x1F ) << 6 );
	}
	else if( fb[0] <= 0xEF )
	{
		*str = fb + 3;
		return ( fb[2] & 0x3F ) | ( ( fb[1] & 0x3F ) << 6 ) | ( ( fb[0] & 0xF )  << 12 );
	}
	else if ( fb[0] <= 0xF7 )
	{
		*str = fb + 4;
		return ( fb[3] & 0x3F ) | ( ( fb[2] & 0x3F ) << 6 ) | ( ( fb[1] & 0x3F ) << 12 ) | ( ( fb[0] & 0x7 ) << 18 );
	}

	return 0;
}

void build_compare_map()
{
	for( unsigned int i = 0; i < ARRAY_LENGTH( chars ); ++i )
		g_compare_map[ chars[i] ] = i;
}

utf8_lookup_error utf8_lookup_perform_std_cmp( const uint8_t* str, const uint8_t** str_left, utf8_lookup_result* res, size_t* res_size )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

	while( *pos && res_out != res_end )
	{
		int code_point = utf8_to_unicode_codepoint( &pos );

		tracked_unordered_map::iterator it = g_compare_map.find( code_point );
		res_out->offset = it == g_compare_map.end() ? 0 : it->second;
		++res_out;
	}

	*res_size = (int)(res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}

int main( int argc, char** argv )
{
	(void)argc; (void)argv;

	build_compare_map();

	// build lookup map
	size_t size;
	utf8_lookup_calc_table_size( &size, chars, ARRAY_LENGTH(chars) );
	void* table = malloc( size );
	utf8_lookup_gen_table( table, size, chars, ARRAY_LENGTH(chars) );

	printf( "unordered_map - allocs: %lu  mem: %f kb\n", my_lib::g_num_alloc, (float)my_lib::g_num_bytes/1024.0f);
	printf( "utf8_lookup   - allocs: 1    mem: %lu byte\n", size );

	const uint8_t* str_iter = TEST_STRING;
	utf8_lookup_result res1[256];
	size_t res_size = 256;

	{
		uint64_t start = cpu_tick();

		for( int i = 0; i < 100000; ++i )
			utf8_lookup_perform_std_cmp( TEST_STRING, &str_iter, res1, &res_size );

		printf( "unordered_map time: %f sec\n", cpu_ticks_to_sec( cpu_tick() - start ) );
	}


	str_iter = TEST_STRING;
	utf8_lookup_result res2[256];

	res_size = 256;

	{
		uint64_t start = cpu_tick();

		for( int i = 0; i < 100000; ++i )
			utf8_lookup_perform( table, TEST_STRING, &str_iter, res2, &res_size );

		printf( "utf8_lookup time:   %f sec\n", cpu_ticks_to_sec( cpu_tick() - start ) );
	}

	free( table );
	return 0;
}
