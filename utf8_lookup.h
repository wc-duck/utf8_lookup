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

#ifndef UTF8_LOOKUP_H_INCLUDED
#define UTF8_LOOKUP_H_INCLUDED

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Struct containing result for one translated utf8-codepoint.
 */
struct utf8_lookup_result
{
	const uint8_t* pos;     //< position in input-data that generated result
	unsigned int   offset;  //< offset in glyph-table where to find character.
};

/**
 * Error-codes returned from utf8_lookup.
 */
enum utf8_lookup_error
{
	UTF8_LOOKUP_ERROR_OK,
	UTF8_LOOKUP_ERROR_BUFFER_TO_SMALL
};

/**
 * Calculates the size needed to build lookup-data for all glyphs in codepoints.
 * 
 * @param table_size pointer to a size_t where to return the size.
 * @param codepoints the codepoints to pack, requires codepoints to be sorted from small to big.
 * @param num_codepoints number of codepoints in codepoints.
 *
 * @return UTF8_LOOKUP_ERROR_OK on success.
 */
utf8_lookup_error utf8_lookup_calc_table_size( size_t*       table_size,
                                               unsigned int* codepoints,
                                               unsigned int  num_codepoint );

/**
 * Builds lookup-data to use with utf8_lookup_perform to lookup glyph offsets.
 * 
 * @param table memory area where to build lookup-data
 * @param table_size size of data pointed to by table
 * @param codepoints the codepoints to pack, requires codepoints to be sorted from small to big.
 * @param num_codepoints number of codepoints in codepoints.
 *
 * @return UTF8_LOOKUP_ERROR_OK on success.
 */
utf8_lookup_error utf8_lookup_gen_table( void*         table,
                                         size_t        table_size,
                                         unsigned int* codepoints,
                                         unsigned int  num_codepoint );

/**
 * Perform lookup of offsets for chars in str.
 *
 * @param table memory area containing data packed with utf8_lookup_gen_table.
 * @param str string to make lookup in.
 * @param res pointer to buffer where to return result.
 * @param res_size size of res.
 *
 * @return pointer into str to start of what is left of string after parse.
 *
 * @note str is assumed to be correct utf8, no error-checking is performed.
 */
const uint8_t* utf8_lookup_perform( void*               table,
                                    const uint8_t*      str,
                                    utf8_lookup_result* res,
                                    size_t*             res_size );

#ifdef __cplusplus
}
#endif

#if defined(UTF8_LOOKUP_IMPLEMENTATION)

#include <ctype.h>
#include <string.h>

#if defined( __GNUC__ )
#  include <cpuid.h>
#elif defined( _MSC_VER )
#  include <intrin.h>
#endif

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
#  define UTF8_LOOKUP_ALWAYSINLINE inline __attribute__((always_inline))
#elif defined( _MSC_VER )
#  define UTF8_LOOKUP_ALWAYSINLINE __forceinline
#else
#  define UTF8_LOOKUP_ALWAYSINLINE inline
#endif

static void utf8_lookup_cpuid( uint32_t op, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx )
{
#if defined( __GNUC__ )
	__get_cpuid(op,eax,ebx,ecx,edx);
#elif defined( _MSC_VER )
	int regs[4];
	__cpuid( regs, op );
	*eax = (uint32_t)regs[0];
	*ebx = (uint32_t)regs[1];
	*ecx = (uint32_t)regs[2];
	*edx = (uint32_t)regs[3];
#else
	*eax = 0;
#endif
}

static bool utf8_lookup_has_popcnt()
{
	uint32_t eax; uint32_t ebx; uint32_t ecx; uint32_t edx;
	utf8_lookup_cpuid(0, &eax, &ebx, &ecx, &edx);
	if( eax >= 1 )
	{
		utf8_lookup_cpuid( 1, &eax, &ebx, &ecx, &edx );
		return ecx & ( 1 << 23 );
	}
	return false;
}

