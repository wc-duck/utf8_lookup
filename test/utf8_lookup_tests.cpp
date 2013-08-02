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

#include "greatest.h"
#include <utf8_lookup/utf8_lookup.h>

#define ARRAY_LENGTH( arr ) ( sizeof( arr )/sizeof( arr[0] ) )

void print_as_bf( uint64_t value )
{
	for( int i = 0; i < 64; ++i )
	{
		if( ( i ) % 10 == 0 )
			printf( " " );
		printf("%d", (i) / 10 );
	}

	printf( "\n" );

	for( uint64_t i = 0; i < 64; ++i )
	{
		if( ( i ) % 10 == 0 )
			printf( " " );
		printf("%c", ( ( value >> i ) & 1 ) ? '1' : '0' );
	}

	printf( "\n" );
}

struct test_me
{
	uint64_t bf;
	uint32_t offset;
};

size_t pack_table( uint8_t* table, size_t tab_size, unsigned int* cps, unsigned int num_cps )
{
	size_t size;
	utf8_lookup_calc_table_size( &size, cps, num_cps );

	ASSERT( tab_size > size ); // safety-check
	memset( table, 0xFE, tab_size );

	utf8_lookup_error err = utf8_lookup_gen_table( table, size, cps, num_cps );
	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, err );
	ASSERT_EQ( 0xFE, table[ size ] );
	ASSERT_EQ(  0x0, table[ size - 1 ] );

	return size;
}

TEST octet_1_simple()
{
	unsigned int test_cps[] = { '\n', '%' };

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"%d";
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 2u, res_size );

	ASSERT_EQ( 2u, res[0].offset );
	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[1].offset );

	return 0;
}

TEST octet_1()
{
	unsigned int test_cps[] = { '%', '6', 'B', 'X', 'a', 'b' };

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"BX%6abd";
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 7u, res_size );

	ASSERT_EQ( 3u, res[0].offset );
	ASSERT_EQ( 4u, res[1].offset );
	ASSERT_EQ( 1u, res[2].offset );
	ASSERT_EQ( 2u, res[3].offset );
	ASSERT_EQ( 5u, res[4].offset );
	ASSERT_EQ( 6u, res[5].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[6].offset );

	return 0;
}

TEST octet_1_complete()
{
	unsigned int* test_cps = (unsigned int*)malloc( 128 * sizeof(unsigned int) );
	for( int i = 0; i < 127; ++i )
		test_cps[i] = i + 1;

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, 127 );

	uint8_t* str = (uint8_t*)malloc( 128 );
	for( int i = 0; i < 128; ++i )
		str[i] = (uint8_t)(127 - i);

	utf8_lookup_result res[256];
	size_t res_size = ARRAY_LENGTH( res );
	const uint8_t* str_iter = str;

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 127u, res_size );

	for( unsigned int i = 0; i < 127; ++i )
		ASSERT_EQ( i, 127 - res[i].offset );

	const uint8_t* missing_str = (const uint8_t*)"\xc3\xa5"   // å
												 "\xc3\xa4"   // ä
												 "\xc3\xb6"   // ö
												 "\xc2\xa5";  // ¥;
	const uint8_t* str_iter2 = missing_str;
	// lookup non-existing!
	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, missing_str, &str_iter2, res, &res_size ) );
	ASSERT_EQ( 4u, res_size );

	ASSERT_EQ( 0u, res[0].offset );
	ASSERT_EQ( 0u, res[1].offset );
	ASSERT_EQ( 0u, res[2].offset );
	ASSERT_EQ( 0u, res[3].offset );

	free( str );
	free( test_cps );

	return 0;
}

TEST octet_2()
{
	// TODO: add chars from all pages
	unsigned int test_cps[] = { 130,     // offset 1 = (130)
								165,     // offset 2 = ¥
								228,     // offset 3 = ä
								229,     // offset 4 = å
								246,     // offset 5 = ö
								0x7CF,   // offset 6
								0x7FF }; // offset 7 = max octet 2

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xc3\xa5"  // å
									     "\xc3\xa4"  // ä
									     "\xc3\xb6"  // ö
									     "\xc2\xa5"  // ¥
									     "\xdf\x8f"
									     "\xdf\xbf"  // max octet 2
									     "\xc2\xa7"; // § ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 7u, res_size );

	ASSERT_EQ( 4u, res[0].offset );
	ASSERT_EQ( 3u, res[1].offset );
	ASSERT_EQ( 5u, res[2].offset );
	ASSERT_EQ( 2u, res[3].offset );
	ASSERT_EQ( 6u, res[4].offset );
	ASSERT_EQ( 7u, res[5].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[6].offset );

	return 0;
}

