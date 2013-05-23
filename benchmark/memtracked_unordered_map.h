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

#ifndef MEMTRACKED_UNORDERED_MAP_H_INCLUDED
#define MEMTRACKED_UNORDERED_MAP_H_INCLUDED

#include <limits>
#include <unordered_map>

namespace my_lib
{
	size_t g_num_alloc = 0;
	size_t g_num_bytes = 0;

   template <class T>
   class alloc_override
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

       template <class U> struct rebind { typedef alloc_override<U> other; };

       pointer       address( reference       value ) const { return &value; }
       const_pointer address( const_reference value ) const { return &value; }

        alloc_override() throw() {}
        alloc_override( const alloc_override& ) throw() {}
        template <class U> alloc_override ( const alloc_override<U>& ) throw() {}
       ~alloc_override() throw() {}

       size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(T); }
       pointer allocate( size_type num, const void* = 0 )
       {
    	   ++g_num_alloc;
    	   g_num_bytes += num * sizeof(T);
           pointer ret = (pointer)(::operator new(num*sizeof(T)));
           return ret;
       }
       void construct ( pointer p, const T& value) { new((void*)p)T(value); }
       void destroy   ( pointer p )                { p->~T(); }
       void deallocate( pointer p, size_type)      { ::operator delete((void*)p); }

       void reset_tracking() { g_num_alloc = 0; g_num_bytes = 0; }
   };

   template <class T1, class T2> bool operator== (const alloc_override<T1>&, const alloc_override<T2>&) throw() { return true; }
   template <class T1, class T2> bool operator!= (const alloc_override<T1>&, const alloc_override<T2>&) throw() { return false; }
}

typedef std::unordered_map< unsigned int,
							unsigned int,
							std::hash<unsigned int>,
							std::equal_to<unsigned int>,
							my_lib::alloc_override< std::pair<const unsigned int, unsigned int> > > tracked_unordered_map;

#endif // MEMTRACKED_UNORDERED_MAP_H_INCLUDED
