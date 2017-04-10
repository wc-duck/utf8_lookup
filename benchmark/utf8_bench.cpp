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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <set>
#include <map>
#include <vector>
#include <unordered_map>
#include <algorithm>

size_t g_num_alloc = 0;
size_t g_num_free  = 0;
size_t g_num_bytes = 0;

template <class T>
class utf8_bench_std_alloc
{
	public:
	// type definitions
	typedef T              value_type;
	typedef T*             pointer;
	typedef const T*       const_pointer;
	typedef T&             reference;
	typedef const T&       const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	template <class U> struct rebind { typedef utf8_bench_std_alloc<U> other; };

	pointer       address( reference       value ) const { return &value; }
	const_pointer address( const_reference value ) const { return &value; }

	utf8_bench_std_alloc() throw() {}
	utf8_bench_std_alloc( const utf8_bench_std_alloc& ) throw() {}
	template <class U> utf8_bench_std_alloc ( const utf8_bench_std_alloc<U>& ) throw() {}
	~utf8_bench_std_alloc() throw() {}

	size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(T); }
	pointer allocate( size_type num, const void* = 0 )
	{
		++g_num_alloc;
		g_num_bytes += num * sizeof(T);
		return (pointer)(::operator new(num*sizeof(T)));
	}
	template< class U, class... Args >
	void construct( U* p, Args&&... args )      { new((void*)p)T(args...); }
	void destroy   ( pointer p )                { p->~T(); }
	void deallocate( pointer p, size_type num)
	{
		++g_num_free;
		g_num_bytes -= num * sizeof(T);
		::operator delete((void*)p);
	}

};

void reset_tracking() { g_num_alloc = 0; g_num_free = 0; g_num_bytes = 0; }

template <class T1, class T2> bool operator== (const utf8_bench_std_alloc<T1>&, const utf8_bench_std_alloc<T2>&) throw() { return true; }
template <class T1, class T2> bool operator!= (const utf8_bench_std_alloc<T1>&, const utf8_bench_std_alloc<T2>&) throw() { return false; }

typedef std::map< unsigned int,
				  unsigned int,
				  std::less<unsigned int>,
				  utf8_bench_std_alloc< std::pair<const unsigned int, unsigned int> > > tracked_map;
typedef std::unordered_map< unsigned int,
							unsigned int,
							std::hash<unsigned int>,
							std::equal_to<unsigned int>,
							utf8_bench_std_alloc< std::pair<const unsigned int, unsigned int> > > tracked_unordered_map;

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
		return ( (unsigned int)fb[1] & 0x3F ) | ( ( (unsigned int)fb[0] & 0x1F ) << 6 );
	}
	else if( fb[0] <= 0xEF )
	{
		*str = fb + 3;
		return ( (unsigned int)fb[2] & 0x3F ) | ( ( (unsigned int)fb[1] & 0x3F ) << 6 ) | ( ( (unsigned int)fb[0] & 0xF )  << 12 );
	}
	else if ( fb[0] <= 0xF7 )
	{
		*str = fb + 4;
		return ( (unsigned int)fb[3] & 0x3F ) | ( ( (unsigned int)fb[2] & 0x3F ) << 6 ) | ( ( (unsigned int)fb[1] & 0x3F ) << 12 ) | ( ( (unsigned int)fb[0] & 0x7 ) << 18 );
	}

	return 0;
}

#if defined( __GNUC__ )
#  define ALWAYSINLINE inline __attribute__((always_inline))
#elif defined( _MSC_VER )
#  define ALWAYSINLINE __forceinline
#else
#  define ALWAYSINLINE inline
#endif

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

utf8_lookup_error utf8_lookup_perform_std_cmp( tracked_map& compare_map, const uint8_t* str, const uint8_t** str_left, utf8_lookup_result* res, size_t* res_size )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

	while( *pos && res_out != res_end )
	{
		res_out->pos    = pos;
		int code_point = utf8_to_unicode_codepoint( &pos );

		tracked_map::iterator it = compare_map.find( code_point );
		res_out->offset = it == compare_map.end() ? 0 : it->second;
		++res_out;
	}

	*res_size = (int)(res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}

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

struct test_case
{
	const char* name;
	uint64_t    runtime;
	size_t      allocs;
	size_t      frees;
	size_t      memused;
};

struct bitarray_lookup
{
	uint64_t* lookup;
	uint16_t* offsets;
};

