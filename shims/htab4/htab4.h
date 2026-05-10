#ifndef HTAB4_H
#define HTAB4_H

#include <cstdlib>
#include <cstring>
#include <cassert>

typedef unsigned long htab4_ind_t;
typedef unsigned long htab4_size_t;
typedef size_t htab4_hash_t;

static constexpr htab4_ind_t HTAB4_EMPTY_IND = 0xfffffffe;
static constexpr htab4_ind_t HTAB4_DELETED_IND = 0xffffffff;
static constexpr unsigned char HTAB4_DELETED_H7 = 0xff;

enum htab4_action { HTAB4_FIND, HTAB4_INSERT, HTAB4_REPLACE, HTAB4_DELETE };

template<typename El>
struct hbin4_t {
  htab4_size_t els_start, els_bound;
  El *els;
  unsigned char *h7;
  htab4_ind_t *entries;
  htab4_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct htab4_t {
  htab4_size_t els_num;
  hbin4_t<El> bin;

  static void create (htab4_t *htab, htab4_size_t min_size) {
    htab4_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    b.h7 = (unsigned char *) std::malloc (size);
    b.entries = (htab4_ind_t *) std::malloc (2 * size * sizeof (htab4_ind_t));
    b.entries_mask = 2 * size - 1;
    for (htab4_size_t i = 0; i <= b.entries_mask; i++) b.entries[i] = HTAB4_EMPTY_IND;
  }

  static void destroy_bin (hbin4_t<El> &b) {
    std::free (b.els);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (htab4_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (htab4_t *htab, hbin4_t<El> &bin, El &el,
                    enum htab4_action action, El **res) {
    htab4_ind_t el_ind, *entry, *first_deleted_entry = nullptr;
    htab4_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    htab4_hash_t hash = hash_fn (el), peterb;
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;

    peterb = hash;
    htab4_size_t ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      el_ind = *entry;
      if (el_ind < HTAB4_EMPTY_IND) {
        if (bin.h7[el_ind] == h7_val && eq_fn (bin.els[el_ind], el)) {
          if (action != HTAB4_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.h7[el_ind] = HTAB4_DELETED_H7;
            *entry = HTAB4_DELETED_IND;
          }
          return true;
        }
      } else if (el_ind != HTAB4_DELETED_IND) {
        if (action == HTAB4_INSERT || action == HTAB4_REPLACE) {
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
  static bool do_ (htab4_t *htab, El &el, enum htab4_action action, El **res) {
    htab4_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HTAB4_DELETE && htab->bin.els_bound >= els_size) {
      htab4_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin4_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      resize_bin.h7 = (unsigned char *) std::malloc (els_size);
      resize_bin.entries = (htab4_ind_t *) std::malloc (size * sizeof (htab4_ind_t));
      resize_bin.entries_mask = size - 1;
      for (htab4_size_t j = 0; j < size; j++) resize_bin.entries[j] = HTAB4_EMPTY_IND;
      resize_bin.els_start = resize_bin.els_bound = 0;
      htab4_size_t bound = htab->bin.els_bound;
      htab4_size_t saved_els_num = htab->els_num;
      for (htab4_size_t i = htab->bin.els_start; i < bound; i++)
        if (htab->bin.h7[i] != HTAB4_DELETED_H7) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HTAB4_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static htab4_size_t size (htab4_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HTAB4_H */
