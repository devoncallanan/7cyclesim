/* This file contains a rough implementation of an L1 cache in the absence of an L2 cache*/
#include <stdlib.h>
#include <stdio.h>

unsigned int L2_accesses = 0;
unsigned int L2_hits = 0;
unsigned int L2_misses = 0;


struct cache_blk_t { // note that no actual data will be stored in the cache 
  unsigned long tag;
  char valid;
  char dirty;
  unsigned LRU;	//to be used to build the LRU stack for the blocks in a cache set
};

struct cache_t {
	// The cache is represented by a 2-D array of blocks. 
	// The first dimension of the 2D array is "nsets" which is the number of sets (entries)
	// The second dimension is "assoc", which is the number of blocks in each set.
  int nsets;					// number of sets
  int blocksize;				// block size
  int assoc;					// associativity
  int mem_latency;				// the miss penalty
  
  //-------------------------------------------------------------------------------------------
  //unsigned int accesses;
  //unsigned int hits;
  //unsigned int misses;
  
  struct cache_blk_t **blocks;	// a pointer to the array of cache blocks
};

struct cache_t * cache_create(int size, int blocksize, int assoc, int mem_latency)
{
  int i, nblocks , nsets ;
  struct cache_t *C = (struct cache_t *)calloc(1, sizeof(struct cache_t));
		
  nblocks = size *1024 / blocksize ;// number of blocks in the cache
  nsets = nblocks / assoc ;			// number of sets (entries) in the cache
  C->blocksize = blocksize ;
  C->nsets = nsets  ; 
  C->assoc = assoc;
  C->mem_latency = mem_latency;

  C->blocks= (struct cache_blk_t **)calloc(nsets, sizeof(struct cache_blk_t *));

  for(i = 0; i < nsets; i++) {
		C->blocks[i] = (struct cache_blk_t *)calloc(assoc, sizeof(struct cache_blk_t));
	}
  return C;
}
//------------------------------

int updateLRU(struct cache_t *cp ,int index, int way)
{
	int k ;
	for (k=0 ; k< cp->assoc ; k++) 
	{
	  if(cp->blocks[index][k].LRU < cp->blocks[index][way].LRU) 
		 cp->blocks[index][k].LRU = cp->blocks[index][k].LRU + 1 ;
	}
	cp->blocks[index][way].LRU = 0 ;
}

