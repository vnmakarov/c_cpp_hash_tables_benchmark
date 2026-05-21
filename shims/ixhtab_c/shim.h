// c_cpp_hash_tables_benchmark/shims/ixhtab_c/shim.h

#include "ixht.h"

template< typename blueprint > struct ixhtab_c
{
  struct entry
  {
    typename blueprint::key_type key;
    typename blueprint::value_type value;
  };

  static ixht_hash_t hash_fn( const entry e )
  {
    return blueprint::hash_key( e.key );
  }

  static bool eq_fn( const entry e1, const entry e2 )
  {
    return blueprint::cmpr_keys( e1.key, e2.key );
  }

  // Generate the ixhtab types and functions
  DEFINE_IXHT(entry, hash_fn, eq_fn)

  using table_type = struct ixht_entry;
  using itr_type = struct ixht_iter_entry;

  static table_type create_table()
  {
    table_type table;
    ixht_create_entry( &table, 8 );
    return table;
  }

  static itr_type find( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    bool found = ixht_perform_entry( &table, &temp, IXHT_FIND, &res );
    itr_type itr;
    if( found )
    {
      // Find the element for the iterator
      itr.ptr = res;
      // Find which bin and element index this corresponds to
      for( unsigned int bin_idx = 0; bin_idx < table.bins_num; bin_idx++ )
      {
        auto &bin = table.bins[bin_idx];
        if( res >= bin.els && res < bin.els + bin.els_bound )
        {
          itr.bin_idx = bin_idx;
          itr.el_idx = res - bin.els;
          break;
        }
      }
    }
    else
    {
      itr.ptr = nullptr;
      itr.bin_idx = table.bins_num;
      itr.el_idx = 0;
    }
    return itr;
  }

  static void insert( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    temp.value = typename blueprint::value_type();
    entry *res;
    bool found = ixht_perform_entry( &table, &temp, IXHT_INSERT, &res );
    if( !found )
      *res = temp;
  }

  static void erase( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    ixht_perform_entry( &table, &temp, IXHT_DELETE, &res );
  }

  static itr_type begin_itr( table_type &table )
  {
    return ixht_iter_begin_entry( &table );
  }

  static bool is_itr_valid( table_type &table, itr_type &itr )
  {
    return ixht_iter_valid_entry( &itr );
  }

  static void increment_itr( table_type &table, itr_type &itr )
  {
    ixht_iter_next_entry( &table, &itr );
  }

  static const typename blueprint::key_type &get_key_from_itr( table_type &table, itr_type &itr )
  {
    return itr.ptr->key;
  }

  static const typename blueprint::value_type &get_value_from_itr( table_type &table, itr_type &itr )
  {
    return itr.ptr->value;
  }

  static void destroy_table( table_type &table )
  {
    ixht_destroy_entry( &table );
  }
};

template<> struct ixhtab_c< void >
{
  static constexpr const char *label = "ixhtab_c";
  static constexpr const char *color = "rgb( 150, 80, 200 )";
  static constexpr bool tombstone_like_mechanism = true;
};