static UTF8_LOOKUP_ALWAYSINLINE uint64_t utf8_popcnt_impl( uint64_t val, const int has_popcnt )
{
#if defined( __GNUC__ )
	if( has_popcnt )
		return (uint64_t)__builtin_popcountll( (unsigned long long)val ); // the gcc implementation of popcountll is only faster when the actual instruction exists
#endif

#if defined(_MSC_VER)
#if defined(_WIN64)
	if( has_popcnt )
		return (uint64_t)__popcnt64((uint64_t)val);
#else
	uint32_t* ptr = (uint32_t*)&val;
	if( has_popcnt )
		return (uint64_t)__popcnt(ptr[0]) + __popcnt(ptr[1]);
#endif
#endif

	val = (val & 0x5555555555555555ULL) + ((val >> 1) & 0x5555555555555555ULL);
	val = (val & 0x3333333333333333ULL) + ((val >> 2) & 0x3333333333333333ULL);
	val = (val & 0x0F0F0F0F0F0F0F0FULL) + ((val >> 4) & 0x0F0F0F0F0F0F0F0FULL);
	return (val * 0x0101010101010101ULL) >> 56;
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

// table telling where to start a lookup-traversal depending on how many bytes the current utf8-char is.
// first item in the avail_bits is always 0, this is used as "not found". If sometime in the lookup-loop
// a char is determined that it do not exist, i.e. a bit in the avail_bits-array is not set, the current
// lookup index will be set to 0 and reference this empty bitset for the rest of the lookup.
//
// This was done under the asumption that you mostly do lookups that "hit" the table, i.e. you will need
// to do all loop-iterations so instead of branching, just make the code always loop all iterations.
//
// if this is a gain is something to actually be tested.
static const uint64_t START_OFFSET[4] = { 1, 3, 4, 5 };

utf8_lookup_error utf8_lookup_gen_table( void*         table,
					 	 	 	 	 	 size_t        table_size,
					 	 	 	 	 	 unsigned int* codepoints,
					 	 	 	 	 	 unsigned int  num_codepoints )
{
    memset( table, 0x0, table_size );

    size_t calc_table_size;
    utf8_lookup_calc_table_size( &calc_table_size, codepoints, num_codepoints );

    if( calc_table_size > table_size )
        return UTF8_LOOKUP_ERROR_BUFFER_TO_SMALL;

    size_t items = ( table_size - sizeof(uint64_t) ) / ( sizeof( uint64_t ) + sizeof(uint16_t) );

    uint64_t* avail_bits = (uint64_t*)((uint8_t*)table + sizeof(uint64_t));
    uint16_t* offsets    = (uint16_t*)((uint8_t*)avail_bits + sizeof(uint64_t) * items);

    // loop all codepoints

    unsigned int* start = codepoints;
    unsigned int* end   = codepoints + num_codepoints;

    int curr_elem = 0;

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
                curr_elem = (int)(START_OFFSET[ curr_octet ] + ( gids[ octet ] >> 6  ) );
            }
            else
            {
                // we are in the dynamic section of the table.
                curr_elem = curr_elem < 6 ? 5 : curr_elem;

                if( ( memcmp( last_prev_gids, gids, (size_t)( octet ) * sizeof( unsigned int ) ) != 0 ) || last_octet != curr_octet )
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
                offsets[ curr_elem ] = 1;
            }
            else
            {
                // mark element as group
                offsets[ curr_elem ] = 2;
            }

			// set the avail bit
			avail_bits[ curr_elem ] |= (uint64_t)1 << ( gids[ octet ] & 63 );
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
		group_offset = (uint32_t)octet_start[ octet + 1 ];

        for( int j = octet_start[ octet ]; j < octet_start[ octet + 1 ]; ++j )
        {
            if( offsets[ j ] == 1 )
            {
                // writing a char
                offsets[ j ] = (uint16_t)char_offset;
                char_offset += (uint32_t)utf8_popcnt_impl( avail_bits[ j ], 0 );
            }
            else if( offsets[ j ] == 2 )
            {
                // writing a group
                offsets[ j ] = (uint16_t)group_offset;
                group_offset += (uint32_t)utf8_popcnt_impl( avail_bits[ j ], 0 );
            }
        }
    }

    *((uint64_t*)table) = (uint64_t)(curr_elem + 2);

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
                curr_elem = (unsigned int)(START_OFFSET[ curr_octet ] + ( gids[ octet ] >> 6 ) );
            }
            else
            {
                // we are in the dynamic section of the table.
                curr_elem = curr_elem < 6 ? 5 : curr_elem;

                if( ( memcmp( last_prev_gids, gids, (size_t)( octet ) * sizeof( unsigned int ) ) != 0 ) || last_octet != curr_octet )
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

    // table layout:
    // uint64_t              - item_count, kept as uint64_t to get valid alignment for the following arrays.
    // uint64_t[item_count]  - availabillity bits.
    // uint16_t[item_count]  - group-offsets.
    unsigned int item_count = curr_elem + 2; // TODO: remember why I need the +2 here and document. ( unittests fail without it )
    *table_size = sizeof(uint64_t) +
                  ( item_count * sizeof(uint64_t) ) +
                  ( item_count * sizeof(uint16_t) );
	return UTF8_LOOKUP_ERROR_OK;
}

