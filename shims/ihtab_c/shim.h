// c_cpp_hash_tables_benchmark/shims/ihtab_c/shim.h

#include "ihtab.h"

template< typename blueprint > struct ihtab_c
{
  struct entry
  {
    typename blueprint::key_type key;
    typename blueprint::value_type value;
  };

  static ihtab_hash_t hash_fn( const entry e )
  {
    return blueprint::hash_key( e.key );
  }

  static bool eq_fn( const entry e1, const entry e2 )
  {
    return blueprint::cmpr_keys( e1.key, e2.key );
  }

  // Generate the ihtab types and functions
  DEFINE_IHTAB(entry, hash_fn, eq_fn)

  using table_type = struct ihtab_entry;
  using itr_type = struct ihtab_iter_entry;

  static table_type create_table()
  {
    table_type table;
    ihtab_create_entry( &table, 8 );
    return table;
  }

  static itr_type find( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    bool found = ihtab_perform_entry( &table, &temp, IHTAB_C_FIND, &res );
    itr_type itr;
    if( found )
    {
      // Find the element index for the iterator
      itr.ptr = res;
      itr.el_idx = res - table.bin.els;
    }
    else
    {
      itr.ptr = nullptr;
      itr.el_idx = table.bin.els_bound;
    }
    return itr;
  }

  static void insert( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    temp.value = typename blueprint::value_type();
    entry *res;
    bool found = ihtab_perform_entry( &table, &temp, IHTAB_C_INSERT, &res );
    if( !found )
      *res = temp;
  }

  static void erase( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    ihtab_perform_entry( &table, &temp, IHTAB_C_DELETE, &res );
  }

  static itr_type begin_itr( table_type &table )
  {
    return ihtab_iter_begin_entry( &table );
  }

  static bool is_itr_valid( table_type &table, itr_type &itr )
  {
    return ihtab_iter_valid_entry( &itr );
  }

  static void increment_itr( table_type &table, itr_type &itr )
  {
    ihtab_iter_next_entry( &table, &itr );
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
    ihtab_destroy_entry( &table );
  }
};

template<> struct ihtab_c< void >
{
  static constexpr const char *label = "ihtab_c";
  static constexpr const char *color = "rgb( 30, 100, 180 )";
  static constexpr bool tombstone_like_mechanism = true;
};