TEST octet_3()
{
	unsigned int test_cps[] = { 0x800, 0x1024, 0x1025, 0xFFFF };
	uint8_t table[ 256 * 2 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xe0\xa0\x80"  // 0x800
									     "\xe1\x80\xa4"  // 0x1024
									     "\xe1\x80\xa5"  // 0x1025
									     "\xef\xbf\xbf"  // 0xFFFF
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 5u, res_size );

	ASSERT_EQ( 1u, res[0].offset );
	ASSERT_EQ( 2u, res[1].offset );
	ASSERT_EQ( 3u, res[2].offset );
	ASSERT_EQ( 4u, res[3].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[4].offset );

	return 0;
}

TEST octet_3_bug()
{
	unsigned int test_cps[] = { 0x2026, 0x300F, 0x7B2C };

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xe2\x80\xa6"  // 0x2026
										 "\xe3\x80\x8f"  // 0x300F
										 "\xe7\xac\xac"  // 0x7B2C
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 4u, res_size );

	ASSERT_EQ( 1u, res[0].offset );
	ASSERT_EQ( 2u, res[1].offset );
	ASSERT_EQ( 3u, res[2].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[3].offset );

	return 0;
}

TEST octet_3_bug_2()
{
	unsigned int test_cps[] = { 0xA0, 0x2026 };

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xc2\xa0"
									     "\xe2\x80\xa6"  // 0x2026
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 3u, res_size );

	ASSERT_EQ( 1u, res[0].offset );
	ASSERT_EQ( 2u, res[1].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[2].offset );

	return 0;
}

TEST octet_4()
{
	unsigned int test_cps[] = { 0x10000, // octet 4 min
								0x10801,
								0x10802,
								0x10FFFF }; // octet 4 max

	uint8_t table[ 512 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xf0\x90\x80\x80"  // 0x10000
									     "\xf0\x90\xa0\x81"  // 0x10801
									     "\xf0\x90\xa0\x82"  // 0x10802
									     "\xf4\x8f\xbf\xbf"  // 0x10FFF
									     "\xf0\x90\xa0\x83"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 5u, res_size );

	ASSERT_EQ( 1u, res[0].offset );
	ASSERT_EQ( 2u, res[1].offset );
	ASSERT_EQ( 3u, res[2].offset );
	ASSERT_EQ( 4u, res[3].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[4].offset );

	return 0;
}


TEST octet_1_and_2()
{
	// TODO: add chars from all pages
	unsigned int test_cps[] = { 'a', 228 }; // aä

	uint8_t table[ 256 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"a" "\xc3\xa4";  // aä
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 2u, res_size );

	ASSERT_EQ( 1u, res[0].offset );
	ASSERT_EQ( 2u, res[1].offset );

	return 0;
}


TEST octet_1_and_3()
{
	unsigned int test_cps[] = { '%', '6', 'B', 'X', 'a', 'b', 0x800, 0x1024, 0x1025, 0xFFFF };

	uint8_t table[ 256 * 2 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"BaXb6%"
									     "\xe0\xa0\x80"  // 0x800
									     "\xe1\x80\xa4"  // 0x1024
									     "\xe1\x80\xa5"  // 0x1025
									     "\xef\xbf\xbf"  // 0xFFFF
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 11u, res_size );

	ASSERT_EQ(  3u, res[0].offset );
	ASSERT_EQ(  5u, res[1].offset );
	ASSERT_EQ(  4u, res[2].offset );
	ASSERT_EQ(  6u, res[3].offset );
	ASSERT_EQ(  2u, res[4].offset );
	ASSERT_EQ(  1u, res[5].offset );
	ASSERT_EQ(  7u, res[6].offset );
	ASSERT_EQ(  8u, res[7].offset );
	ASSERT_EQ(  9u, res[8].offset );
	ASSERT_EQ( 10u, res[9].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[10].offset );

	return 0;
}


