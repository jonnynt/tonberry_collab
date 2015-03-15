#include "Main.h"
#include "cachemap.h"

#define DEBUG 0

#if DEBUG
string debug_file = "tonberry\\debug\\texture_cache.log";
#endif

TextureCache::TextureCache(unsigned max_size)
{
	this->max_size = max_size;

#if DEBUG
	ofstream debug(debug_file, fstream::out | fstream::trunc);
	debug << "CACHE_SIZE: " << max_size << endl << endl << endl;
	debug.close();
#endif

	nh_list				= new nhcache_list_t();
	nh_map				= new nhcache_map_t();
	handlecache			= new handlecache_t();
	reverse_handlecache = new reverse_handlecache_t();
}

TextureCache::~TextureCache()
{
	delete nh_list;
	delete nh_map;
	delete handlecache;
	delete reverse_handlecache;
}

bool TextureCache::contains(uint64_t hash)
{
	return nh_map->find(hash) != nh_map->end();
}


bool TextureCache::contains(HANDLE replaced)
{
	return handlecache->find(replaced) != handlecache->end();
}


HANDLE TextureCache::at(uint64_t hash)
{
	nhcache_map_iter iter = nh_map->find(hash);

	if (iter == nh_map->end()) return NULL;

	return iter->second->second;
}


HANDLE TextureCache::at(HANDLE replaced)
{

	handlecache_iter cache_iter = handlecache->find(replaced);
	if (cache_iter == handlecache->end()) return NULL;

	nhcache_map_iter map_iter = nh_map->find(cache_iter->second);
	if (map_iter == nh_map->end()) {															// this should never happen
#if DEBUG
		ofstream debug(debug_file, ofstream::out | ofstream::app);
		debug << endl << "ERROR: handlecache entry (" << cache_iter->first << ", " << cache_iter->second << ") not in nh_map!" << endl;
		debug.close();
#endif
		return NULL;
	}

	return map_iter->second->second;
}


void TextureCache::map_insert(uint64_t hash, nhcache_list_iter item, HANDLE replaced)
{
#if DEBUG
	ofstream debug(debug_file, ofstream::out | ofstream::app);
#endif

	// update nh_map with new list item pointer
	pair<nhcache_map_iter, bool> map_insertion = nh_map->insert(							// returns iterator to nh_map[hash] and boolean success
		pair<uint64_t, nhcache_list_iter>(hash, item));
	if (!map_insertion.second)																// if nh_map already contained hash, 
		map_insertion.first->second = nh_list->begin();										// change nh_map[hash] to nh_list->begin()

	pair<handlecache_iter, bool> cache_insertion = handlecache->insert(						// returns iterator to handlecache[HANDLE] and boolean success
		pair<HANDLE, uint64_t>(replaced, hash));
	if (!cache_insertion.second) {															// if handlecache already contained HANDLE,
		uint64_t old_hash = cache_insertion.first->second;

		if (old_hash == hash) {			
			return;
		}																					// otherwise, we need to remove the old reverse_handlecache entry

		pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range = reverse_handlecache->equal_range(old_hash);
		reverse_handlecache_iter backpointer = backpointer_range.first;
		for (; backpointer != backpointer_range.second && backpointer != reverse_handlecache->end(); backpointer++)
			if (backpointer->second == replaced) {
				int size_before = reverse_handlecache->size();
				reverse_handlecache->erase(backpointer);
#if DEBUG
				debug << "\tRemoving (" << backpointer->first << ", " << backpointer->second << ") from reverse_handlecache-> ";
				debug << "(size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;
#endif
				break;
			}
		cache_insertion.first->second = hash;												// change handlecache entry
	}
	reverse_handlecache->emplace(nhcache_item_t(hash, replaced));

#if DEBUG
	debug << "\tAdding (" << item->first << ", (" << replaced << ") to reverse_handlecache:" << endl;
	pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range = reverse_handlecache->equal_range(hash);
	reverse_handlecache_iter backpointer = reverse_handlecache->begin();// backpointer_range.first;
	for (; /*backpointer != backpointer_range.second &&*/ backpointer != reverse_handlecache->end(); backpointer++)
		debug << "\t\t\t\t\t\t\t\t(" << backpointer->first << ", " << backpointer->second << ")" << endl;

	debug << endl;
	debug.close();
#endif
}


