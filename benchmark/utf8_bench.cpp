/*
   A small drop-in library for converting utf8-strings into offsets into a table.
   Inspired by the HAMT-data structure and designed to be used to translate string
   into a list of glyphs for rendering.

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
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <iterator>

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

utf8_lookup_error utf8_lookup_perform_std_cmp( tracked_unordered_map& compare_map, const uint8_t* str, const uint8_t** str_left, utf8_lookup_result* res, size_t* res_size )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

	while( *pos && res_out != res_end )
	{
		res_out->pos    = pos;
		int code_point = utf8_to_unicode_codepoint( &pos );

		tracked_unordered_map::iterator it = compare_map.find( code_point );
		res_out->offset = it == compare_map.end() ? 0 : it->second;
		++res_out;
	}

	*res_size = (int)(res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}

static uint8_t* load_file( const char* file_name, size_t* file_size )
{
	FILE* f = fopen( file_name, "rb" );
	if( f == 0x0 )
		return 0x0;

	fseek( f, 0, SEEK_END );
	*file_size = ftell( f );
	fseek( f, 0, SEEK_SET );

	uint8_t* text = (uint8_t*)malloc( *file_size + 1 );
	size_t r = fread( text, 1, *file_size, f );
	text[ *file_size ] = '\0';
	(void)r;
	fclose( f );
	return text;
}

static int codepoint_to_octet( unsigned int cp )
{
	if( cp <= 0x7F ) return 0;
	if( cp <= 0x7FF ) return 1;
	if( cp <= 0xFFFF ) return 2;
	if( cp <= 0x10FFFF ) return 3;

	return 0;
}

static void find_all_codepoints( const uint8_t* text, std::vector<unsigned int>& cps )
{
	unsigned int count[4] = { 0, 0, 0, 0 };
	const uint8_t* iter = text;
	std::set<unsigned int> s;

	while( *iter )
		s.insert( utf8_to_unicode_codepoint( &iter ) );

	cps.reserve( s.size() );
	std::copy( s.begin(), s.end(), std::back_inserter(cps) );
	std::sort( cps.begin(), cps.end() );

	printf( "num codepoints in file %lu\n", cps.size() );

	for( size_t i = 0; i < cps.size(); ++i )
		++count[ codepoint_to_octet( cps[i] ) ];

	printf( "octet 1: %u\n", count[0] );
	printf( "octet 2: %u\n", count[1] );
	printf( "octet 3: %u\n", count[2] );
	printf( "octet 4: %u\n", count[3] );
}

void build_compare_map( std::vector<unsigned int>& cps, tracked_unordered_map& output )
{
	for( size_t i = 0; i < cps.size(); ++i )
		output[ cps[i] ] = (unsigned int)i + 1;

	printf( "unordered_map - allocs: %lu  mem: %f kb\n", my_lib::g_num_alloc, (float)my_lib::g_num_bytes/1024.0f );
}

void* build_utf8_lookup_map( std::vector<unsigned int>& cps )
{
	size_t size;
	utf8_lookup_calc_table_size( &size, &cps[0], (unsigned int)cps.size() );
	void* table = malloc( size );
	utf8_lookup_gen_table( table, size, &cps[0], (unsigned int)cps.size() );

	printf( "utf8_lookup   - allocs: 1    mem: %f kb\n", (float)size/1024.0f );

	return table;
}

int main( int argc, char** argv )
{
	if( argc < 1 )
	{
		printf( "bad input parameters!\n" );
		return 1;
	}

	size_t file_size;
	uint8_t* text_data = load_file( argv[1], &file_size );
	if( text_data == 0x0 )
	{
		printf( "couldn't load file %s\n", argv[1] );
		return 1;
	}

	uint8_t* text = text_data;
	// skip BOM
	if( text_data[0] == 0xEF && text_data[1] == 0xBB && text_data[2] == 0xBF )
	{
		printf("has bom!\n");
		text = &text_data[3];
	}

	std::vector<unsigned int> cps;
	find_all_codepoints( text, cps );

	tracked_unordered_map compare_map;
	build_compare_map( cps, compare_map );

	// build lookup map
	void* table = build_utf8_lookup_map( cps );


	{
		utf8_lookup_result res1[256];
		size_t res_size = 256;

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform_std_cmp( compare_map, str_iter, &str_iter, res1, &res_size );
		}

		float time = cpu_ticks_to_sec( cpu_tick() - start );
		printf( "unordered_map time: %f sec, %f GB/sec\n", time, ( (float)file_size / ( 1024 * 1024 ) ) / time );
	}

	{
		utf8_lookup_result res2[256];
		size_t res_size = 256;

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform( table, str_iter, &str_iter, res2, &res_size );
		}

		float time = cpu_ticks_to_sec( cpu_tick() - start );
		printf( "utf8_lookup time:   %f sec, %f GB/sec\n", time, ( (float)file_size / ( 1024 * 1024 ) ) / time );
	}

	// check output!
	{
		utf8_lookup_result res1[256];
		utf8_lookup_result res2[256];
		size_t res_size1 = 256;
		size_t res_size2 = 256;

		memset( res1, 0x0, sizeof(res1) );
		memset( res2, 0x0, sizeof(res2) );

		const uint8_t* str_iter1 = text;
		const uint8_t* str_iter2 = text;

		unsigned int chars = 0;
		unsigned int num_missmatch = 0;

		int loop = 0;
		bool print = true;
		while( *str_iter1 && *str_iter2 )
		{
			utf8_lookup_perform_std_cmp( compare_map, str_iter1, &str_iter1, res1, &res_size1 );
			utf8_lookup_perform( table, str_iter2, &str_iter2, res2, &res_size2 );

			if( str_iter1 != str_iter2 )
			{
				printf("mismatching str_iter!\n");
				break;
			}

			if( res_size1 != res_size2 )
			{
				printf("mismatching res_size!\n");
				break;
			}

			for( size_t i = 0; i < res_size1; ++i )
			{
				++chars;

				if( res1[i].pos != res2[i].pos )
				{
					if( print )
						printf("%d[%lu] = pos mismatch! %p %p\n", loop, i, res1[i].pos, res2[i].pos);
				}

				if( res1[i].offset != res2[i].offset )
				{
					if( print )
					{
						printf("%d[%lu] = %u %u\n", loop, i, res1[i].offset, res2[i].offset );
						char test[4] = { res1[i].pos[0], res1[i].pos[1], res1[i].pos[2], '\0' };
						printf("char %s\n", test);
						print = false;
					}
					++num_missmatch;
				}
			}

			++loop;
		}

		if( num_missmatch != 0 )
		{
			printf("mismatching res! %u of %u\n", num_missmatch, chars);
		}
	}

	free( text_data );
	free( table );
	return 0;
}
