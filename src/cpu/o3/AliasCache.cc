
#include "cpu/o3/isa_specific.hh"
#include "cpu/o3/AliasCache_impl.hh"

template class LRUAliasCache<O3CPUImpl>;
template class LRUVictimCache<O3CPUImpl>;