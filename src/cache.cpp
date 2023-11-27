//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.hpp"
using namespace std;
//
// TODO:Student Information
//
const char *studentName = "Suraj Sathya Prakash";
const char *studentID = "A59026390";
const char *email = "ssathyaprakash@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;      // Number of sets in the I$
uint32_t icacheAssoc;     // Associativity of the I$
uint32_t icacheBlocksize; // Blocksize of the I$
uint32_t icacheHitTime;   // Hit Time of the I$

uint32_t dcacheSets;      // Number of sets in the D$
uint32_t dcacheAssoc;     // Associativity of the D$
uint32_t dcacheBlocksize; // Blocksize of the D$
uint32_t dcacheHitTime;   // Hit Time of the D$

uint32_t l2cacheSets;      // Number of sets in the L2$
uint32_t l2cacheAssoc;     // Associativity of the L2$
uint32_t l2cacheBlocksize; // Blocksize of the L2$
uint32_t l2cacheHitTime;   // Hit Time of the L2$
uint32_t inclusive;        // Indicates if the L2 is inclusive

uint32_t prefetch; // Indicate if prefetching is enabled

uint32_t memspeed; // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;      // I$ references
uint64_t icacheMisses;    // I$ misses
uint64_t icachePenalties; // I$ penalties

uint64_t dcacheRefs;      // D$ references
uint64_t dcacheMisses;    // D$ misses
uint64_t dcachePenalties; // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

uint64_t compulsory_miss; // Compulsory misses on all caches
uint64_t other_miss; // Other misses (Conflict / Capacity miss) on all caches

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

// uint8_t *icache;

cacheLine icacheLine;
vector<cacheLine> icacheSet(icacheAssoc);
vector<vector<cacheLine>> icache(icacheSets);

cacheLine l2cacheLine;
vector<cacheLine> l2cacheSet(l2cacheAssoc);
vector<vector<cacheLine>> l2cache(l2cacheSets);

cacheLine dcacheLine;
vector<cacheLine> dcacheSet(dcacheAssoc);
vector<vector<cacheLine>> dcache(dcacheSets);

//
// TODO: Add your Cache data structures here
//

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
vector<vector<cacheLine>> init_cacheMem(int cacheTags, int cacheSets,
                                        int cacheBlocks, int cacheAssoc) {
  int i, j;
  vector<vector<cacheLine>> cache(1 << cacheSets);
  vector<cacheLine> cacheSet(cacheAssoc);

  vector<uint8_t> temptag(cacheTags, 0);
  vector<uint8_t> tempindex(cacheSets, 0);
  vector<uint8_t> tempoffset(cacheBlocks, 0);
  cacheLine tempCacheLine;
  tempCacheLine.index = tempindex;
  tempCacheLine.offset = tempoffset;
  tempCacheLine.tag = temptag;
  tempCacheLine.age = 0;

  for (i = 0; i < 1 << cacheSets; i++) {
    cache[i] = cacheSet;
    for (j = 0; j < cacheAssoc; j++) {
      cache[i][j].index = tempindex;
      cache[i][j].offset = tempoffset;
      cache[i][j].tag = temptag;
      cache[i][j].age = j;
      cache[i][j].valid = 0;
    }
  }
  return cache;
}

void init_cache() {
  // Initialize cache stats
  icacheRefs = 0;
  icacheMisses = 0;
  icachePenalties = 0;
  dcacheRefs = 0;
  dcacheMisses = 0;
  dcachePenalties = 0;
  l2cacheRefs = 0;
  l2cacheMisses = 0;
  l2cachePenalties = 0;

  compulsory_miss = 0;
  other_miss = 0;
  int i = 0, j;

  icache = init_cacheMem(32 - log2(icacheSets) - log2(icacheBlocksize),
                         log2(icacheSets), log2(icacheBlocksize), icacheAssoc);
  l2cache =
      init_cacheMem(32 - log2(l2cacheSets) - log2(l2cacheBlocksize),
                    log2(l2cacheSets), log2(l2cacheBlocksize), l2cacheAssoc);
  dcache = init_cacheMem(32 - log2(dcacheSets) - log2(dcacheBlocksize),
                         log2(dcacheSets), log2(dcacheBlocksize), dcacheAssoc);
}

// Clean Up the Cache Hierarchy
//
void clean_cache() {
  //
  // free(icache);
  // TODO: Clean Up Cache Simulator Data Structures
  //
}
void cacheUpdate(int updatestatus, vector<cacheLine> *cacheSet,
                 cacheLine incomCacheLine, int cacheAssoc) {
  int i = 0, mark = -1;
  if (updatestatus == -1) {
    for (i = 0; i < cacheAssoc; i++) {
      if ((*cacheSet)[i].age == 0) {
        mark = i;
        (*cacheSet)[i].tag = incomCacheLine.tag;
        (*cacheSet)[i].valid = 1;
        break;
      }
    }

  } else {
    // (*cacheSet)[updatestatus].tag = incomCacheLine.tag;
    mark = updatestatus;
  }
  int prevAge = (*cacheSet)[mark].age;
  for (i = 0; i < cacheAssoc; i++) {
    if (mark == i)
      (*cacheSet)[i].age = cacheAssoc - 1;
    else if (prevAge < (*cacheSet)[i].age)
      (*cacheSet)[i].age--;
    else
      continue;
  }
}

