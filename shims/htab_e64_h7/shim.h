// c_cpp_hash_tables_benchmark/shims/htab_e64_h7/shim.h

#include "htab_e64_h7.h"

template< typename blueprint > struct htab_e64_h7
{
  struct entry
  {
    typename blueprint::key_type key;
    typename blueprint::value_type value;
  };

  struct hash
  {
    htab_e64_h7_hash_t operator()( const entry &e ) const
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

  using tab = htab_e64_h7_t< entry, hash, eq >;
  using table_type = tab;

  struct itr_type
  {
    htab_e64_h7_size_t el_idx;
    entry *entry_ptr;
  };

  static table_type create_table()
  {
    table_type table;
    tab::create( &table, 8 );
    return table;
  }

  static void advance_itr( table_type &table, itr_type &itr )
  {
    while( itr.el_idx < table.bin.els_bound )
    {
      if( table.bin.h7[itr.el_idx] != HTAB_E64_H7_DELETED_H7 )
      {
        itr.entry_ptr = &table.bin.els[itr.el_idx];
        return;
      }
      ++itr.el_idx;
    }
    itr.entry_ptr = nullptr;
  }

  static itr_type find( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    bool found = tab::do_( &table, temp, HTAB_E64_H7_FIND, &res );
    itr_type itr;
    itr.entry_ptr = found ? res : nullptr;
    itr.el_idx = 0;
    return itr;
  }

  static void insert( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    temp.value = typename blueprint::value_type();
    entry *res;
    bool found = tab::do_( &table, temp, HTAB_E64_H7_INSERT, &res );
    if( !found )
      *res = temp;
  }

  static void erase( table_type &table, const typename blueprint::key_type &key )
  {
    entry temp;
    temp.key = key;
    entry *res;
    tab::do_( &table, temp, HTAB_E64_H7_DELETE, &res );
  }

  static itr_type begin_itr( table_type &table )
  {
    itr_type itr;
    itr.el_idx = table.bin.els_start;
    itr.entry_ptr = nullptr;
    advance_itr( table, itr );
    return itr;
  }

  static bool is_itr_valid( table_type &table, itr_type &itr )
  {
    return itr.entry_ptr != nullptr;
  }

  static void increment_itr( table_type &table, itr_type &itr )
  {
    ++itr.el_idx;
    advance_itr( table, itr );
  }

  static const typename blueprint::key_type &get_key_from_itr( table_type &table, itr_type &itr )
  {
    return itr.entry_ptr->key;
  }

  static const typename blueprint::value_type &get_value_from_itr( table_type &table, itr_type &itr )
  {
    return itr.entry_ptr->value;
  }

  static void destroy_table( table_type &table )
  {
    tab::destroy( &table );
  }
};

template<> struct htab_e64_h7< void >
{
  static constexpr const char *label = "htab_e64_h7";
  static constexpr const char *color = "rgb( 180, 50, 180 )";
  static constexpr bool tombstone_like_mechanism = true;
};
