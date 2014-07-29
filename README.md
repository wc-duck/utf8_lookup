# UTF8-lookup

[![Build Status](https://travis-ci.org/wc-duck/utf8_lookup.svg?branch=master)](https://travis-ci.org/wc-duck/utf8_lookup)
[![Build status](https://ci.appveyor.com/api/projects/status/o9c9qv5n6rh6w8gp)](https://ci.appveyor.com/project/wc-duck/utf8-lookup)

A small drop-in library for converting utf8-strings into offsets into a table. Inspired by the HAMT-data structure and designed to 
be used to translate string into a list of glyphs for rendering.


## Properties

* Small memory-footprint
* Rellatively fast ( need more benchmarking before I can say that it is fast )
* Return 0 if value is not available in input-data


## Usage

The library started out when I needed someway of translating characters in a string into a list of glyphs in a font
for rendering within a game.

Just indexing with the chararcters codepoint into a list of glyphs works when only working with ascii, however when
considering languages with characters in the higher values this would be unfeasable.

This library works by using the properties of the utf8 string encoding with an HAMT-inspired bitpacking to get a 
rellatively fast lookup of chars.

Input to the library is a list of unicode codepoints that is available translatable. The library will then return
the index of a codepoint in the input-data, or 0 if not existing, when translating strings.

An example:
```c

struct glyph_data
{
    // ... user-defined glyph-data, could be texture-coords in a texture etc.
};

extern glyph_data* my_glyphs; // place glyph for "not found" on index 0.

unsigned int my_input_chars[] = { 'a', 'b', 'c' /* glyphs available in my_glyphs */ };

// build lookup-structure. This could be done offline, the data generated is directly loadable and usable.

size_t size;
utf8_lookup_calc_table_size( &size, cps, num_cps );

void* lookup_data = malloc( size );

utf8_lookup_error err = utf8_lookup_gen_table( lookup_data, size, my_input_chars, ARRAY_LENGTH( my_input_chars ) );

```

```c

// do lookup...

const uint8* str_iter = (const uint8*)str;
while( *str_iter )
{
	utf8_lookup_result res[128];
	size_t res_size = ARRAY_LENGTH( res );

	utf8_lookup_error err = utf8_lookup_perform( lookup_data, str_iter, &str_iter, res, &res_size );
	ASSERT( err == UTF8_LOOKUP_ERROR_OK );

	for( int i = 0; i < res_size; ++i )
	{
		glyph_data* g = my_glyphs + res[i].character.offset;

		// ... do something with your glyph ...
	}
}

```


## Performance

The library is highly dependent on an efficient implementation of the population-count instruction so on cpu:s
with this implemented in hardware this should be efficient. However it should be noted that there hasn't been 
any big performance-tests done except the ones present in the benchmark test-program, and they can definitively
be improved on!

The first test however suggests that it performs really well in lookup-speed and REALLY well in memory-usage.