TEST octet_2_and_3()
{
	unsigned int test_cps[] = { 130,     // offset 1 = (130)
								165,     // offset 2 = ¥
								228,     // offset 3 = ä
								229,     // offset 4 = å
								246,     // offset 5 = ö
								0x7CF,   // offset 6
								0x7FF,   // offset 7 = max octet 2

								// octet 3
								0x800,
								0x1024,
								0x1025,
								0xFFFF };

	uint8_t table[ 512 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"\xc3\xa5"  // å
									     "\xc3\xa4"  // ä
									     "\xc3\xb6"  // ö
									     "\xc2\xa5"  // ¥
									     "\xdf\x8f"
									     "\xdf\xbf"  // max octet 2
									     "\xc2\xa7"  // § ... do not exist in table ...
									     "\xe0\xa0\x80"  // 0x800
									     "\xe1\x80\xa4"  // 0x1024
									     "\xe1\x80\xa5"  // 0x1025
									     "\xef\xbf\xbf"  // 0xFFFF
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 12u, res_size );

	ASSERT_EQ(  4u, res[0].offset );
	ASSERT_EQ(  3u, res[1].offset );
	ASSERT_EQ(  5u, res[2].offset );
	ASSERT_EQ(  2u, res[3].offset );
	ASSERT_EQ(  6u, res[4].offset );
	ASSERT_EQ(  7u, res[5].offset );
	ASSERT_EQ(  0u, res[6].offset ); // do not exist
	ASSERT_EQ(  8u, res[7].offset );
	ASSERT_EQ(  9u, res[8].offset );
	ASSERT_EQ( 10u, res[9].offset );
	ASSERT_EQ( 11u, res[10].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[11].offset );

	return 0;
}

TEST octet_1_2_and_3()
{
	unsigned int test_cps[] = { 'A', 'B', 'b', 'd',
								130,     // offset 5 = (130)
								165,     // offset 6 = ¥
								228,     // offset 7 = ä
								229,     // offset 8 = å
								246,     // offset 9 = ö
								0x7CF,   // offset 10
								0x7FF,   // offset 11 = max octet 2

								// octet 3
								0x800,
								0x1024,
								0x1025,
								0xFFFF };

	uint8_t table[ 512 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"dbBA"
									     "q"         // q ... do not exist in table ...
									     "\xc3\xa5"  // å
									     "\xc3\xa4"  // ä
									     "\xc3\xb6"  // ö
									     "\xc2\xa5"  // ¥
									     "\xdf\x8f"
									     "\xdf\xbf"  // max octet 2
									     "\xc2\xa7"  // § ... do not exist in table ...
									     "\xe0\xa0\x80"  // 0x800
									     "\xe1\x80\xa4"  // 0x1024
									     "\xe1\x80\xa5"  // 0x1025
									     "\xef\xbf\xbf"  // 0xFFFF
									     "\xe2\x81\x88"; // ... do not exist in table ...
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 17u, res_size );

	ASSERT_EQ(  4u, res[0].offset );
	ASSERT_EQ(  3u, res[1].offset );
	ASSERT_EQ(  2u, res[2].offset );
	ASSERT_EQ(  1u, res[3].offset );
	ASSERT_EQ(  0u, res[4].offset );

	ASSERT_EQ(  8u, res[5].offset );
	ASSERT_EQ(  7u, res[6].offset );
	ASSERT_EQ(  9u, res[7].offset );
	ASSERT_EQ(  6u, res[8].offset );
	ASSERT_EQ( 10u, res[9].offset );
	ASSERT_EQ( 11u, res[10].offset );
	ASSERT_EQ(  0u, res[11].offset ); // do not exist

	ASSERT_EQ( 12u, res[12].offset );
	ASSERT_EQ( 13u, res[13].offset );
	ASSERT_EQ( 14u, res[14].offset );
	ASSERT_EQ( 15u, res[15].offset );

	// ... d should return 0 since it do not exist in the lookup table...
	ASSERT_EQ( 0u, res[16].offset );

	return 0;
}

