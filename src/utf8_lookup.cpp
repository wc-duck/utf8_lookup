/*
   DOC ME HERE ASWELL!.
   version 0.1, october, 2012

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

#include <ctype.h>
#include <string.h>

/*
	range		      binary code point             utf8 bytes
	 0 - 7F           0xxxxxxx                      0xxxxxxx

	80 - 7FF          00000yyy yyxxxxxx             110yyyyy
													10xxxxxx

	800 - FFFF        zzzzyyyy yyxxxxxx             1110zzzz
													10yyyyyy
													10xxxxxx

	10000 - 10FFFF    000wwwzz zzzzyyyy yyxxxxxx    11110www
													10zzzzzz
													10yyyyyy
													10xxxxxx



	* if glyph do not exist in font, result shoud be zero ( place default glyph at zero )
	* all octets need a base do do lookup in that need to be addressable for all bytes.
		- for first octet 3 lookup elements is needed ( enough to cover all chars in octet )
		- for the other octets one 32 bit lookup is needed since none of them has more than 5 bits
		  in first byte that is valid for this byte.

	* on octet one we need to find if the char is in character-group 0, 1 or 2 and on the other octets we need
	  the code to say that they belong in sub-group 0 ( since there are only one on top-level. )

		- this is done by selecting a bitmask from octet of char. On octet one, we mask off top 3 and shift 5.
		  mask with 0 on other octets.

	* on bytes over first byte, we shift by 5 and get either 1 or 0 to index into the next lookup-element.

	* lookup-elem 0 is always 0 and 0 to be able to revert all lookups to it if any other lookup is found as no-available.
		- could use pop_count to select if available to get 0 or 1 on availability.
*/

//static inline uint32_t test( uint32_t u32 )
//{
//	u32 = u32 - ( ( u32 >> 1 ) & 0x55555555 );
//	u32 = ( u32 & 0x33333333 ) + ( ( u32 >> 2 ) & 0x33333333 );
//	return ( ( ( u32 + ( u32 >> 4 ) ) & 0x0F0F0F0F ) * 0x01010101 ) >> 24;
//}

static inline uint64_t bit_pop_cnt( uint64_t val )
{
	return __builtin_popcountll( (unsigned long long)val );
	// return test( (uint32_t)( val >> 32 ) ) | test( (uint32_t)val );
}

static inline int utf8_num_trailing_bytes( int first_byte )
{
	static const int UTF8_TRAILING_BYTES_TABLE[256] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
	};

	return UTF8_TRAILING_BYTES_TABLE[ first_byte ];

	/*
	// Second implementation, might be faster if pop_count-instruction is present.
	static const int OCTET_LOOKUP[] = { 0, 1337, 1, 2 }; // 1337 is an error ;)
	return OCTET_LOOKUP[ bit_leading_one_count( first_byte ) ];
	*/
}

static int utf8_split_to_bytes( unsigned int cp, unsigned int* bytes )
{
	if ( cp <= 127 )
	{
		bytes[0] = cp;
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0;
		return 0;
	}
	if( cp <= 0x7FF )
	{
		bytes[0] = cp >> 6;
		bytes[1] = cp & 63;
		bytes[2] = 0;
		bytes[3] = 0;
		return 1;
	}

	if( cp <= 0xFFFF )
	{
		bytes[0] = ( cp >> 12 ) & 15;
		bytes[1] = ( cp >> 6  ) & 63;
		bytes[2] = cp & 63;
		bytes[3] = 0;
		return 2;
	}

	if( cp <= 0x10FFFF )
	{
		bytes[0] = ( cp >> 18 ) & 7;
		bytes[1] = ( cp >> 12 ) & 63;
		bytes[2] = ( cp >> 6  ) & 63;
		bytes[3] = cp & 63;
		return 3;
	}

	return -1;
}

static int utf8_split_group_and_gid( unsigned int cp, uint64_t* groups, uint64_t* gids )
{
	unsigned int bytes[4];
	int octet = utf8_split_to_bytes( cp, bytes );

	for( int i = 0; i <= octet; ++i )
	{
		groups[i] = (uint64_t)(bytes[i] >> 6);
		gids[i]   = (uint64_t)(bytes[i] & 63);
	}

	return octet;
}

struct lookup_elem
{
	uint64_t avail;
	uint32_t offset;
};

static void utf8_finalize_subgroup( lookup_elem* first, lookup_elem* last, uint32_t start_offset )
{
	uint32_t offset = start_offset;
	lookup_elem* curr = first;

	while( curr != last )
	{
		curr->offset = offset;
		offset += (uint32_t)bit_pop_cnt( curr->avail );
		++curr;
	}

	curr->offset = offset;
}