int cache_access(struct cache_t *L1, struct cache_t *L2, unsigned long address, int access_type /*0 for read, 1 for write*/)
{
	
	
	int flag = 0;
	int i,latency ;
	//L1
	int L1_block_address ;
	int L1_index ;
	int L1_tag ;
	//L2
	int L2_block_address ;
	int L2_index ;
	int L2_tag ;
	
	int way ;
	int max ;
	
	//Extras for rehashing to keep "inclusive"
	unsigned long rehash_address;
	int temp_block_address;
	int temp_tag;
	int temp_index;
	int temp;

	L1_block_address = (address / L1->blocksize);
	L1_tag = L1_block_address / L1->nsets;
	L1_index = L1_block_address - (L1_tag * L1->nsets) ;
	
	if (L2->nsets != 0) {
		L2_block_address = (address / L2->blocksize);
		L2_tag = L2_block_address / L2->nsets;
		L2_index = L2_block_address - (L2_tag * L2->nsets) ;
	}

	latency = 0;
	
	//L1 CACHE HIT------------------------------------------------------------------------------------------------
	for (i = 0; i < L1->assoc; i++) {	/* look for the requested block */
		if (L1->blocks[L1_index][i].tag == L1_tag && L1->blocks[L1_index][i].valid == 1) {
			updateLRU(L1, L1_index, i) ;
			if (access_type == 1) {
				L1->blocks[L1_index][i].dirty = 1 ;
			}
			return(latency);					/* a cache hit */
		}
	}
	
	//L1 CACHE MISS------------------------------------------------------------------------------------------------
	/* a cache miss */
	for (way=0 ; way < L1->assoc ; way++){		/* look for an invalid entry */
		if (L1->blocks[L1_index][way].valid == 0) {
			latency = latency + L1->mem_latency;	/* account for reading the block from memory*/
													/* should instead read from L2, in case you have an L2 */
			L2_accesses++;										
			L1->blocks[L1_index][way].valid = 1 ;
		    L1->blocks[L1_index][way].tag = L1_tag ;
		    updateLRU(L1, L1_index, way); 
		    L1->blocks[L1_index][way].dirty = 0;
			
		    if(access_type == 1) L1->blocks[L1_index][way].dirty = 1 ;
			
			flag = 1;//return(latency);				/* an invalid entry is available*/
		}
	}
	
	//Only execute if we couldn't find an open space
	if (flag == 0) {
		max = L1->blocks[L1_index][0].LRU ;	/* find the LRU block */
		way = 0 ;
		for (i=1 ; i< L1->assoc ; i++) {
			if (L1->blocks[L1_index][i].LRU > max) {
				max = L1->blocks[L1_index][i].LRU ;
				way = i ;
			}
		} 
		
		if (L1->blocks[L1_index][way].dirty == 1) {
			latency = latency + L1->mem_latency;	/* for writing back the evicted block */
		} 
		
		latency = latency + L1->mem_latency;		/* for reading the block from memory*/
													/* should instead write to and/or read from L2, in case you have an L2 */
		L1->blocks[L1_index][way].tag = L1_tag ;
		updateLRU(L1, L1_index, way) ;
		L1->blocks[L1_index][i].dirty = 0 ;
		if(access_type == 1) L1->blocks[L1_index][i].dirty = 1 ;
	}
	
	if (L2->nsets != 0) {
		//L2 HIT------------------------------------------------------------------------------------------------
		for (i = 0; i < L2->assoc; i++) {	/* look for the requested block */
			if (L2->blocks[L2_index][i].tag == L2_tag && L2->blocks[L2_index][i].valid == 1) {
				updateLRU(L2, L2_index, i) ;
				L2_hits++;
				if (access_type == 1) {
					L2->blocks[L2_index][i].dirty = 1 ;
				}
				return(latency);					/* a cache hit */ //IF THIS IS HIT THEN WE DONT RETURN LATENCY BUT JUMP TO L1 MISS INFO
			}
		}
		
		//L2 MISS------------------------------------------------------------------------------------------------
		for (way=0 ; way < L2->assoc ; way++){		/* look for an invalid entry */
			if (L2->blocks[L2_index][way].valid == 0) {
				latency = latency + L2->mem_latency;	/* account for reading the block from memory*/
														/* should instead read from L2, in case you have an L2 */
				L2_misses++;										
				L2->blocks[L2_index][way].valid = 1 ;
				L2->blocks[L2_index][way].tag = L2_tag ;
				updateLRU(L2, L2_index, way); 
				L2->blocks[L2_index][way].dirty = 0;
				
				if(access_type == 1) L2->blocks[L2_index][way].dirty = 1 ;
				
				return(latency);				/* an invalid entry is available*/ //JUMP TO L1 MISS
			}
		}
		
		max = L2->blocks[L2_index][0].LRU ;	/* find the LRU block */
		way = 0 ;
		
		for (i=1 ; i< L2->assoc ; i++) {
			//Find LRU in L2
			if (L2->blocks[L2_index][i].LRU > max) {
				//Rehash to check in L1
				rehash_address = (L2_index + (L2->blocks[L2_index][i].tag * L2->nsets) * L2->blocksize);
				temp_block_address = (rehash_address / L1->blocksize);
				temp_tag =  temp_block_address / L1->nsets;
				temp_index = temp_block_address - (temp_tag * L1->nsets);
				//Check LRU against L1 cache
				for (temp = 0; temp < L1->assoc; temp++) {
					//If LRU isn't in L1 then you can evict it
					if (L1->blocks[temp_index][temp].tag == temp_tag && L1->blocks[temp_index][temp].valid != 1) {
						max = L2->blocks[L2_index][i].LRU ;
						way = i ;
					}
				}
				
			}
		} 
		   
		
		//If dirty in L2, WRITE BACK
		if (L2->blocks[L2_index][way].dirty == 1) {
			latency = latency + L2->mem_latency;
		} 
		
		L2_misses++;
		latency = latency + L2->mem_latency;		/* for reading the block from memory*/
													/* should instead write to and/or read from L2, in case you have an L2 */
		L2->blocks[L2_index][way].tag = L2_tag ;
		updateLRU(L2, L2_index, way) ;
		L2->blocks[L2_index][i].dirty = 0 ;
		if(access_type == 1) L2->blocks[L2_index][i].dirty = 1 ;
	}
	
	
	//------------------------------------------------------------------------------------------------------------
	
	return(latency) ;
}

