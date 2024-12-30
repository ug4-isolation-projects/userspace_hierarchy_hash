//this file contains code from the ht repository by Ben Hoyt, which is licensed under the MIT license

#include "hash.h"
#include "jhash.c"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


size_t get_bucket_index(void* addr, bool shared)
{
    if (shared)
    {
        return SHARED_BUCKET_INDEX;
    }
    uint32_t hash = jhash(&addr, sizeof(addr), 0);
    return hash % (BUCKETS_AMNT - 2); // the shared bucket is the last bucket
}

ht* ht_create(size_t capacity) 
{
    // Allocate space for hash table struct.
    ht* table = malloc(sizeof(ht));
    if (table == NULL) {
        return NULL;
    }
    table->length = 0;
    table->capacity = capacity;

    // Allocate (zero'd) space for entry buckets.
    table->entries = calloc(table->capacity, sizeof(ht_entry));
    if (table->entries == NULL) {
        perror("Error: Could not allocate memory\n");
        free(table); // error, free table before we return!
        return NULL;
    }

    //allocate subtables for each bucket

    for(size_t i = 0; i < table->capacity - 2; i++)
    {
        table->entries[i].subtable = ht_subtable_create(SUBBUCKETS_AMNT);
    }
    table->entries[table->capacity - 1].subtable = ht_subtable_create(SHARED_SUBBUCKETS_AMNT); //shared bucket - 128 is a provisional number subject to testing

    return table;
}

ht_subtable* ht_subtable_create(size_t capacity)
{
    ht_subtable* subtable = malloc(sizeof(ht_subtable));
    if (subtable == NULL)
    {
        free(subtable);
        return NULL;
    }
    subtable->length = 0;
    subtable->capacity = capacity;
    subtable->entries = calloc(subtable->capacity, sizeof(ht_subentry));
    if (subtable->entries == NULL)
    {
        free(subtable->entries);
        free(subtable);
        return NULL;
    }
    return subtable;
}

