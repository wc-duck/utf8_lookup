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
	UTF8_LOOKUP_ERROR_OK
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
 * @param str_left pointer that will point to start of what is left of string after parse.
 * @param res pointer to buffer where to return result.
 * @param res_size size of res.
 *
 * @return UTF8_LOOKUP_ERROR_OK on success.
 *
 * @note str is assumed to be correct utf8, no error-checking is performed.
 */
utf8_lookup_error utf8_lookup_perform( void*               table,
                                       const uint8_t*      str,
                                       const uint8_t**     str_left,
                                       utf8_lookup_result* res,
                                       size_t*             res_size );

#ifdef __cplusplus
}
#endif

#endif // UTF8_LOOKUP_H_INCLUDED