void TextureCache::insert(HANDLE replaced, uint64_t hash)
{
#if DEBUG
	ofstream debug(debug_file, ofstream::out | ofstream::app);
	debug << "Inserting (" << replaced << " :-> nh_map[" << hash << "] = " << nh_map->at(hash)->second << "):" << endl;
#endif

	nhcache_map_iter updated = nh_map->find(hash);	//really needed?									// this line is needed, we need to access the map item 
	if (updated == nh_map->end()) return;			//our precondition is to have an existing hash!		// this line... yes, this should never happen, but if for some reason it does
																										// (bug in GlobalContext, whatever) this will prevent a crash
	/* UPDATE NH CACHE ACCESS ORDER */
	nhcache_list_iter item = updated->second;

	// move (most-recently-accessed) list item to front of nh_list
	nh_list->push_front(*item);
	nh_list->erase(item);

#if DEBUG
	debug << "\tMoving (" << item->first << ", " << item->second << ") to front of nh_list: nh_list->begin() = ";
	debug << "(" << nh_list->begin()->first << ", " << nh_list->begin()->second << ")" << endl;
	debug.close();
#endif

	map_insert(hash, nh_list->begin(), replaced);
}


void TextureCache::insert(HANDLE replaced, uint64_t hash, HANDLE replacement)
{
	nh_list->push_front(nhcache_item_t(hash, replacement));

#if DEBUG
	ofstream debug(debug_file, ofstream::out | ofstream::app);
	debug << "Inserting (" << replaced << " :-> (" << hash << ", " << replacement << "):" << endl;
	debug << "\tInserting (" << hash << ", " << replacement << ") at front of nh_list: nh_list->begin() = ";
	debug << "(" << nh_list->begin()->first << ", " << nh_list->begin()->second << ")" << endl;
#endif

	/* MAKE SURE NHCACHE IS THE CORRECT SIZE */
	while (nh_list->size() > max_size) {													// "while" for completeness but this should only ever loop once
		// get pointer to last (least recent) list item
		nhcache_list_iter last_elem = nh_list->end();
		--last_elem;

#if DEBUG
		debug << "\tRemoving (" << last_elem->first << ", " << last_elem->second << ") from back of nh_list." << endl;
#endif

		// dispose of texture
		((IDirect3DTexture9*)last_elem->second)->Release();
		last_elem->second = NULL;

		// if we're going to delete a hash from the nh_map, we need to first remove entries that map to that hash from the handlecache
		nhcache_map_iter to_delete = nh_map->find(last_elem->first);
		pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range =
			reverse_handlecache->equal_range(last_elem->first);

		reverse_handlecache_iter backpointer = backpointer_range.first;
		for (; backpointer != backpointer_range.second && backpointer != reverse_handlecache->end(); backpointer++) {
#if DEBUG
			debug << "\t\tRemoving (" << backpointer->second << ", " << backpointer->first << ") from handlecache." << endl;
#endif
			handlecache->erase(backpointer->second);										// remove from handlecache; reverse_handlecache will be removed
		}																					// afterward to preserve iterators in the backpointer_range
		int size_before = reverse_handlecache->size();
		int num_removed = reverse_handlecache->erase(last_elem->first);

#if DEBUG
		debug << "\t\tRemoved " << num_removed << " entries from reverse_handlecache-> (size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;
		debug << "\tRemoving (" << to_delete->first << ", (" << to_delete->second->first << ", " << to_delete->second->second << ")) from nh_map." << endl;
#endif

		// remove from map (this is why the nh_list stores pair<hash, handle>)
		nh_map->erase(to_delete);

		// pop from list
		nh_list->pop_back();
	}
	/* END MAKE SURE NHCACHE IS THE CORRECT SIZE */

#if DEBUG
	debug.close();
#endif

	map_insert(hash, nh_list->begin(), replaced);
}

void TextureCache::erase(HANDLE replaced)
{
	handlecache_iter iter;
	if ((iter = handlecache->find(replaced)) != handlecache->end()) {

#if DEBUG
		ofstream debug(debug_file, ofstream::out | ofstream::app);
		debug << "Erasing unused HANDLE " << replaced << ": " << endl;
		debug << "\tRemoving (" << iter->first << ", " << iter->second << ") from handlecache." << endl;
#endif

		pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range = reverse_handlecache->equal_range(iter->second);
		reverse_handlecache_iter backpointer = backpointer_range.first;
		for (; backpointer != backpointer_range.second && backpointer != reverse_handlecache->end(); backpointer++)
			if (backpointer->second == replaced) {
				int size_before = reverse_handlecache->size();
				reverse_handlecache->erase(backpointer);									// remove matching backpointer from reverse_handlecache
#if DEBUG
				debug << "\tRemoving (" << backpointer->first << ", " << backpointer->second << ") from reverse_handlecache-> ";
				debug << "(size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;
#endif
				break;
			}

		handlecache->erase(iter);															// remove entry from handlecache

#if DEBUG
		debug << endl;
		debug.close();
#endif
	}
}