void ht_add_entry(ht* table, const char* key, void* value, int uid, bool shared)
{
    //find the index in the primary level of the hash table

    size_t bucket_index = get_bucket_index((void*)key, shared);

    //TODO: handle logic for shared bucket. we enforce 1 entry to each bucket. if there is a collision, we resize up, rehash everything and try again.

    printf("bucket_index: %lu\n", bucket_index);

    ht_entry* entry = &table->entries[bucket_index];

    ht_subtable* subtable = entry->subtable;

    //find the sub bucket index in the subtable - if it is not shared, this is hashed by uid.
    //if it is shared, hash as is done in linux

    size_t sub_bucket_index;

    if(!shared)
    {
        //if not shared, hash by uid
        sub_bucket_index = uid % subtable->capacity;
    }
    else
    {
        //in the shared bucket, hash by address as in original implementation in linux
        sub_bucket_index = jhash(&key, sizeof(key), 0) % subtable->capacity;
    }

    printf("sub_bucket_index: %lu\n", sub_bucket_index);

    ht_subentry* subtable_entry = &subtable->entries[sub_bucket_index];

    //create the list of entries if it doesn't exist

    if(!shared && subtable_entry->entries == NULL)
    {
        subtable_entry->entries = malloc(sizeof(ht_subentry_list));
        if(subtable_entry->entries == NULL)
        {
            free(subtable_entry->entries);
            return; // deal with alloc failure later (TODO)
        }

        subtable_entry->entries->count = 0;
        subtable_entry->entries->capacity = INIT_SUBLIST_SIZE;
        subtable_entry->entries->items = malloc(INIT_SUBLIST_SIZE * sizeof(ht_entry_item));

        if(subtable_entry->entries->items == NULL)
        {
            free(subtable_entry->entries);
            return; // deal with alloc failure later (TODO)
        }
    }

    //shared policy - we enforce 1 entry per shared bucket. if there is a collision, we resize up, rehash everything
    //it is important that we optimise numbers here such that resizing is kept to a minimum.

    if(shared && subtable_entry->entries != NULL)
    {
        printf("Error: Shared bucket already has an entry, resizing...\n");
        //keep a copy of the old entries object
        ht_subentry* old_entries = malloc(subtable->capacity * sizeof(ht_subentry));
        for(int i = 0; i < subtable->capacity; i++)
        {
            old_entries[i].entries = subtable->entries[i].entries;
        }

        //resize the subtable
        subtable->capacity *= 2;

        subtable->entries = realloc(subtable->entries, subtable->capacity * sizeof(ht_subentry));
        memset(subtable->entries, 0, subtable->capacity * sizeof(ht_subentry));
        if(subtable->entries == NULL)
        {
            perror("Error: Could not reallocate memory\n");
            free(subtable->entries);
            return; // deal with alloc failure later (TODO)
        }
        //rehash the entries
        for(int i = 0; i < subtable->capacity / 2; i++)
        {
            //if the entry is not null, rehash it
            if(old_entries[i].entries != NULL)
            {
                //Recreating the subtable entry
                printf("Recreating subtable entry...\n");
                ht_subentry* subtable_entry = &subtable->entries[i];
                subtable_entry->entries = malloc(sizeof(ht_subentry_list));
                if(subtable_entry->entries == NULL)
                {
                    free(subtable_entry->entries);
                    return; // deal with alloc failure later (TODO)
                }

                subtable_entry->entries->count = 0;
                subtable_entry->entries->capacity = 1; //only 1 entry in the shared sub bucket
                subtable_entry->entries->items = malloc(sizeof(ht_entry_item));

                printf("entry malloc#'d\n");

                if(subtable_entry->entries->items == NULL)
                {
                    free(subtable_entry->entries);
                    return; // deal with alloc failure later (TODO)
                }
            }
        }
        free(old_entries);
    }

    //if the shared bucket is empty, create a new entry

    else if(shared && subtable_entry->entries == NULL)
    {
        printf("Shared bucket is empty, creating new entry...\n");
        subtable_entry->entries = malloc(sizeof(ht_subentry_list));
        if(subtable_entry->entries == NULL)
        {
            free(subtable_entry->entries);
            return; // deal with alloc failure later (TODO)
        }

        subtable_entry->entries->count = 0;
        subtable_entry->entries->capacity = 1; //only 1 entry in the shared sub bucket
        subtable_entry->entries->items = malloc(sizeof(ht_entry_item));

        if(subtable_entry->entries->items == NULL)
        {
            free(subtable_entry->entries);
            return; // deal with alloc failure later (TODO)
        }
    }

    ht_subentry_list* entry_list = subtable_entry->entries;

    if(entry_list->count >= entry_list->capacity)
    {
        //realloc
        entry_list->capacity *= 2;
        ht_entry_item* new_items = realloc(entry_list->items, entry_list->capacity * sizeof(ht_entry_item));
        if(new_items == NULL)
        {
            free(entry_list->items);
            return; // deal with alloc failure later (TODO)
        }
        entry_list->items = new_items;
    }

    entry_list->items[entry_list->count].key = key;
    entry_list->items[entry_list->count].value = value;
    entry_list->count++;
}

ht_entry_item* get_entry_item(ht* table, const char* key, int uid, bool shared)
{
    //find an item given a key and uid (and it it's shared or not)

    size_t bucket_index = get_bucket_index((void*)key, shared);

    ht_entry* entry = &table->entries[bucket_index];

    if (entry->subtable == NULL)
    {
        perror("Error: Could't find subtable\n");
        free(entry->subtable);
        return NULL;
    }

    ht_subtable* subtable = entry->subtable;
    size_t sub_bucket_index;
    
    if(!shared)
    {
        size_t sub_bucket_index = uid % subtable->capacity;
    }
    else
    {
        sub_bucket_index = jhash(&key, sizeof(key), 0) % subtable->capacity;
    }
    ht_subentry* subtable_entry = &subtable->entries[sub_bucket_index];

    if(subtable_entry->entries == NULL)
    {
        perror("Error: Couldn't find subtable entries\n");
        free(subtable_entry->entries);
        return NULL;
    }

    ht_subentry_list* entry_list = subtable_entry->entries;

    for(size_t i = 0; i < entry_list->count; i++)
    {
        if(strcmp(key, entry_list->items[i].key) == 0)
        {
            return &entry_list->items[i];
        }
    }
    perror("Error: No such entry\n");
    return NULL;
}