static void utf8_write_start_bitfield( unsigned int* cp_start, unsigned int* cp_end, lookup_elem* elem, int octet )
{
	uint64_t group[4]; uint64_t gid[4];

	while( cp_start < cp_end && utf8_split_group_and_gid( *cp_start, group, gid ) == octet )
	{
		elem->avail |= (uint64_t)1 << gid[0];
		++cp_start;
	}
}

utf8_lookup_error utf8_lookup_gen_table( void*         table,
					 	 	 	 	 	 size_t        table_size,
					 	 	 	 	 	 unsigned int* codepoints,
					 	 	 	 	 	 unsigned int  num_codepoints )
{
	lookup_elem* first_bitfield = (lookup_elem*)table;
	lookup_elem* next_bitfield  = first_bitfield;
	next_bitfield++; // save one for 0x0

	lookup_elem* o0_first_byte = next_bitfield; next_bitfield += 3; // need 3 for ascii
	lookup_elem* o1_first_byte = next_bitfield++;
	lookup_elem* o2_first_byte = next_bitfield++;
	lookup_elem* o3_first_byte = next_bitfield++;

	uint64_t GROUP_RESET[3] = { (uint64_t)-1, (uint64_t)-1, (uint64_t)-1 };
	uint64_t last_gids[3];

	#define RESET_LAST_GIDS  memcpy( last_gids, GROUP_RESET, sizeof( last_gids ) )
	#define UPDATE_LAST_GIDS memcpy( last_gids, gid,         sizeof( last_gids ) )

	lookup_elem* o2; lookup_elem* o3; lookup_elem* o4;

	lookup_elem* o2_start = 0x0;
	lookup_elem* o3_start = 0x0;

	unsigned int written_chars = 1; // 0 is reserved as non-existing

	memset( table, 0x0, table_size );

	unsigned int i = 0;

	uint64_t group[4];
	uint64_t gid[4];

	// o1!
	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 0; ++i )
	{
		o0_first_byte[ group[0] ].avail |= (uint64_t)1 << gid[0];
		++written_chars;
	}

	// finalize o1!
	o0_first_byte[0].offset = 1;
	o0_first_byte[1].offset = o0_first_byte[0].offset + (uint32_t)bit_pop_cnt( o0_first_byte[0].avail );


	// octet 2
	RESET_LAST_GIDS;

	o1_first_byte->offset = (uint32_t)(next_bitfield - first_bitfield);
	utf8_write_start_bitfield( codepoints + i, codepoints + num_codepoints, o1_first_byte, 1 );

	o2 = next_bitfield;


	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 1; ++i )
	{
		if( last_gids[0] != gid[0] )
		{
			o2 = next_bitfield++;
			o2->offset = written_chars;
			UPDATE_LAST_GIDS;
		}

		o2->avail |= (uint64_t)1 << gid[1];

		++written_chars;
	}

	// octet 3

	RESET_LAST_GIDS;

	// build o1
	o2_first_byte->offset = (uint32_t)(next_bitfield - first_bitfield); // next
	utf8_write_start_bitfield( codepoints + i, codepoints + num_codepoints, o2_first_byte, 2 );

	o2_start = next_bitfield;
	o2       = o2_start;

	// build o1 and o2
	for( unsigned int i2 = i; i2 < num_codepoints && utf8_split_group_and_gid( codepoints[i2], group, gid ) == 2; ++i2 )
	{
		if( gid[0] != last_gids[0] )
		{
			o2 = next_bitfield++;
			UPDATE_LAST_GIDS;
		}

		o2->avail |= (uint64_t)1 << gid[1];
	}

	utf8_finalize_subgroup( o2_start, o2, (uint32_t)(next_bitfield - first_bitfield) );

	o3 = next_bitfield;

	RESET_LAST_GIDS;

	// write chars
	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 2; ++i )
	{
		if( gid[0] != last_gids[0] || gid[1] != last_gids[1] )
		{
			o3 = next_bitfield++;
			o3->offset = written_chars;
			UPDATE_LAST_GIDS;
		}

		o3->avail |= (uint64_t)1 << gid[2];
		++written_chars;
	}

	// octet 4

	// write start-lookup
	o3_first_byte->offset = (uint32_t)(next_bitfield - first_bitfield); // next
	utf8_write_start_bitfield( codepoints + i, codepoints + num_codepoints, o3_first_byte, 3 );


	RESET_LAST_GIDS;

	o2_start = next_bitfield;
	o2       = o2_start;

	// build o1 and o2
	for( unsigned int i2 = i; i2 < num_codepoints && utf8_split_group_and_gid( codepoints[i2], group, gid ) == 3; ++i2 )
	{
		if( gid[0] != last_gids[0] )
		{
			o2 = next_bitfield++;
			UPDATE_LAST_GIDS;
		}

		o2->avail |= (uint64_t)1 << gid[1];
	}

	utf8_finalize_subgroup( o2_start, o2, (uint32_t)(next_bitfield - first_bitfield) );

	// build o3
	o3_start = next_bitfield;
	o3       = o3_start;

	RESET_LAST_GIDS;

	for( unsigned int i2 = i; i2 < num_codepoints && utf8_split_group_and_gid( codepoints[i2], group, gid ) == 3; ++i2 )
	{
		if( gid[0] != last_gids[0] || gid[1] != last_gids[1] )
		{
			o3 = next_bitfield++;
			UPDATE_LAST_GIDS;
		}

		o3->avail |= (uint64_t)1 << gid[2];
	}

	utf8_finalize_subgroup( o3_start, o3, (uint32_t)(next_bitfield - first_bitfield) );

	// write chars
	o4 = next_bitfield;

	RESET_LAST_GIDS;

	// write chars
	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 3; ++i )
	{
		if( gid[0] != last_gids[0] ||
			gid[1] != last_gids[1] ||
			gid[2] != last_gids[2] )
		{
			o4 = next_bitfield++;
			o4->offset = written_chars;
			UPDATE_LAST_GIDS;
		}

		o4->avail |= (uint64_t)1 << gid[3];
		++written_chars;
	}

	return UTF8_LOOKUP_ERROR_OK;
}

