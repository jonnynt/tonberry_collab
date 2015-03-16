#ifndef _CACHEMAP_H
#define _CACHEMAP_H

#include "Main.h"
#include <stdint.h>
#include <unordered_map>

using namespace std;

class TextureCache{
private:
	typedef pair<uint64_t, HANDLE>						nhcache_item_t;			// associates hashes with newhandles
	typedef list<nhcache_item_t>						nhcache_list_t;			// holds hashes and their associated newhandle in least-recently-accessed order
	typedef nhcache_list_t::iterator					nhcache_list_iter;
	typedef unordered_map<uint64_t, nhcache_list_iter>	nhcache_map_t;			// maps hashes to an entry in the newhandle list
	typedef nhcache_map_t::iterator						nhcache_map_iter;
	typedef unordered_map<HANDLE, uint64_t>				handlecache_t;			// maps a handle to a hash that has an entry in a nhcache_map_t
	typedef handlecache_t::iterator						handlecache_iter;

	typedef unordered_multimap<uint64_t, HANDLE>		reverse_handlecache_t;	// reverse indexing of handlecache; needed for when values in handlecache are removed from the nhcache
	typedef reverse_handlecache_t::iterator				reverse_handlecache_iter;


	// together these make nhcache:
	nhcache_list_t			*nh_list;
	nhcache_map_t			*nh_map;

	// handlecache:
	handlecache_t			*handlecache;
	reverse_handlecache_t	*reverse_handlecache;

	size_t				max_size;
	size_t				entries;

	/*fastupdate: inserts replaced :-> hash into the handlecache cache.
		PRECONDITION: nh_map[hash] exists.
	*/
	void fastupdate(nhcache_map_iter replaced);

	void map_insert(uint64_t hash,				// nh_map key		- the hash
					HANDLE replaced				// handlecache key	- will point to the new entry in nh_map
	);

public:
	TextureCache(unsigned);
	~TextureCache();

	/*find: determine whether a hash is in the nhcache
	  returns: true if hash is in the nh_map, else false
	*/
	bool TextureCache::contains(uint64_t hash	// the hash to find
		);

	/*find: determine whether a HANDLE is in the handlecache
	returns: true if HANDLE is in the handlecache map, else false
	*/
	bool TextureCache::contains(HANDLE replaced	// the HANDLE to find
		);

	/*at: access an element in the nhcache
	  returns: a reference to the HANDLE mapped to hash in the nhcache if it exists, or else null
	*/
	HANDLE TextureCache::at(uint64_t hash	// the hash key
		);
	
	/*at: access an element in the handlecache
	  returns: a reference to the HANDLE mapped to replaced in the handlecache if it exists, or else null
	*/
	HANDLE TextureCache::at(HANDLE replaced	// the HANDLE key
		);

	/*update: if nh_map[hash] exists, inserts replaced :-> hash into the handlecache cache.
	  returns: true if nh_map[hash] exist, false if it doesn't
	*/
	bool update(HANDLE replaced,	// the handlecache key
				uint64_t hash			// the nhcache key
		);

	/*insert: inserts replaced :-> hash into the cache and hash :-> replacement into the nhcache
	  PRECONDITIONS:
		- hash is not on the cache
		- replacement has been created
	*/
	void insert(HANDLE replaced,	// in-game texture to be replaced by replacement
				uint64_t hash,		// texture hash
				HANDLE replacement	// modded texture handle
		);

	/*erase: removes HANDLE from the cache
	*/
	void erase(HANDLE replaced		// in-game texture to remove from the cache
		);


};

#endif