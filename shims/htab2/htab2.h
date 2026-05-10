#ifndef HTAB2_H
#define HTAB2_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint32_t htab2_ind_t;
typedef unsigned long htab2_size_t;
typedef size_t htab2_hash_t;

static constexpr htab2_ind_t HTAB2_EMPTY_IND = 0xfffffffe;
static constexpr htab2_ind_t HTAB2_DELETED_IND = 0xffffffff;
static constexpr unsigned char HTAB2_DELETED_H7 = 0xff;

enum htab2_action { HTAB2_FIND, HTAB2_INSERT, HTAB2_REPLACE, HTAB2_DELETE };

template<typename El>
struct hbin2_t {
  htab2_size_t els_start, els_bound;
  El *els;
  unsigned char *h7;
  htab2_ind_t *entries;
  htab2_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct htab2_t {
  htab2_size_t els_num;
  hbin2_t<El> bin;

  static void create (htab2_t *htab, htab2_size_t min_size) {
    htab2_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    b.h7 = (unsigned char *) std::malloc (size);
    b.entries = (htab2_ind_t *) std::malloc (2 * size * sizeof (htab2_ind_t));
    b.entries_mask = 2 * size - 1;
    for (htab2_size_t i = 0; i <= b.entries_mask; i++) b.entries[i] = HTAB2_EMPTY_IND;
  }

  static void destroy_bin (hbin2_t<El> &b) {
    std::free (b.els);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (htab2_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (htab2_t *htab, hbin2_t<El> &bin, El &el,
                    enum htab2_action action, El **res) {
    htab2_ind_t el_ind, *entry, *first_deleted_entry = nullptr;
    htab2_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    htab2_hash_t hash = hash_fn (el), peterb;
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;

    peterb = hash;
    htab2_size_t ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      el_ind = *entry;
      if (el_ind < HTAB2_EMPTY_IND) {
        if (bin.h7[el_ind] == h7_val && eq_fn (bin.els[el_ind], el)) {
          if (action != HTAB2_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.h7[el_ind] = HTAB2_DELETED_H7;
            *entry = HTAB2_DELETED_IND;
          }
          return true;
        }
      } else if (el_ind != HTAB2_DELETED_IND) {
        if (action == HTAB2_INSERT || action == HTAB2_REPLACE) {
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
  static bool do_ (htab2_t *htab, El &el, enum htab2_action action, El **res) {
    htab2_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HTAB2_DELETE && htab->bin.els_bound >= els_size) {
      htab2_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin2_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      resize_bin.h7 = (unsigned char *) std::malloc (els_size);
      resize_bin.entries = (htab2_ind_t *) std::malloc (size * sizeof (htab2_ind_t));
      resize_bin.entries_mask = size - 1;
      for (htab2_size_t j = 0; j < size; j++) resize_bin.entries[j] = HTAB2_EMPTY_IND;
      resize_bin.els_start = resize_bin.els_bound = 0;
      htab2_size_t bound = htab->bin.els_bound;
      htab2_size_t saved_els_num = htab->els_num;
      for (htab2_size_t i = htab->bin.els_start; i < bound; i++)
        if (htab->bin.h7[i] != HTAB2_DELETED_H7) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HTAB2_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static htab2_size_t size (htab2_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HTAB2_H */