int ht_remove_entry(ht* table, const char* key, int uid)
{
    size_t bucket_index = get_bucket_index((void*)key, 0);
    printf("bucket_index: %lu\n", bucket_index);
    ht_entry* entry = &table->entries[bucket_index];
    if(&table->entries[bucket_index] == NULL)
    {
        perror("Error: Couldn't find entry\n");
        free(entry);
    }
    ht_subtable* subtable = entry->subtable;
    if(entry->subtable == NULL)
    {
        perror("Error: Couldn't find subtable\n");
        free(entry->subtable);
    }
    size_t sub_bucket_index = uid % subtable->capacity;
    ht_subentry* subtable_entry = &subtable->entries[sub_bucket_index];
    if(subtable_entry->entries == NULL)
    {
        perror("Error: Couldn't find subtable entries\n");
        free(subtable_entry->entries);
    }
    ht_subentry_list* entry_list = subtable_entry->entries;
    if(subtable_entry->entries == NULL)
    {
        perror("Error: Couldn't find subtable entries\n");
        free(entry_list);
    }

    size_t i;
    for (i = 0; i < entry_list->count; i++)
    {
        if (entry_list->items[i].key == key)
        {
            printf("found entry to remove\n");
            break;
        }
        if(i == entry_list->count - 1)
        {
            perror("Error: Couldn't find entry to remove\n");
            return -1;
        }
    }

    //shift all entries to the left to fill the gap

    for (size_t j = i; j < entry_list->count - 1; j++)
    {
        entry_list->items[j] = entry_list->items[j + 1];
    }
    entry_list->count--;

    if(entry_list->count == 0)
    {
        printf("List is empty, freeing memory.");
        free(entry_list->items);
        free(entry_list);
    }

    //if array is too large, shrink it

    if(entry_list->count < entry_list->capacity / 4 && entry_list->capacity > INIT_SUBLIST_SIZE)
    {
        size_t new_capacity = entry_list->capacity / 2;
        if(new_capacity < INIT_SUBLIST_SIZE)
        {
            new_capacity = INIT_SUBLIST_SIZE;
        }
        ht_entry_item* new_items = realloc(entry_list->items, new_capacity * sizeof(ht_entry_item));
        if(new_items == NULL)
        {
            perror("Error: Could not reallocate memory\n");
            return -1;
        }
        entry_list->items = new_items;
        entry_list->capacity = new_capacity;
        printf("Shrunk the list.\n");
    }
}

void print_entries_in_subtable(ht* table, int uid, size_t bucket_index)
{
    ht_entry* entry = &table->entries[bucket_index];

    if (entry->subtable == NULL)
    {
        perror("Error: Couldn't find subtable\n");
        free(entry->subtable);
        return;
    }

    ht_subtable* subtable = entry->subtable;

    if(subtable->entries == NULL)
    {
        perror("Error: Couldn't find subtable entries\n");
        free(subtable->entries);
        return;
    }

    size_t sub_bucket_index = uid % subtable->capacity;
    ht_subentry* subtable_entry = &subtable->entries[sub_bucket_index];
    if(subtable_entry->entries == NULL)
    {
        perror("Error: Couldn't find subtable entries\n");
        free(subtable_entry->entries);
        return;
    }

    ht_subentry_list* entry_list = subtable_entry->entries;

    if(entry_list->count == 0)
    {
        perror("Error: No entries in subtable\n");
        return;
    }

    for(size_t i = 0; i < entry_list->count; i++)
    {
        printf("Key: %p\n", (void*)entry_list->items[i].key);
        printf("Value: %s\n", (char*)entry_list->items[i].value);
    }
}