utf8_lookup_error utf8_lookup_calc_table_size( size_t*       table_size,
                                               unsigned int* codepoints,
                                               unsigned int  num_codepoints )
{
	uint64_t GROUP_RESET[3] = { (uint64_t)-1, (uint64_t)-1, (uint64_t)-1 };
	uint64_t last_gids[3];
	uint64_t group[4];
	uint64_t gid[4];
	unsigned int i = 0;

	*table_size = 8;

	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 0; ++i )
	{}

	RESET_LAST_GIDS;

	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 1; ++i )
	{
		if( last_gids[0] != gid[0] )
		{
			(*table_size)++;
			UPDATE_LAST_GIDS;
		}
	}

	// octet 3

	RESET_LAST_GIDS;

	// build o1 and o2
	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 2; ++i )
	{
		if( gid[0] != last_gids[0] ) (*table_size)++;
		if( gid[1] != last_gids[1] ) (*table_size)++;
		if( gid[0] != last_gids[0] || gid[1] != last_gids[1] )
			UPDATE_LAST_GIDS;
	}

	// octet 4
	RESET_LAST_GIDS;

	// write chars
	for( ; i < num_codepoints && utf8_split_group_and_gid( codepoints[i], group, gid ) == 3; ++i )
	{
		if( gid[0] != last_gids[0] ) (*table_size)++;
		if( gid[1] != last_gids[1] ) (*table_size)++;
		if( gid[2] != last_gids[2] ) (*table_size)++;

		if( gid[0] != last_gids[0] ||
			gid[1] != last_gids[1] ||
			gid[2] != last_gids[2] )
			UPDATE_LAST_GIDS;
	}

	(*table_size) *= sizeof( lookup_elem );
	return UTF8_LOOKUP_ERROR_OK;
}


utf8_lookup_error utf8_lookup_perform( void*               lookup,
                                       const uint8_t*      str,
                                       const uint8_t**     str_left,
                                       utf8_lookup_result* res,
                                       size_t*             res_size )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

	while( *pos && res_out != res_end )
	{
		uint8_t first_byte = *pos;
		res_out->pos = pos;

		lookup_elem* lookup_elems = (lookup_elem*)lookup;

		int octet = utf8_num_trailing_bytes( first_byte );

		static const uint64_t START_OFFSET[4] = {   1,  4,  5,  6 };
		static const uint64_t GROUP_MASK[4]   = { 127, 63, 63, 63 };
		static const uint64_t GID_MASK[4]     = {  63, 31, 15,  7 };

		uint64_t curr_offset = START_OFFSET[octet];
		uint64_t group_mask  = GROUP_MASK[octet];
		uint64_t gid_mask    = GID_MASK[octet];

		for( int i = 0; i <= octet; ++i )
		{
			uint64_t group     = (uint64_t)(*pos & group_mask) >> (uint64_t)6;
			uint64_t gid       = (uint64_t)(*pos & gid_mask);
			uint64_t check_bit = (uint64_t)1 << gid;

			gid_mask = 63;

			++pos;

			lookup_elem* elem     = lookup_elems + group + curr_offset;
			uint64_t available    = ( elem->avail & check_bit ) > (uint64_t)0;
			uint64_t items_before = bit_pop_cnt( elem->avail & ( check_bit - (uint64_t)1 ) );

			curr_offset = ( elem->offset + items_before ) * available;
		}

		// curr_offset is now either 0 for not found or offset in glyphs-table

		res_out->offset = (unsigned int)curr_offset;
		++res_out;
	}

	*res_size = (res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}
