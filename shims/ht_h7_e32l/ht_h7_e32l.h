#ifndef HT_H7_E32L_H
#define HT_H7_E32L_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint32_t ht_h7_e32l_ind_t;
typedef unsigned long ht_h7_e32l_size_t;
typedef size_t ht_h7_e32l_hash_t;

static constexpr unsigned char HT_H7_E32L_EMPTY_H7 = 0xfe;
static constexpr unsigned char HT_H7_E32L_DELETED_H7 = 0xff;

enum ht_h7_e32l_action { HT_H7_E32L_FIND, HT_H7_E32L_INSERT, HT_H7_E32L_REPLACE, HT_H7_E32L_DELETE };

template<typename El>
struct hbin_h7_e32l_t {
  ht_h7_e32l_size_t els_start, els_bound;
  El *els;
  char *deleted;
  unsigned char *h7;
  ht_h7_e32l_ind_t *entries;
  ht_h7_e32l_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_h7_e32l_t {
  ht_h7_e32l_size_t els_num;
  hbin_h7_e32l_t<El> bin;

  static void create (ht_h7_e32l_t *htab, ht_h7_e32l_size_t min_size) {
    ht_h7_e32l_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    ht_h7_e32l_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    ht_h7_e32l_size_t entries_size = 2 * size;
    b.h7 = (unsigned char *) std::malloc (entries_size);
    std::memset (b.h7, HT_H7_E32L_EMPTY_H7, entries_size);
    b.entries = (ht_h7_e32l_ind_t *) std::malloc (entries_size * sizeof (ht_h7_e32l_ind_t));
    b.entries_mask = entries_size - 1;
  }

  static void destroy_bin (hbin_h7_e32l_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ht_h7_e32l_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_h7_e32l_t *htab, hbin_h7_e32l_t<El> &bin, El &el,
                    enum ht_h7_e32l_action action, El **res) {
    ht_h7_e32l_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    ht_h7_e32l_hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    ht_h7_e32l_size_t first_deleted_ind = ~(ht_h7_e32l_size_t) 0;

    ht_h7_e32l_size_t ind = hash & mask;
    for (;;) {
      unsigned char h7_slot = bin.h7[ind];
      if (h7_slot == h7_val) {
        ht_h7_e32l_ind_t el_ind = bin.entries[ind];
        if (eq_fn (bin.els[el_ind], el)) {
          if (action != HT_H7_E32L_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            bin.h7[ind] = HT_H7_E32L_DELETED_H7;
          }
          return true;
        }
      } else if (h7_slot == HT_H7_E32L_EMPTY_H7) {
        if (action == HT_H7_E32L_INSERT || action == HT_H7_E32L_REPLACE) {
          htab->els_num++;
          if (first_deleted_ind != ~(ht_h7_e32l_size_t) 0) ind = first_deleted_ind;
          bin.h7[ind] = h7_val;
          bin.entries[ind] = (ht_h7_e32l_ind_t) bin.els_bound;
          *res = &bin.els[bin.els_bound];
          bin.els_bound++;
        }
        return false;
      } else if (h7_slot == HT_H7_E32L_DELETED_H7) {
        if (first_deleted_ind == ~(ht_h7_e32l_size_t) 0) first_deleted_ind = ind;
      }
      ind = (ind + 1) & mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_h7_e32l_t *htab, El &el, enum ht_h7_e32l_action action, El **res) {
    ht_h7_e32l_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HT_H7_E32L_DELETE && htab->bin.els_bound >= els_size) {
      ht_h7_e32l_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin_h7_e32l_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ht_h7_e32l_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::malloc (size);
      std::memset (resize_bin.h7, HT_H7_E32L_EMPTY_H7, size);
      resize_bin.entries = (ht_h7_e32l_ind_t *) std::malloc (size * sizeof (ht_h7_e32l_ind_t));
      resize_bin.entries_mask = size - 1;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_h7_e32l_size_t bound = htab->bin.els_bound;
      ht_h7_e32l_size_t saved_els_num = htab->els_num;
      for (ht_h7_e32l_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_H7_E32L_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_h7_e32l_size_t size (ht_h7_e32l_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HT_H7_E32L_H */
