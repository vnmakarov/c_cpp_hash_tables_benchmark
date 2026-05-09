#ifndef HT_E32_H7_H
#define HT_E32_H7_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint32_t ht_e32_h7_ind_t;
typedef unsigned long ht_e32_h7_size_t;
typedef size_t ht_e32_h7_hash_t;

static constexpr ht_e32_h7_ind_t HT_E32_H7_EMPTY_IND = 0xfffffffe;
static constexpr ht_e32_h7_ind_t HT_E32_H7_DELETED_IND = 0xffffffff;
static constexpr unsigned char HT_E32_H7_DELETED_H7 = 0xff;

enum ht_e32_h7_action { HT_E32_H7_FIND, HT_E32_H7_INSERT, HT_E32_H7_REPLACE, HT_E32_H7_DELETE };

template<typename El>
struct hbin_e32_h7_t {
  ht_e32_h7_size_t els_start, els_bound;
  El *els;
  unsigned char *h7;
  ht_e32_h7_ind_t *entries;
  ht_e32_h7_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_e32_h7_t {
  ht_e32_h7_size_t els_num;
  hbin_e32_h7_t<El> bin;

  static void create (ht_e32_h7_t *htab, ht_e32_h7_size_t min_size) {
    ht_e32_h7_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    b.h7 = (unsigned char *) std::malloc (size);
    b.entries = (ht_e32_h7_ind_t *) std::malloc (2 * size * sizeof (ht_e32_h7_ind_t));
    b.entries_mask = 2 * size - 1;
    for (ht_e32_h7_size_t i = 0; i <= b.entries_mask; i++) b.entries[i] = HT_E32_H7_EMPTY_IND;
  }

  static void destroy_bin (hbin_e32_h7_t<El> &b) {
    std::free (b.els);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ht_e32_h7_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_e32_h7_t *htab, hbin_e32_h7_t<El> &bin, El &el,
                    enum ht_e32_h7_action action, El **res) {
    ht_e32_h7_ind_t el_ind, *entry, *first_deleted_entry = nullptr;
    ht_e32_h7_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    ht_e32_h7_hash_t hash = hash_fn (el), peterb;
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;

    peterb = hash;
    ht_e32_h7_size_t ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      el_ind = *entry;
      if (el_ind < HT_E32_H7_EMPTY_IND) {
        if (bin.h7[el_ind] == h7_val && eq_fn (bin.els[el_ind], el)) {
          if (action != HT_E32_H7_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.h7[el_ind] = HT_E32_H7_DELETED_H7;
            *entry = HT_E32_H7_DELETED_IND;
          }
          return true;
        }
      } else if (el_ind != HT_E32_H7_DELETED_IND) {
        if (action == HT_E32_H7_INSERT || action == HT_E32_H7_REPLACE) {
          htab->els_num++;
          if (first_deleted_entry != nullptr) entry = first_deleted_entry;
          bin.h7[bin.els_bound] = h7_val;
          *res = &bin.els[bin.els_bound];
          *entry = bin.els_bound++;
        }
        return false;
      } else {
        first_deleted_entry = entry;
      }
      peterb >>= 11;
      ind = (5 * ind + peterb + 1) & mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_e32_h7_t *htab, El &el, enum ht_e32_h7_action action, El **res) {
    ht_e32_h7_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HT_E32_H7_DELETE && htab->bin.els_bound >= els_size) {
      ht_e32_h7_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin_e32_h7_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      resize_bin.h7 = (unsigned char *) std::malloc (els_size);
      resize_bin.entries = (ht_e32_h7_ind_t *) std::malloc (size * sizeof (ht_e32_h7_ind_t));
      resize_bin.entries_mask = size - 1;
      for (ht_e32_h7_size_t j = 0; j < size; j++) resize_bin.entries[j] = HT_E32_H7_EMPTY_IND;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_e32_h7_size_t bound = htab->bin.els_bound;
      ht_e32_h7_size_t saved_els_num = htab->els_num;
      for (ht_e32_h7_size_t i = htab->bin.els_start; i < bound; i++)
        if (htab->bin.h7[i] != HT_E32_H7_DELETED_H7) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_E32_H7_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_e32_h7_size_t size (ht_e32_h7_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HT_E32_H7_H */