utf8_lookup_error utf8_lookup_perform_bitarray_cmp( const bitarray_lookup& bitarray, const uint8_t* str, const uint8_t** str_left, utf8_lookup_result* res, size_t* res_size )
{
	utf8_lookup_result* res_out = res;
	utf8_lookup_result* res_end = res + *res_size;

	const uint8_t* pos = str;

	while( *pos && res_out != res_end )
	{
		res_out->pos   = pos;
		int code_point = utf8_to_unicode_codepoint( &pos );

		int index = code_point >> 6;
		int bit = code_point & ( 64 - 1 );

		uint64_t bit_mask = 1ULL << bit;
		uint64_t lookup = bitarray.lookup[index];

		if( lookup & bit_mask )
			res_out->offset = bitarray.offsets[index] + (unsigned int)bit_pop_cnt(lookup & (bit_mask - 1ULL));
		else
			res_out->offset = 0;

		++res_out;
	}

	*res_size = (int)(res_out - res);
	*str_left = pos;
	return UTF8_LOOKUP_ERROR_OK;
}

static void build_bitarray_lookup_map( std::vector<unsigned int>& cps, bitarray_lookup& output, test_case* test )
{
	unsigned int max_codepoint = cps.back();
	unsigned int needed_elements = max_codepoint / 64 + 1;
	test->memused = needed_elements * ( sizeof(output.lookup[0]) + sizeof(output.offsets[0]) );
	output.lookup = (uint64_t*)malloc( test->memused );
	output.offsets = (uint16_t*)(output.lookup + needed_elements);
	memset( output.lookup, 0x0, test->memused );

	for( size_t i = 0; i < cps.size(); ++i )
	{
		unsigned int cp = cps[i];
		unsigned int index = cp >> 6;
		unsigned int bit = cp & (64-1);

		uint64_t bit_mask = 1ULL << (uint64_t)bit;

		if( output.lookup[index] == 0 )
			output.offsets[index] = (uint16_t)(i + 1); // + 1 since 0 is reserved for "error"
		output.lookup[index] |= bit_mask;
	}

	test->allocs = 1;
	test->frees  = 0;
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


	for( size_t i = 0; i < cps.size(); ++i )
		++count[ codepoint_to_octet( cps[i] ) ];

	printf( "num codepoints in file %lu\n", cps.size() );
	printf( "octet: %7d %7d %7d %7d\n", 1, 2, 3, 4);
	printf( "       %7u %7u %7u %7u\n\n", count[0], count[1], count[2], count[3]);
}

void build_compare_map( std::vector<unsigned int>& cps, tracked_map& output, test_case* test )
{
	reset_tracking();
	for( size_t i = 0; i < cps.size(); ++i )
		output.insert( std::make_pair( cps[i], (unsigned int)i + 1 ) );

	test->allocs  = g_num_alloc;
	test->frees   = g_num_free;
	test->memused = g_num_bytes;
	reset_tracking();
}

void build_compare_map( std::vector<unsigned int>& cps, tracked_unordered_map& output, test_case* test )
{
	reset_tracking();
	for( size_t i = 0; i < cps.size(); ++i )
		output[ cps[i] ] = (unsigned int)i + 1;
	test->allocs  = g_num_alloc;
	test->frees   = g_num_free;
	test->memused = g_num_bytes;
	reset_tracking();
}

static void* build_utf8_lookup_map( std::vector<unsigned int>& cps, test_case* test )
{
	utf8_lookup_calc_table_size( &test->memused, &cps[0], (unsigned int)cps.size() );
	void* table = malloc( test->memused );
	utf8_lookup_gen_table( table, test->memused, &cps[0], (unsigned int)cps.size() );
	test->allocs = 1;
	test->frees  = 0;
	return table;
}

