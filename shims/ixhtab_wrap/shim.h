// c_cpp_hash_tables_benchmark/shims/ixhtab_cpp/shim.h

#include "ixhtab.hpp"

template< typename blueprint > struct ixhtab_wrap
{
  struct entry
  {
    typename blueprint::key_type key;
    typename blueprint::value_type value;
  };

  struct hash
  {
    ixhtab_hash_t operator()( const entry &e ) const
    {
      return blueprint::hash_key( e.key );
    }
  };

  struct eq
  {
    bool operator()( const entry &e1, const entry &e2 ) const
    {
      return blueprint::cmpr_keys( e1.key, e2.key );
    }
  };

  using tab = ixhtab< entry, hash, eq >;
  using table_type = tab;
  using itr_type = typename tab::ixhtab_iter;

  static table_type create_table()
  {
    return table_type( 8 );
  }

  static itr_type find( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    bool found = table.perform( temp, IXHTAB_FIND, &res );

    // Create iterator directly from result - no search needed
    itr_type it;
    it.ptr = found ? res : nullptr;
    it.bin_idx = 0; // Not used for find results
    it.el_idx = 0;  // Not used for find results
    return it;
  }

  static void insert( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    temp.value = typename blueprint::value_type();
    entry *res;
    bool found = table.perform( temp, IXHTAB_INSERT, &res );
    if( !found )
      *res = temp;
  }

  static void erase( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    table.perform( temp, IXHTAB_DELETE, &res );
  }

  static itr_type begin_itr( table_type &table )
  {
    return table.iter_begin();
  }

  static bool is_itr_valid( table_type &table, itr_type &itr )
  {
    return table.iter_valid( itr );
  }

  static void increment_itr( table_type &table, itr_type &itr )
  {
    table.iter_next( itr );
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
    // destructor handles cleanup
  }
};

template<> struct ixhtab_wrap< void >
{
  static constexpr const char *label = "ixhtab";
  static constexpr const char *color = "rgb( 150, 80, 200 )";
  static constexpr bool tombstone_like_mechanism = true;
};
