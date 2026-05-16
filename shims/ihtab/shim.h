// c_cpp_hash_tables_benchmark/shims/ihtab/shim.h

#include "ihtab.h"

template< typename blueprint > struct ihtab
{
  struct entry
  {
    typename blueprint::key_type key;
    typename blueprint::value_type value;
  };

  struct hash
  {
    ihtab_hash_t operator()( const entry &e ) const
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

  using tab = ihtab_t< entry, hash, eq >;
  using table_type = tab;
  using itr_type = typename tab::iterator;

  static table_type create_table()
  {
    table_type table;
    tab::create( &table, 8 );
    return table;
  }

  static itr_type find( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    bool found = tab::do_( &table, temp, IHTAB_FIND, &res );
    itr_type itr;
    itr.ptr = found ? res : nullptr;
    itr.el_idx = 0;
    return itr;
  }

  static void insert( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    temp.value = typename blueprint::value_type();
    entry *res;
    bool found = tab::do_( &table, temp, IHTAB_INSERT, &res );
    if( !found )
      *res = temp;
  }

  static void erase( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    tab::do_( &table, temp, IHTAB_DELETE, &res );
  }

  static itr_type begin_itr( table_type &table )
  {
    return tab::iter_begin( &table );
  }

  static bool is_itr_valid( table_type &table, itr_type &itr )
  {
    return tab::iter_valid( itr );
  }

  static void increment_itr( table_type &table, itr_type &itr )
  {
    tab::iter_next( &table, itr );
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
    tab::destroy( &table );
  }
};

template<> struct ihtab< void >
{
  static constexpr const char *label = "ihtab";
  static constexpr const char *color = "rgb( 30, 100, 180 )";
  static constexpr bool tombstone_like_mechanism = true;
};