TEST octet_1_2_3_and_4()
{
	unsigned int test_cps[] = { 'A', 'B', 'b', 'd',
								130,     // offset 5 = (130)
								165,     // offset 6 = ¥
								228,     // offset 7 = ä
								229,     // offset 8 = å
								246,     // offset 9 = ö
								0x7CF,   // offset 10
								0x7FF,   // offset 11 = max octet 2

								// octet 3
								0x800,
								0x1024,
								0x1025,
								0xFFFF,

								 // octet 4
								0x10000,
								0x10801,
								0x10802,
								0x10FFFF };

	uint8_t table[ 512 ];
	pack_table( table, sizeof(table), test_cps, ARRAY_LENGTH(test_cps) );

	const uint8_t* str = (const uint8_t*)"dbBA"
									     "q"         // q ... do not exist in table ...
									     "\xc3\xa5"  // å
									     "\xc3\xa4"  // ä
									     "\xc3\xb6"  // ö
									     "\xc2\xa5"  // ¥
									     "\xdf\x8f"
									     "\xdf\xbf"  // max octet 2
									     "\xc2\xa7"  // § ... do not exist in table ...
									     "\xe0\xa0\x80"  // 0x800
									     "\xe1\x80\xa4"  // 0x1024
									     "\xe1\x80\xa5"  // 0x1025
									     "\xef\xbf\xbf"  // 0xFFFF
									     "\xe2\x81\x88"  // ... do not exist in table ...
										 "\xf0\x90\x80\x80"  // 0x10000
										 "\xf0\x90\xa0\x81"  // 0x10801
										 "\xf0\x90\xa0\x82"  // 0x10802
										 "\xf4\x8f\xbf\xbf"  // 0x10FFF
										 "\xf0\x90\xa0\x83";
	const uint8_t* str_iter = str;

	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	ASSERT_EQ( UTF8_LOOKUP_ERROR_OK, utf8_lookup_perform( table, str, &str_iter, res, &res_size ) );
	ASSERT_EQ( 22u, res_size );

	ASSERT_EQ(  4u, res[0].offset );
	ASSERT_EQ(  3u, res[1].offset );
	ASSERT_EQ(  2u, res[2].offset );
	ASSERT_EQ(  1u, res[3].offset );
	ASSERT_EQ(  0u, res[4].offset ); // do not exist

	ASSERT_EQ(  8u, res[5].offset );
	ASSERT_EQ(  7u, res[6].offset );
	ASSERT_EQ(  9u, res[7].offset );
	ASSERT_EQ(  6u, res[8].offset );
	ASSERT_EQ( 10u, res[9].offset );
	ASSERT_EQ( 11u, res[10].offset );
	ASSERT_EQ(  0u, res[11].offset ); // do not exist

	ASSERT_EQ( 12u, res[12].offset );
	ASSERT_EQ( 13u, res[13].offset );
	ASSERT_EQ( 14u, res[14].offset );
	ASSERT_EQ( 15u, res[15].offset );
	ASSERT_EQ( 0u,  res[16].offset ); // do not exist

	ASSERT_EQ( 16u, res[17].offset );
	ASSERT_EQ( 17u, res[18].offset );
	ASSERT_EQ( 18u, res[19].offset );
	ASSERT_EQ( 19u, res[20].offset );
	ASSERT_EQ( 0u,  res[21].offset ); // do not exist

	return 0;
}

GREATEST_SUITE( utf8_lookup )
{
	RUN_TEST( octet_1_simple );
	RUN_TEST( octet_1 );
	RUN_TEST( octet_1_complete );
	RUN_TEST( octet_2 );
	RUN_TEST( octet_3 );
	RUN_TEST( octet_3_bug );
	RUN_TEST( octet_3_bug_2 );
	RUN_TEST( octet_4 );
	RUN_TEST( octet_1_and_2 );
	RUN_TEST( octet_1_and_3 );
	RUN_TEST( octet_2_and_3 );
	RUN_TEST( octet_1_2_and_3 );
	RUN_TEST( octet_1_2_3_and_4 );
}

GREATEST_MAIN_DEFS();

int main( int argc, char **argv )
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE( utf8_lookup );
    GREATEST_MAIN_END();
}