// Perform a memory accesswH through the icache interface for the address
// 'addr' Return the access time for the memory operation
//
uint32_t icache_access(uint32_t addr) {

  uint32_t penalty = 0;
  int i, hitFlag = 0;
  icacheRefs++;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(icacheSets) - log2(icacheBlocksize),
                      log2(icacheSets), log2(icacheBlocksize));
  int indexInt = BoolVect2Int(incomCacheLine.index);
  for (i = 0; i < icacheAssoc; i++) {
    if (icache[indexInt][i].tag == incomCacheLine.tag &&
        icache[indexInt][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &icache[indexInt], incomCacheLine, icacheAssoc);
      break;
    }
  }
  if (hitFlag == 0) {
    penalty += l2cache_access(addr);
    icacheMisses++;
    icachePenalties += penalty;
    cacheUpdate(-1, &icache[indexInt], incomCacheLine, icacheAssoc);
  }
  return penalty + icacheHitTime;
}

// Perform a memory access through the dcache interface for the address
// 'addr' Return the access time for the memory operation
//
uint32_t dcache_access(uint32_t addr) {
  uint32_t penalty = 0;
  int i, hitFlag = 0;
  dcacheRefs++;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(dcacheSets) - log2(dcacheBlocksize),
                      log2(dcacheSets), log2(dcacheBlocksize));
  int indexInt = BoolVect2Int(incomCacheLine.index);
  for (i = 0; i < dcacheAssoc; i++) {
    if (dcache[indexInt][i].tag == incomCacheLine.tag &&
        dcache[indexInt][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &dcache[indexInt], incomCacheLine, dcacheAssoc);
      break;
    }
  }
  if (hitFlag == 0) {
    penalty += l2cache_access(addr);
    dcacheMisses++;
    dcachePenalties += penalty;
    cacheUpdate(-1, &dcache[indexInt], incomCacheLine, dcacheAssoc);
  }
  return penalty + dcacheHitTime;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t l2cache_access(uint32_t addr) {
  uint32_t penalty = 0;
  int i, hitFlag = 0;
  l2cacheRefs++;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(l2cacheSets) - log2(l2cacheBlocksize),
                      log2(l2cacheSets), log2(l2cacheBlocksize));
  int indexInt = BoolVect2Int(incomCacheLine.index);
  for (i = 0; i < l2cacheAssoc; i++) {
    if (l2cache[indexInt][i].tag == incomCacheLine.tag &&
        l2cache[indexInt][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &l2cache[indexInt], incomCacheLine, l2cacheAssoc);
      break;
    }
  }

  if (hitFlag == 0) {
    penalty += memspeed;
    l2cacheMisses++;
    l2cachePenalties += penalty;
    cacheUpdate(-1, &l2cache[indexInt], incomCacheLine, l2cacheAssoc);
  }
  return penalty + l2cacheHitTime;
}

// Predict an address to prefetch on icache with the information of last
// icache access: 'pc':     Program Counter of the instruction of last
// icache access 'addr':   Accessed Address of last icache access 'r_or_w':
// Read/Write of last icache access
uint32_t icache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w) {
  return addr + icacheBlocksize; // Next line prefetching
  //
  // TODO: Implement a better prefetching strategy
  //
}

// Predict an address to prefetch on dcache with the information of last
// dcache access: 'pc':     Program Counter of the instruction of last
// dcache access 'addr':   Accessed Address of last dcache access 'r_or_w':
// Read/Write of last dcache access
uint32_t dcache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w) {
  return addr + dcacheBlocksize; // Next line prefetching
  //
  // TODO: Implement a better prefetching strategy
  //
}

// Perform a prefetch operation to I$ for the address 'addr'
void icache_prefetch(uint32_t addr) {

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(icacheSets) - log2(icacheBlocksize),
                      log2(icacheSets), log2(icacheBlocksize));
  int indexInt = BoolVect2Int(incomCacheLine.index);

  cacheUpdate(-1, &icache[indexInt], incomCacheLine, icacheAssoc);

  // //
  // // TODO: Implement I$ prefetch operation
  // //
}

// Perform a prefetch operation to D$ for the address 'addr'
void dcache_prefetch(uint32_t addr) {

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(dcacheSets) - log2(dcacheBlocksize),
                      log2(dcacheSets), log2(dcacheBlocksize));
  int indexInt = BoolVect2Int(incomCacheLine.index);

  cacheUpdate(-1, &dcache[indexInt], incomCacheLine, dcacheAssoc);
  //
  // TODO: Implement D$ prefetch operation
  //
}

int log2(uint32_t number) {
  int exp = 0;
  while ((number >> 1) > 1) {
    number = number >> 1;
    exp = exp + 1;
  }
  return exp + 1;
}

cacheLine AddrToCacheLine(uint32_t addr, int tagSize, int indexSize,
                          int blockSize) {
  bool bit;
  cacheLine tempCacheLine;

  vector<uint8_t> temptag(tagSize, 0);
  vector<uint8_t> tempindex(indexSize, 0);
  vector<uint8_t> tempoffset(blockSize, 0);

  tempCacheLine.tag = temptag;
  tempCacheLine.index = tempindex;
  tempCacheLine.offset = tempoffset;

  for (int i = 0; i < 32; i++) {
    bit = (addr >> i) & 1;
    if (i < blockSize && i >= 0) {
      tempCacheLine.offset[i] = bit;
    } else if (i >= blockSize && i < indexSize + blockSize) {
      tempCacheLine.index[i - blockSize] = bit;
    } else {
      tempCacheLine.tag[i - blockSize - indexSize] = bit;
    }
    tempCacheLine.age = 0;
  }
  return tempCacheLine;
}

uint32_t BoolVect2Int(vector<uint8_t> boolVect) {
  uint32_t intSum = 0;
  for (int i = 0; i < boolVect.size(); i++) {
    if (boolVect[i])
      intSum += 1 << i;
  }

  return intSum;
}
