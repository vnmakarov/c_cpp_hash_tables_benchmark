// c_cpp_hash_tables_benchmark/shims/ehtab/shim.h

#include "varr.h"
#include "ehtab.h"

template< typename > struct ehtab
{
  static constexpr const char *label = "ehtab";
  static constexpr const char *color = "rgb( 50, 180, 100 )";
  static constexpr bool tombstone_like_mechanism = true;
};

struct ehtab_itr
{
  size_t bin_idx;
  htab_size_t el_idx;
  void *entry_ptr;
};

#define EHTAB_SPECIALIZATION( blueprint )                                                          \
                                                                                                   \
struct ehtab_##blueprint##_entry                                                                   \
{                                                                                                  \
  blueprint::key_type key;                                                                         \
  blueprint::value_type value;                                                                     \
};                                                                                                 \
                                                                                                   \
static htab_hash_t ehtab_##blueprint##_hash( void *el, void *arg )                                 \
{                                                                                                  \
  return (htab_hash_t) blueprint::hash_key( ((ehtab_##blueprint##_entry *)el)->key );              \
}                                                                                                  \
                                                                                                   \
static int ehtab_##blueprint##_eq( void *el1, void *el2, void *arg )                               \
{                                                                                                  \
  return blueprint::cmpr_keys( ((ehtab_##blueprint##_entry *)el1)->key,                            \
                               ((ehtab_##blueprint##_entry *)el2)->key );                          \
}                                                                                                  \
                                                                                                   \
EHTAB_INIT(blueprint, ehtab_##blueprint##_hash, ehtab_##blueprint##_eq)                            \
                                                                                                   \
template<> struct ehtab< blueprint >                                                               \
{                                                                                                  \
  using table_type = htab_t;                                                                       \
  using entry_type = ehtab_##blueprint##_entry;                                                    \
                                                                                                   \
  static table_type create_table()                                                                 \
  {                                                                                                \
    table_type table;                                                                              \
    _htab_create( &table, sizeof( entry_type ), 8,                                                 \
                  ehtab_##blueprint##_hash, ehtab_##blueprint##_eq, NULL, NULL );                   \
    return table;                                                                                  \
  }                                                                                                \
                                                                                                   \
  static void advance_itr( table_type &table, ehtab_itr &itr )                                    \
  {                                                                                                \
    size_t bins_num = VARR_LENGTH( hbin_t, table.bins );                                           \
    hbin_t *bins_addr = VARR_ADDR( hbin_t, table.bins );                                          \
    while( itr.bin_idx < bins_num )                                                                \
    {                                                                                              \
      hbin_t *bin = &bins_addr[itr.bin_idx];                                                       \
      htab_hash_t *hashes_addr = VARR_ADDR( htab_hash_t, bin->hashes );                           \
      while( itr.el_idx < bin->els_bound )                                                         \
      {                                                                                            \
        if( hashes_addr[itr.el_idx] != HTAB_DELETED_HASH )                                         \
        {                                                                                          \
          char *els_addr = (char *)_varr_addr( &bin->els );                                        \
          itr.entry_ptr = els_addr + itr.el_idx * sizeof( entry_type );                            \
          return;                                                                                  \
        }                                                                                          \
        ++itr.el_idx;                                                                              \
      }                                                                                            \
      ++itr.bin_idx;                                                                               \
      if( itr.bin_idx < bins_num )                                                                 \
        itr.el_idx = bins_addr[itr.bin_idx].els_start;                                             \
    }                                                                                              \
    itr.entry_ptr = NULL;                                                                          \
  }                                                                                                \
                                                                                                   \
  static ehtab_itr find( table_type &table, const blueprint::key_type &key )                       \
  {                                                                                                \
    entry_type temp;                                                                               \
    temp.key = key;                                                                                \
    void *res;                                                                                     \
    bool found = _htab_do_##blueprint(&table, sizeof( entry_type ), &temp, HTAB_FIND, &res );                 \
    ehtab_itr itr;                                                                                 \
    itr.entry_ptr = found ? res : NULL;                                                            \
    itr.bin_idx = 0;                                                                               \
    itr.el_idx = 0;                                                                                \
    return itr;                                                                                    \
  }                                                                                                \
                                                                                                   \
  static void insert( table_type &table, const blueprint::key_type &key )                          \
  {                                                                                                \
    entry_type temp;                                                                               \
    temp.key = key;                                                                                \
    temp.value = blueprint::value_type();                                                          \
    void *res;                                                                                     \
    bool found = _htab_do_##blueprint(&table, sizeof( entry_type ), &temp, HTAB_INSERT, &res );               \
    if( !found )                                                                                   \
      *(entry_type *)res = temp;                                                                   \
  }                                                                                                \
                                                                                                   \
  static void erase( table_type &table, const blueprint::key_type &key )                           \
  {                                                                                                \
    entry_type temp;                                                                               \
    temp.key = key;                                                                                \
    void *res;                                                                                     \
    _htab_do_##blueprint(&table, sizeof( entry_type ), &temp, HTAB_DELETE, &res );                            \
  }                                                                                                \
                                                                                                   \
  static ehtab_itr begin_itr( table_type &table )                                                  \
  {                                                                                                \
    ehtab_itr itr;                                                                                 \
    itr.bin_idx = 0;                                                                               \
    size_t bins_num = VARR_LENGTH( hbin_t, table.bins );                                           \
    hbin_t *bins_addr = VARR_ADDR( hbin_t, table.bins );                                          \
    itr.el_idx = bins_num > 0 ? bins_addr[0].els_start : 0;                                       \
    itr.entry_ptr = NULL;                                                                          \
    advance_itr( table, itr );                                                                     \
    return itr;                                                                                    \
  }                                                                                                \
                                                                                                   \
  static bool is_itr_valid( table_type &table, ehtab_itr &itr )                                   \
  {                                                                                                \
    return itr.entry_ptr != NULL;                                                                  \
  }                                                                                                \
                                                                                                   \
  static void increment_itr( table_type &table, ehtab_itr &itr )                                  \
  {                                                                                                \
    ++itr.el_idx;                                                                                  \
    advance_itr( table, itr );                                                                     \
  }                                                                                                \
                                                                                                   \
  static const blueprint::key_type &get_key_from_itr( table_type &table, ehtab_itr &itr )         \
  {                                                                                                \
    return ((entry_type *)itr.entry_ptr)->key;                                                     \
  }                                                                                                \
                                                                                                   \
  static const blueprint::value_type &get_value_from_itr( table_type &table, ehtab_itr &itr )     \
  {                                                                                                \
    return ((entry_type *)itr.entry_ptr)->value;                                                   \
  }                                                                                                \
                                                                                                   \
  static void destroy_table( table_type &table )                                                   \
  {                                                                                                \
    _htab_destroy( &table, sizeof( entry_type ) );                                                 \
  }                                                                                                \
};                                                                                                 \

#ifdef UINT32_UINT32_MURMUR_ENABLED
EHTAB_SPECIALIZATION( uint32_uint32_murmur )
#endif

#ifdef UINT64_STRUCT448_MURMUR_ENABLED
EHTAB_SPECIALIZATION( uint64_struct448_murmur )
#endif

#ifdef CSTRING_UINT64_FNV1A_ENABLED
EHTAB_SPECIALIZATION( cstring_uint64_fnv1a )
#endif
