//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.hpp"
using namespace std;

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

strideStruct icacheStride;
strideStruct dcacheStride;
strideStruct l2cacheStride;
vector<int32_t> icacheTagAccess;
vector<int32_t> dcacheTagAccess;
vector<int32_t> l2cacheTagAccess;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

bool tagLookup(int cacheType, uint32_t tag) {
  if (cacheType == 0) {
    auto found0 = find(icacheTagAccess.begin(), icacheTagAccess.end(), tag);
    if (found0 != icacheTagAccess.end())
      return 0;
    else
      icacheTagAccess.push_back(tag);
  } else if (cacheType == 1) {
    auto found1 = find(dcacheTagAccess.begin(), dcacheTagAccess.end(), tag);
    if (found1 != dcacheTagAccess.end())
      return 0;
    else
      dcacheTagAccess.push_back(tag);
  } else {
    auto found2 = find(l2cacheTagAccess.begin(), l2cacheTagAccess.end(), tag);
    if (found2 != l2cacheTagAccess.end())
      return 0;
    else
      l2cacheTagAccess.push_back(tag);
  }
  return 1;
}
vector<vector<cacheLine>> init_cacheMem(int cacheTags, int cacheSets,
                                        int cacheBlocks, int cacheAssoc) {
  int i, j;
  vector<vector<cacheLine>> cache(1 << cacheSets);
  vector<cacheLine> cacheSet(cacheAssoc);

  for (i = 0; i < 1 << cacheSets; i++) {
    cache[i] = cacheSet;
    for (j = 0; j < cacheAssoc; j++) {
      cache[i][j].index = 0;
      cache[i][j].offset = 0;
      cache[i][j].tag = 0;
      cache[i][j].age = j;
      cache[i][j].valid = 0;
    }
  }
  return cache;
}
strideStruct initStrideDetector() {
  strideStruct tempStrideStruct;
  tempStrideStruct.addrt_1 = 0;
  tempStrideStruct.addrt_2 = 0;
  tempStrideStruct.addrt_3 = 0;
  tempStrideStruct.addrt_4 = 0;
  tempStrideStruct.difft = 0;
  tempStrideStruct.difft_1 = 0;
  tempStrideStruct.difft_2 = 0;
  tempStrideStruct.difft_3 = 0;
  tempStrideStruct.count = 0;

  return tempStrideStruct;
}
void strideCalculator(strideStruct *tempStrideStruct, uint32_t addr) {
  tempStrideStruct->strideFlag = 0;

  (*tempStrideStruct).difft_3 =
      (*tempStrideStruct).addrt_3 - (*tempStrideStruct).addrt_4;
  (*tempStrideStruct).difft_2 =
      (*tempStrideStruct).addrt_2 - (*tempStrideStruct).addrt_3;
  (*tempStrideStruct).difft_1 =
      (*tempStrideStruct).addrt_1 - (*tempStrideStruct).addrt_2;
  (*tempStrideStruct).difft = addr - (*tempStrideStruct).addrt_1;

  (*tempStrideStruct).addrt_4 = (*tempStrideStruct).addrt_3;
  (*tempStrideStruct).addrt_3 = (*tempStrideStruct).addrt_2;
  (*tempStrideStruct).addrt_2 = (*tempStrideStruct).addrt_1;
  (*tempStrideStruct).addrt_1 = addr;

  if (((*tempStrideStruct).difft_1 - (*tempStrideStruct).difft_2 == 0) &&
      ((*tempStrideStruct).difft - (*tempStrideStruct).difft_1 == 0)) {
    tempStrideStruct->stride = (*tempStrideStruct).difft;
    tempStrideStruct->strideFlag = 1;
  }
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

  icacheStride = initStrideDetector();
  dcacheStride = initStrideDetector();
  l2cacheStride = initStrideDetector();
}

