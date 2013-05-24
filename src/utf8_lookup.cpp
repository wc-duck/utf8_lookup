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

#if defined( __GNUC__ )
#  define ALWAYSINLINE __attribute__((always_inline))
#elif defined( _MSC_VER )
#  define ALWAYSINLINE __forceinline
#else
#  define ALWAYSINLINE inline
#endif

static ALWAYSINLINE uint64_t bit_pop_cnt( uint64_t val )
{
#if defined( __GNUC__ ) && defined( __POPCNT__ )
	// the gcc implementation of popcountll is only faster when the actual instruction exists
	return __builtin_popcountll( (unsigned long long)val );
#else
	val = (val & 0x5555555555555555ULL) + ((val >> 1) & 0x5555555555555555ULL);
	val = (val & 0x3333333333333333ULL) + ((val >> 2) & 0x3333333333333333ULL);
	val = (val & 0x0F0F0F0F0F0F0F0FULL) + ((val >> 4) & 0x0F0F0F0F0F0F0F0FULL);
	return (val * 0x0101010101010101ULL) >> 56;
#endif
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

/*static */int utf8_split_to_bytes( unsigned int cp, unsigned int* bytes )
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

struct lookup_elem
{
	uint64_t avail;
	uint32_t offset;
};

static const uint64_t START_OFFSET[4] = { 1, 3, 4, 5 };

#include <stdio.h>

utf8_lookup_error utf8_lookup_gen_table( void*         table,
					 	 	 	 	 	 size_t        table_size,
					 	 	 	 	 	 unsigned int* codepoints,
					 	 	 	 	 	 unsigned int  num_codepoints )
{
    memset( table, 0x0, table_size );
    
    lookup_elem* out_table = (lookup_elem*)table;

    // loop all codepoints
    
    unsigned int* start = codepoints;
    unsigned int* end   = codepoints + num_codepoints;
    
    unsigned int curr_elem = 0;
    
    int octet_start[5] = { -1, -1, -1, -1, -1 };
    int last_octet = -1;
    
    for( int octet = 0; octet < 5; ++octet )
    {
        unsigned int last_prev_gids[4] = { (unsigned int)-1, (unsigned int)-1, (unsigned int)-1, (unsigned int)-1 };
        unsigned int* curr = start;
        while( curr != end )
        {
			unsigned int gids[4];
            int curr_octet = utf8_split_to_bytes( *curr++, gids );

            if( octet == 0 )
            {
                // we are in the static section of the table.
                curr_elem = (unsigned int)(START_OFFSET[ curr_octet ] + ( gids[ octet ] >> 6  ) );
            }
            else
            {
                // we are in the dynamic section of the table.
                curr_elem = curr_elem < 6 ? 5 : curr_elem;
                
                if( ( memcmp( last_prev_gids, gids, ( octet ) * sizeof( unsigned int ) ) != 0 ) || last_octet != curr_octet )
                {
                    ++curr_elem;
                    memcpy( last_prev_gids, gids, sizeof( gids ) );
                }
            }
            
            last_octet = curr_octet;

            if( octet_start[ octet ] == -1 )
                octet_start[ octet ] = curr_elem;
            
            if( octet == curr_octet )
            {
                // advance start, we will not do anything with this char again.
                ++start;
                
                // mark element as chars
                out_table[ curr_elem ].offset = 1;
            }
            else
            {
                // mark element as group
                out_table[ curr_elem ].offset = 2;
            }

			// set the avail bit
			out_table[ curr_elem ].avail |= (uint64_t)1 << gids[ octet ];
        }
    }

    // write offsets!

    octet_start[4] = curr_elem + 1;
    int last_start = curr_elem + 1;
    for( int i = 3; i >= 0; --i )
    {
        if( octet_start[i] == -1 )
            octet_start[i] = last_start;
        else
            last_start = octet_start[i];
    }
    
    uint32_t char_offset = 1;
    uint32_t group_offset = 6;
    
    for( unsigned int octet = 0; octet < 4; ++octet )
    {
		group_offset = octet_start[ octet + 1 ];

        for( int j = octet_start[ octet ]; j < octet_start[ octet + 1 ]; ++j )
        {
            if( out_table[ j ].offset == 1 )
            {
                // writing a char
                out_table[ j ].offset = char_offset;
                char_offset += (uint32_t)bit_pop_cnt( out_table[ j ].avail );
            }
            else if( out_table[ j ].offset == 2 )
            {
                // writing a group
                out_table[ j ].offset = group_offset;
                group_offset += (uint32_t)bit_pop_cnt( out_table[ j ].avail );
            }
        }
    }

	return UTF8_LOOKUP_ERROR_OK;
}

utf8_lookup_error utf8_lookup_calc_table_size( size_t*       table_size,
                                               unsigned int* codepoints,
                                               unsigned int  num_codepoints )
{
    // loop all codepoints

    unsigned int* start = codepoints;
    unsigned int* end   = codepoints + num_codepoints;

    unsigned int curr_elem = 0;
    int last_octet = -1;

    for( int octet = 0; octet < 5; ++octet )
    {
        unsigned int last_prev_gids[4] = { (unsigned int)-1, (unsigned int)-1, (unsigned int)-1, (unsigned int)-1 };
        unsigned int* curr = start;
        while( curr != end )
        {
			unsigned int gids[4];
            int curr_octet = utf8_split_to_bytes( *curr++, gids );

            if( octet == 0 )
            {
                // we are in the static section of the table.
                curr_elem = (unsigned int)(START_OFFSET[ curr_octet ] + ( gids[ octet ] >> 6  ) );
            }
            else
            {
                // we are in the dynamic section of the table.
                curr_elem = curr_elem < 6 ? 5 : curr_elem;

                if( ( memcmp( last_prev_gids, gids, ( octet ) * sizeof( unsigned int ) ) != 0 ) || last_octet != curr_octet )
                {
                    ++curr_elem;
                    memcpy( last_prev_gids, gids, sizeof( gids ) );
                }
            }

            last_octet = curr_octet;

            if( octet == curr_octet )
                // advance start, we will not do anything with this char again.
                ++start;
        }
    }

    *table_size = ( curr_elem + 2 ) * sizeof( lookup_elem );
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
			uint64_t items_before = bit_pop_cnt( elem->avail & ( check_bit - (uint64_t)1 ) );

			curr_offset = ( elem->avail & check_bit ) > (uint64_t)0 ? elem->offset + items_before : 0x0;
		}

		// curr_offset is now either 0 for not found or offset in glyphs-table

		res_out->offset = (unsigned int)curr_offset;
		++res_out;
	}

	*res_size = (res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}
