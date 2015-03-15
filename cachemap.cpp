#include "Main.h"
#include "cachemap.h"

TextureCache::TextureCache(unsigned max_size)
{
	entries			= 0;
	this->max_size = max_size;

	//ofstream debug(debug_file, fstream::out | fstream::trunc);
	//debug << "CACHE_SIZE: " << max_size << endl << endl << endl;
	//debug.close();

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
	//ofstream debug(debug_file, ofstream::out | ofstream::app);

	handlecache_iter cache_iter = handlecache->find(replaced);
	if (cache_iter == handlecache->end()) return NULL;

	nhcache_map_iter map_iter = nh_map->find(cache_iter->second);
	if (map_iter == nh_map->end()) {															// this should never happen
		//debug << endl << "ERROR: handlecache entry (" << cache_iter->first << ", " << cache_iter->second << ") not in nh_map!" << endl;
		return NULL;
	}

	//debug.close();
	return map_iter->second->second;
}


void TextureCache::map_insert(uint64_t hash, nhcache_list_iter item, HANDLE replaced)
{
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
				//debug << "\tRemoving (" << backpointer->first << ", " << backpointer->second << ") from reverse_handlecache-> ";
				reverse_handlecache->erase(backpointer);
				//debug << "(size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;
				break;
			}
		cache_insertion.first->second = hash;												// change handlecache entry
	}

	//debug << "\tAdding (" << item->first << ", (" << replaced << ") to reverse_handlecache:" << endl;
	reverse_handlecache->emplace(hash, replaced);

	//pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range = reverse_handlecache->equal_range(hash);
	//reverse_handlecache_iter backpointer = reverse_handlecache->begin();// backpointer_range.first;
	//for (; /*backpointer != backpointer_range.second &&*/ backpointer != reverse_handlecache->end(); backpointer++)
		//debug << "\t\t\t\t\t\t\t\t(" << backpointer->first << ", " << backpointer->second << ")" << endl;

	//debug << endl;
	//debug.close();
}


void TextureCache::insert(HANDLE replaced, uint64_t hash)
{
	//ofstream debug(debug_file, ofstream::out | ofstream::app);

	//debug << "Inserting (" << replaced << " :-> nh_map[" << hash << "] = " << nh_map->at(hash)->second << "):" << endl;
	nhcache_map_iter updated = nh_map->find(hash);	//really needed? 
	if (updated == nh_map->end()) return;			//our precondition is to have an existing hash!

	/* UPDATE NH CACHE ACCESS ORDER */
	nhcache_list_iter item = updated->second;

	//debug << "\tMoving (" << item->first << ", " << item->second << ") to front of nh_list: nh_list->begin() = ";
	// move (most-recently-accessed) list item to front of nh_list
	nh_list->push_front(*item);
	nh_list->erase(item);						//IS THIS SAFE??

	//debug << "(" << nh_list->begin()->first << ", " << nh_list->begin()->second << ")" << endl;

	//debug.close();
	map_insert(hash, nh_list->begin(), replaced);
}


void TextureCache::insert(HANDLE replaced, uint64_t hash, HANDLE replacement)
{
	//ofstream debug(debug_file, ofstream::out | ofstream::app);
	//debug << "Inserting (" << replaced << " :-> (" << hash << ", " << replacement << "):" << endl;

	//debug << "\tInserting (" << hash << ", " << replacement << ") at front of nh_list: nh_list->begin() = ";
	nh_list->push_front(nhcache_item_t(hash, replacement));
	//debug << "(" << nh_list->begin()->first << ", " << nh_list->begin()->second << ")" << endl;

	/* MAKE SURE NHCACHE IS THE CORRECT SIZE */
	while (nh_list->size() > max_size) {													// "while" for completeness but this should only ever loop once
		// get pointer to last (least recent) list item
		nhcache_list_iter last_elem = nh_list->end();
		--last_elem;
		//debug << "\tRemoving (" << last_elem->first << ", " << last_elem->second << ") from back of nh_list." << endl;

		// dispose of texture
		((IDirect3DTexture9*)last_elem->second)->Release();
		last_elem->second = NULL;

		// if we're going to delete a hash from the nh_map, we need to first remove entries that map to that hash from the handlecache
		nhcache_map_iter to_delete = nh_map->find(last_elem->first);
		pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range =
			reverse_handlecache->equal_range(last_elem->first);

		reverse_handlecache_iter backpointer = backpointer_range.first;
		for (;  backpointer != backpointer_range.second && backpointer != reverse_handlecache->end(); backpointer++) {
			//debug << "\t\tRemoving (" << backpointer->second << ", " << backpointer->first << ") from handlecache." << endl;
			handlecache->erase(backpointer->second);										// remove from handlecache; reverse_handlecache will be removed
		}																					// afterward to preserve iterators in the backpointer_range
		int size_before = reverse_handlecache->size();
		int num_removed = reverse_handlecache->erase(last_elem->first);
		//debug << "\t\tRemoved " << num_removed << " entries from reverse_handlecache-> (size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;

		// remove from map (this is why the nh_list stores pair<hash, handle>)
		//debug << "\tRemoving (" << to_delete->first << ", (" << to_delete->second->first << ", " << to_delete->second->second << ")) from nh_map." << endl;
		nh_map->erase(to_delete);

		// pop from list
		nh_list->pop_back();
	}
	/* END MAKE SURE NHCACHE IS THE CORRECT SIZE */

	//debug.close();
	map_insert(hash, nh_list->begin(), replaced);
}

void TextureCache::erase(HANDLE replaced)
{
	handlecache_iter iter;
	if ((iter = handlecache->find(replaced)) != handlecache->end()) {
		//ofstream debug(debug_file, ofstream::out | ofstream::app);
		//debug << "Erasing unused HANDLE " << replaced << ": " << endl;
		//debug << "\tRemoving (" << iter->first << ", " << iter->second << ") from handlecache." << endl;

		pair<reverse_handlecache_iter, reverse_handlecache_iter> backpointer_range = reverse_handlecache->equal_range(iter->second);
		reverse_handlecache_iter backpointer = backpointer_range.first;
		for (; backpointer != backpointer_range.second && backpointer != reverse_handlecache->end(); backpointer++)
			if (backpointer->second == replaced) {
				//int size_before = reverse_handlecache->size();
				//debug << "\tRemoving (" << backpointer->first << ", " << backpointer->second << ") from reverse_handlecache-> ";
				reverse_handlecache->erase(backpointer);									// remove matching backpointer from reverse_handlecache
				//debug << "(size: " << size_before << " --> " << reverse_handlecache->size() << ")" << endl;
				break;
			}

		handlecache->erase(iter);															// remove entry from handlecache
		//debug << endl;
		//debug.close();
	}
}