// Clean Up the Cache Hierarchy
void clean_cache() {}

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
uint32_t icache_access(uint32_t addr) {

  uint32_t penalty = 0;
  int i, hitFlag = 0;
  icacheRefs++;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(icacheSets) - log2(icacheBlocksize),
                      log2(icacheSets), log2(icacheBlocksize));
  for (i = 0; i < icacheAssoc; i++) {
    if (icache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        icache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &icache[incomCacheLine.index], incomCacheLine,
                  icacheAssoc);
      break;
    }
  }
  if (hitFlag == 0) {
    compulsory_miss += tagLookup(0, incomCacheLine.tag);
    penalty += l2cache_access(addr);
    icacheMisses++;
    icachePenalties += penalty;
    cacheUpdate(-1, &icache[incomCacheLine.index], incomCacheLine, icacheAssoc);
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
  for (i = 0; i < dcacheAssoc; i++) {
    if (dcache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        dcache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &dcache[incomCacheLine.index], incomCacheLine,
                  dcacheAssoc);
      break;
    }
  }
  if (hitFlag == 0) {
    compulsory_miss += tagLookup(1, incomCacheLine.tag);
    penalty += l2cache_access(addr);
    dcacheMisses++;
    dcachePenalties += penalty;
    cacheUpdate(-1, &dcache[incomCacheLine.index], incomCacheLine, dcacheAssoc);
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
  for (i = 0; i < l2cacheAssoc; i++) {
    if (l2cache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        l2cache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      cacheUpdate(i, &l2cache[incomCacheLine.index], incomCacheLine,
                  l2cacheAssoc);
      break;
    }
  }

  if (hitFlag == 0) {
    compulsory_miss += tagLookup(2, incomCacheLine.tag);
    penalty += memspeed;
    l2cacheMisses++;
    l2cachePenalties += penalty;
    cacheUpdate(-1, &l2cache[incomCacheLine.index], incomCacheLine,
                l2cacheAssoc);
  }
  return penalty + l2cacheHitTime;
}

// Predict an address to prefetch on icache with the information of last
// icache access: 'pc':     Program Counter of the instruction of last
// icache access 'addr':   Accessed Address of last icache access 'r_or_w':
// Read/Write of last icache access
uint32_t icache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w) {
  strideCalculator(&icacheStride, addr);
  if (icacheStride.strideFlag) {
    icacheStride.count++;
    return addr + icacheStride.stride; // Stride prefetching
  } else
    return addr + icacheBlocksize; // Next line prefetching
}

// Predict an address to prefetch on dcache with the information of last
// dcache access: 'pc':     Program Counter of the instruction of last
// dcache access 'addr':   Accessed Address of last dcache access 'r_or_w':
// Read/Write of last dcache access
uint32_t dcache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w) {
  strideCalculator(&dcacheStride, addr);
  if (dcacheStride.strideFlag) {
    dcacheStride.count++;
    return addr + dcacheStride.stride; // Stride prefetching
  } else
    return addr + dcacheBlocksize; // Next line prefetching
}

uint32_t l2cache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w) {
  strideCalculator(&l2cacheStride, addr);
  if (l2cacheStride.strideFlag) {
    l2cacheStride.count++;
    return addr + l2cacheStride.stride; // Stride prefetching
  } else
    return addr + l2cacheBlocksize; // Next line prefetching
}

// Perform a prefetch operation to I$ for the address 'addr'
void icache_prefetch(uint32_t addr) {
  int i, hitFlag = 0;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(icacheSets) - log2(icacheBlocksize),
                      log2(icacheSets), log2(icacheBlocksize));
  for (i = 0; i < icacheAssoc; i++) {
    if (icache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        icache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      break;
    }
  }
  if (hitFlag == 0) {
    cacheUpdate(-1, &icache[incomCacheLine.index], incomCacheLine, icacheAssoc);
  }
}

// Perform a prefetch operation to D$ for the address 'addr'
void dcache_prefetch(uint32_t addr) {

  int i, hitFlag = 0;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(dcacheSets) - log2(dcacheBlocksize),
                      log2(dcacheSets), log2(dcacheBlocksize));
  for (i = 0; i < dcacheAssoc; i++) {
    if (dcache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        dcache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      break;
    }
  }
  if (hitFlag == 0) {
    cacheUpdate(-1, &dcache[incomCacheLine.index], incomCacheLine, dcacheAssoc);
  }
}

void l2cache_prefetch(uint32_t addr) {

  int i, hitFlag = 0;

  cacheLine incomCacheLine =
      AddrToCacheLine(addr, 32 - log2(l2cacheSets) - log2(l2cacheBlocksize),
                      log2(l2cacheSets), log2(l2cacheBlocksize));
  for (i = 0; i < l2cacheAssoc; i++) {
    if (l2cache[incomCacheLine.index][i].tag == incomCacheLine.tag &&
        l2cache[incomCacheLine.index][i].valid == 1) {
      hitFlag = 1;
      break;
    }
  }
  if (hitFlag == 0) {
    cacheUpdate(-1, &l2cache[incomCacheLine.index], incomCacheLine,
                l2cacheAssoc);
  }
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

  tempCacheLine.tag = 0;
  tempCacheLine.index = 0;
  tempCacheLine.offset = 0;

  uint32_t offsetMask = (1 << blockSize) - 1;
  uint32_t indexMask = ((1 << indexSize) - 1) << blockSize;
  uint32_t tagMask = ((1 << tagSize) - 1) << (blockSize + indexSize);

  tempCacheLine.tag |= (addr & tagMask) >> (blockSize + indexSize);
  tempCacheLine.index |= (addr & indexMask) >> blockSize;
  tempCacheLine.offset |= addr & offsetMask;
  tempCacheLine.age = 0;

  return tempCacheLine;
}