static void run_test_case(const char* test_text_file)
{
	printf("\ntest text: %s\n", test_text_file);

	size_t file_size;
	uint8_t* text_data = load_file( test_text_file, &file_size );
	if( text_data == 0x0 )
	{
		printf( "couldn't load file %s\n", test_text_file );
		return;
	}

	uint8_t* text = text_data;

	// skip BOM
	if( text_data[0] == 0xEF && text_data[1] == 0xBB && text_data[2] == 0xBF )
	{
		text = &text_data[3];
	}

	test_case test_cases[] = {
		{ "utf8_lookup", 0 ,0, 0, 0 },
		{ "std::map", 0 ,0, 0, 0 },
		{ "std::unordered_map", 0 ,0, 0, 0 },
		{ "bitarray", 0 ,0, 0, 0 }
	};

	std::vector<unsigned int> cps;
	find_all_codepoints( text, cps );

	// build lookup map
	void* table = build_utf8_lookup_map( cps, &test_cases[0] );

	tracked_map compare_map;
	tracked_unordered_map compare_unordered_map;
	build_compare_map( cps, compare_map, &test_cases[1] );
	build_compare_map( cps, compare_unordered_map, &test_cases[2] );
	bitarray_lookup bitarray;
	build_bitarray_lookup_map( cps, bitarray, &test_cases[3] );

	{
		utf8_lookup_result res[256];
		size_t res_size = ARRAY_LENGTH(res);

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform( table, str_iter, &str_iter, res, &res_size );
		}
		test_cases[0].runtime = cpu_tick() - start;
	}

	{
		utf8_lookup_result res[256];
		size_t res_size = ARRAY_LENGTH(res);

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform_std_cmp( compare_map, str_iter, &str_iter, res, &res_size );
		}
		test_cases[1].runtime = cpu_tick() - start;
	}

	{
		utf8_lookup_result res[256];
		size_t res_size = ARRAY_LENGTH(res);

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform_std_cmp( compare_unordered_map, str_iter, &str_iter, res, &res_size );
		}
		test_cases[2].runtime = cpu_tick() - start;
	}

	{
		utf8_lookup_result res[256];
		size_t res_size = ARRAY_LENGTH(res);

		uint64_t start = cpu_tick();

		for( int i = 0; i < 100; ++i )
		{
			const uint8_t* str_iter = text;
			while( *str_iter )
				utf8_lookup_perform_bitarray_cmp( bitarray, str_iter, &str_iter, res, &res_size );
		}
		test_cases[3].runtime = cpu_tick() - start;
	}

	printf("%-20s%-20s%-20s%-20s%-20s%-20s%-20s\n", "name", "allocs", "frees", "memused (kb)", "bytes/codepoint", "time (sec)", "GB/sec");
	for( size_t i = 0; i < ARRAY_LENGTH(test_cases); ++i )
	{
		float time = cpu_ticks_to_sec( test_cases[i].runtime );
		printf("%-20s%-20zu%-20zu%-20f%-20f%-20f%-20f\n", test_cases[i].name,
										  	   		 	  test_cases[i].allocs,
													 	  test_cases[i].frees,
										  	   		 	  (float)test_cases[i].memused / 1024.0f,
													 	  (float)(test_cases[i].memused) / (float)cps.size(),
											   		 	  time,
											   		 	  ( (float)file_size / ( 1024 * 1024 ) ) / time);
	}

	// check output!
	{
		utf8_lookup_result res1[256];
		utf8_lookup_result res2[256];
		utf8_lookup_result res3[256];
		size_t res_size1 = 256;
		size_t res_size2 = 256;
		size_t res_size3 = 256;

		memset( res1, 0x0, sizeof(res1) );
		memset( res2, 0x0, sizeof(res2) );
		memset( res3, 0x0, sizeof(res3) );

		const uint8_t* str_iter1 = text;
		const uint8_t* str_iter2 = text;
		const uint8_t* str_iter3 = text;

		unsigned int chars = 0;
		unsigned int num_missmatch = 0;

		int loop = 0;
		bool print = true;
		while( *str_iter1 && *str_iter2 )
		{
			utf8_lookup_perform_std_cmp( compare_unordered_map, str_iter1, &str_iter1, res1, &res_size1 );
			utf8_lookup_perform( table, str_iter2, &str_iter2, res2, &res_size2 );
			utf8_lookup_perform_bitarray_cmp( bitarray, str_iter3, &str_iter3, res3, &res_size3 );

			if( str_iter1 != str_iter2 ) { printf("mismatching str_iter!\n"); break; }
			if( str_iter1 != str_iter3 ) { printf("mismatching str_iter!\n"); break; }
			if( res_size1 != res_size2 ) { printf("mismatching res_size!\n"); break; }
			if( res_size1 != res_size3 ) { printf("mismatching res_size!\n"); break; }

			for( size_t i = 0; i < res_size1; ++i )
			{
				++chars;

				if( res1[i].pos != res2[i].pos )
				{
					if( print )
						printf("%d[%lu] = pos mismatch! %p %p\n", loop, i, res1[i].pos, res2[i].pos);
				}

				if( res1[i].pos != res3[i].pos )
				{
					if( print )
						printf("%d[%lu] = pos mismatch! %p %p\n", loop, i, res1[i].pos, res3[i].pos);
				}

				if( res1[i].offset != res2[i].offset )
				{
					if( print )
					{
						printf("%d[%lu] = %u %u\n", loop, i, res1[i].offset, res2[i].offset );
						uint8_t test[4] = { res1[i].pos[0], res1[i].pos[1], res1[i].pos[2], '\0' };
						printf("char %s\n", test);
						print = false;
					}
					++num_missmatch;
				}

				if( res1[i].offset != res3[i].offset )
				{
					if( print )
					{
						printf("%d[%lu] = %u %u\n", loop, i, res1[i].offset, res3[i].offset );
						uint8_t test[4] = { res1[i].pos[0], res1[i].pos[1], res1[i].pos[2], '\0' };
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
	free( bitarray.lookup );
}

int main( int argc, char** argv )
{
	if( argc < 1 )
	{
		printf( "bad input parameters!\n" );
		return 1;
	}

	for( int i = 1; i < argc; ++i )
		run_test_case(argv[i]);

	return 0;
}