UTF8_LOOKUP_ALWAYSINLINE const uint8_t* utf8_lookup_perform_impl( void*               lookup,
													  const uint8_t*      str,
													  utf8_lookup_result* res,
													  size_t*             res_size,
													  int                 has_popcnt )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

    uint64_t  items      = *((uint64_t*)lookup);
    uint64_t* avail_bits = (uint64_t*)((uint8_t*)lookup + sizeof(uint64_t));
    uint16_t* offsets    = (uint16_t*)((uint8_t*)avail_bits + sizeof(uint64_t) * items);

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

	while( *pos && res_out != res_end )
	{
		uint8_t first_byte = *pos;
		res_out->pos = pos;

		int octet = UTF8_TRAILING_BYTES_TABLE[ first_byte ];

		static const uint64_t GROUP_MASK[4]   = { 127, 63, 63, 63 };
		static const uint64_t GID_MASK[4]     = {  63, 31, 15,  7 };

		uint64_t curr_offset = START_OFFSET[octet];
		uint64_t group_mask  = GROUP_MASK[octet];
		uint64_t gid_mask    = GID_MASK[octet];

		for( int i = 0; i <= octet; ++i )
		{
			// make sure that we get a value between 0-63 to decide what bit the current byte.
			// it is only octet 1 that will have more than 6 significant bits.
			uint64_t group     = (uint64_t)(*pos & group_mask) >> (uint64_t)6;

			// mask of the bits that is valid in this mask, only the first byte will have a
			// different amount of set bits. Thereof the table above.
			uint64_t gid       = (uint64_t)(*pos & gid_mask);

			uint64_t check_bit = (uint64_t)1 << gid;

			// gid mask will always be 0b111111 i.e. the lowest 6 bit set on all loops except
			// the first one. This is due to how utf8 is structured, see table at the top of
			// the file.
			gid_mask = 63;

			++pos;

			// index in avail_bits and corresponding offsets that we are currently working in.
			uint64_t index = group + curr_offset;

			// how many bits are set "before" the current element in this group? this is used
			// to calculate the next item in the lookup.
			uint64_t items_before = utf8_popcnt_impl( avail_bits[index] & ( check_bit - (uint64_t)1 ), has_popcnt );

			// select the next offset in the avail_bits-array to check or if this is the last iteration this
			// will be the actual result.
			// note: if the lookup is a miss, i.e. bit is not set, point curr_offset to 0 that is a bitfield
			//       that is always 0 and offsets[0] == 0 to just keep on "missing"
			curr_offset = ( avail_bits[index] & check_bit ) > (uint64_t)0 ? offsets[index] + items_before : 0x0;
		}

		// curr_offset is now either 0 for not found or offset in glyphs-table

		res_out->offset = (unsigned int)curr_offset;
		++res_out;
	}

	*res_size = (size_t)(res_out - res);
	return pos;
}

const uint8_t* utf8_lookup_perform_scalar( void*               lookup,
                                           const uint8_t*      str,
                                           utf8_lookup_result* res,
                                           size_t*             res_size )
{
	return utf8_lookup_perform_impl( lookup, str, res, res_size, 0 );
}

#if defined(__GNUC__)
#  if defined(__clang__)
#    if defined(__has_attribute)
#      if __has_attribute(target)
#        define UTF8_LOOKUP_HAS_ATTRIBUTE_TARGET
#      endif
#    endif
#  else
#      define UTF8_LOOKUP_HAS_ATTRIBUTE_TARGET
#  endif
#endif

#if defined(UTF8_LOOKUP_HAS_ATTRIBUTE_TARGET)
// ... tell gcc to optimize this as if a popcnt instruction exists ...
const uint8_t* utf8_lookup_perform_popcnt( void*               lookup,
                                           const uint8_t*      str,
                                           utf8_lookup_result* res,
                                           size_t*             res_size ) __attribute__((target("popcnt")));
#endif

const uint8_t* utf8_lookup_perform_popcnt( void*               lookup,
                                           const uint8_t*      str,
                                           utf8_lookup_result* res,
                                           size_t*             res_size )
{
	return utf8_lookup_perform_impl( lookup, str, res, res_size, 1 );
}

const uint8_t* utf8_lookup_perform( void*               lookup,
                                    const uint8_t*      str,
                                    utf8_lookup_result* res,
                                    size_t*             res_size )
{
	static const uint8_t* (*_func)( void*, const uint8_t*, utf8_lookup_result*, size_t* ) = 0;
	if( _func == 0 )
	{
		if(utf8_lookup_has_popcnt())
			_func = utf8_lookup_perform_popcnt;
		else
			_func = utf8_lookup_perform_scalar;
	}

	return _func( lookup, str, res, res_size );
}

#endif // defined(UTF8_LOOKUP_IMPLEMENTATION)

#endif // UTF8_LOOKUP_H_INCLUDED
