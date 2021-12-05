/*
 * EECS 370, University of Michigan
 * Project 4: LC-2K Cache Simulator
 * Instructions are found in the project spec.
 */

#include <stdio.h>
#include <math.h>

#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

extern int mem_access(int addr, int write_flag, int write_data);

enum actionType
{
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

typedef struct blockStruct
{
    int data[MAX_BLOCK_SIZE];
    int dirty;
    int valid;
    int lruLabel;
    int set;
    int tag;
} blockStruct;

typedef struct cacheStruct
{
    blockStruct blocks[MAX_CACHE_SIZE][MAX_BLOCK_SIZE];
    int blockSize;
    int numSets;
    int blocksPerSet;
    int tag_bits_length;
    int set_bits_length;
    int block_offset_bits_length;

    int num_instructions;
    int num_words_accessed;
    int num_dirty_left;
    int hits;
    int misses;
    int writebacks;

} cacheStruct;

/* Global Cache variable */
cacheStruct cache;

void printAction(int, int, enum actionType);
void printCache();
void fill_block(int, int, int, int);
void replace_block(int, int, int, int);
void write_back(int, int, int);

/*
 * Set up the cache with given command line parameters. This is 
 * called once in main(). You must implement this function.
 */
void cache_init(int blockSize, int numSets, int blocksPerSet)
{
    cache.blockSize = blockSize;
    cache.numSets = numSets;
    cache.blocksPerSet = blocksPerSet;

    cache.block_offset_bits_length = log(blocksPerSet) / log(2);
    cache.set_bits_length = log(numSets) / log(2);
    cache.tag_bits_length = 16 - cache.block_offset_bits_length - cache.set_bits_length;

    cache.hits = 0;
    cache.misses = 0;
    cache.writebacks = 0;
    cache.num_instructions = 0;
    cache.num_words_accessed = 0;
    cache.num_dirty_left = 0;

    for (int s = 0; s < MAX_CACHE_SIZE; s++)
    {
        for (int b = 0; b < MAX_CACHE_SIZE; b++)
        {
            cache.blocks[s][b].dirty = 0;
            cache.blocks[s][b].valid = 0;
            cache.blocks[s][b].lruLabel = 0;
        }
    }

    return;
}

/*
 * Access the cache. This is the main part of the project,
 * and should call printAction as is appropriate.
 * It should only call mem_access when absolutely necessary.
 * addr is a 16-bit LC2K word address.
 * write_flag is 0 for reads (fetch/lw) and 1 for writes (sw).
 * write_data is a word, and is only valid if write_flag is 1.
 * The return of mem_access is undefined if write_flag is 1.
 * Thus the return of cache_access is undefined if write_flag is 1.
 */
int cache_access(int addr, int write_flag, int write_data)
{

    //printCache();

    int block_offset_bits_mask = 0xFFFF;
    int set_bits_mask = 0xFFFF;
    int tag_bits_mask = 0xFFFF;

    for (int i = 0; i < 16 - cache.block_offset_bits_length; i++)
    {
        block_offset_bits_mask >>= 1;
    }
    for (int i = 0; i < 16 - cache.set_bits_length; i++)
    {
        set_bits_mask >>= 1;
    }
    for (int i = 0; i < 16 - cache.tag_bits_length; i++)
    {
        tag_bits_mask >>= 1;
    }

    int block_offset = addr & block_offset_bits_mask;
    int set_index = addr & set_bits_mask;
    int tag = addr & tag_bits_mask;

    // For fetch/lw
    if (write_flag == 0)
    {
        for (int b = 0; b < cache.blocksPerSet; b++)
        {
            if (cache.blocks[set_index][b].valid && cache.blocks[set_index][b].tag == tag)
            {
                printAction(addr, 1, cacheToProcessor);
                cache.blocks[set_index][b].lruLabel++;
                return cache.blocks[set_index][b].data[block_offset];
            }
        }

        // If not found in cache, access main memory and read a block of data
        int begin = addr & 0xFFFC;

        int no_empty_block = 1;
        for (int b = 0; b < cache.blocksPerSet; b++)
        {
            if (!cache.blocks[set_index][b].valid)
            {
                // Fill the block
                no_empty_block = 0;
                fill_block(set_index, b, begin, tag);
                break;
            }
        }

        // If we don't find an empty block, invoke replacement policy (LRU)
        if (no_empty_block == 1)
        {
            int replace_target = 0, max_lru = 0;
            for (int b = 0; b < cache.blocksPerSet; b++)
            {
                if (cache.blocks[set_index][b].lruLabel > max_lru)
                {

                    max_lru = cache.blocks[set_index][b].lruLabel;
                    replace_target = b;
                }
            }
            replace_block(set_index, replace_target, begin, tag);
        }
    }
    // For sw
    else
    {
        for (int b = 0; b < cache.blocksPerSet; b++)
        {
            if (cache.blocks[set_index][b].valid && cache.blocks[set_index][b].tag == tag)
            {
                printAction(b, 1, processorToCache);
                cache.blocks[set_index][b].lruLabel++;
                cache.blocks[set_index][b].data[block_offset] = write_data;
                return mem_access(addr, write_flag, write_data);
            }
        }

        // If not found in cache, access main memory and read a block of data into the cache
        int begin = addr & 0xFFFC;

        int no_empty_block = 1;
        for (int b = 0; b < cache.blocksPerSet; b++)
        {
            if (!cache.blocks[set_index][b].valid)
            {
                // Fill the block
                no_empty_block = 0;
                fill_block(set_index, b, begin, tag);
                cache.blocks[set_index][b].data[block_offset] = write_data;
            }
        }

        // If we don't find an empty block, invoke replacement policy (LRU)
        if (no_empty_block == 1)
        {
            int replace_target = 0, max_lru = 0;
            for (int b = 0; b < cache.blocksPerSet; b++)
            {
                if (cache.blocks[set_index][b].lruLabel > max_lru)
                {
                    max_lru = cache.blocks[set_index][b].lruLabel;
                    replace_target = b;
                }
            }

            replace_block(set_index, replace_target, begin, tag);
        }
    }

    return mem_access(addr, write_flag, write_data);
}

void fill_block(int set_index, int target, int addr, int tag)
{
    printAction(addr, cache.blockSize, memoryToCache);
    int begin = addr;

    for (int i = 0; i < cache.blockSize; i++)
    {
        cache.blocks[set_index][target].data[i] = mem_access(begin++, 0, 0);
    }

    cache.blocks[set_index][target].lruLabel = 0;
    cache.blocks[set_index][target].valid = 1;
    cache.blocks[set_index][target].tag = tag;
    cache.blocks[set_index][target].dirty = 0;
}

void replace_block(int set_index, int target, int addr, int tag)
{

    if (cache.blocks[set_index][target].dirty)
    {
        printAction(addr, cache.blockSize, cacheToMemory);
        write_back(set_index, target, addr);
    }

    printAction(addr, cache.blockSize, memoryToCache);
    fill_block(set_index, target, addr, tag);
}

void write_back(int set_index, int target, int addr)
{
    int begin = addr;

    for (int i = 0; i < cache.blockSize; i++)
    {
        mem_access(begin++, 1, cache.blocks[set_index][target].data[i]);
    }
}

/*
 * print end of run statistics like in the spec. This is not required,
 * but is very helpful in debugging.
 * This should be called once a halt is reached.
 * DO NOT delete this function, or else it won't compile.
 */
void printStats()
{
    printf("End of run statistics\n");
    printf(" total of % d instructions executed\n ", cache.num_instructions);
    printf(" $$$ Main memory words accessed: %d\n", cache.num_words_accessed);
    printf("hits: %d, misses: %d, writebacks: %d\n", cache.hits, cache.misses, cache.writebacks);
    printf("%d dirty memory blocks left\n", cache.num_dirty_left);
    return;
}

/*
 * Log the specifics of each cache action.
 *
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor
 *  -    processorToCache: writing data from the processor to the cache
 *  -    memoryToCache: reading data from the memory to the cache
 *  -    cacheToMemory: evicting cache data and writing it to the memory
 *  -    cacheToNowhere: evicting cache data and throwing it away
 */
void printAction(int address, int size, enum actionType type)
{
    printf("$$$ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor)
    {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache)
    {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache)
    {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory)
    {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere)
    {
        printf("from the cache to nowhere\n");
    }
}

/*
 * Prints the cache based on the configurations of the struct
 * This is for debugging only and is not graded, so you may
 * modify it, but that is not recommended.
 */
void printCache()
{
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set)
    {
        printf("\tset %i:\n", set);
        for (int s = 0; s < cache.numSets; s++)
        {
            for (int block = 0; block < cache.blocksPerSet; ++block)
            {
                printf("\t\t[ %i ]: {", block);
                for (int index = 0; index < cache.blockSize; ++index)
                {
                    printf(" %i", cache.blocks[s][block].data[index]);
                }
                printf(" }\n");
            }
        }
    }
    printf("end cache\n");
}