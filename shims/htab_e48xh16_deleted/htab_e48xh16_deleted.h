#ifndef HTAB_E48XH16_DELETED_H
#define HTAB_E48XH16_DELETED_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint64_t htab_e48xh16_deleted_ind_t;
typedef unsigned long htab_e48xh16_deleted_size_t;
typedef size_t htab_e48xh16_deleted_hash_t;

static constexpr htab_e48xh16_deleted_ind_t HTAB_E48XH16_DELETED_EMPTY_IND = 0xfffffffffffffffeULL;
static constexpr htab_e48xh16_deleted_ind_t HTAB_E48XH16_DELETED_DELETED_IND = 0xffffffffffffffffULL;

enum htab_e48xh16_deleted_action { HTAB_E48XH16_DELETED_FIND, HTAB_E48XH16_DELETED_INSERT, HTAB_E48XH16_DELETED_REPLACE, HTAB_E48XH16_DELETED_DELETE };

template<typename El>
struct hbin_e48xh16_deleted_t {
  htab_e48xh16_deleted_size_t els_start, els_bound;
  El *els;
  char *deleted;
  htab_e48xh16_deleted_ind_t *entries;
  htab_e48xh16_deleted_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct htab_e48xh16_deleted_t {
  htab_e48xh16_deleted_size_t els_num;
  hbin_e48xh16_deleted_t<El> bin;

  static void create (htab_e48xh16_deleted_t *htab, htab_e48xh16_deleted_size_t min_size) {
    htab_e48xh16_deleted_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    htab_e48xh16_deleted_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    b.entries = (htab_e48xh16_deleted_ind_t *) std::malloc (2 * size * sizeof (htab_e48xh16_deleted_ind_t));
    b.entries_mask = 2 * size - 1;
    for (htab_e48xh16_deleted_size_t i = 0; i <= b.entries_mask; i++) b.entries[i] = HTAB_E48XH16_DELETED_EMPTY_IND;
  }

  static void destroy_bin (hbin_e48xh16_deleted_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.entries);
  }

  static void destroy (htab_e48xh16_deleted_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (htab_e48xh16_deleted_t *htab, hbin_e48xh16_deleted_t<El> &bin, El &el,
                    enum htab_e48xh16_deleted_action action, El **res) {
    htab_e48xh16_deleted_ind_t *entry, *first_deleted_entry = nullptr;
    htab_e48xh16_deleted_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    htab_e48xh16_deleted_hash_t hash = hash_fn (el), peterb;
    uint16_t h16_val = (uint16_t) (hash >> (sizeof (size_t) * 8 - 16));

    peterb = hash;
    htab_e48xh16_deleted_size_t ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      htab_e48xh16_deleted_ind_t entry_val = *entry;
      if (entry_val < HTAB_E48XH16_DELETED_EMPTY_IND) {
        htab_e48xh16_deleted_size_t el_ind = entry_val & 0xffffffffffffULL;
        if ((uint16_t) (entry_val >> 48) == h16_val && eq_fn (bin.els[el_ind], el)) {
          if (action != HTAB_E48XH16_DELETED_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            *entry = HTAB_E48XH16_DELETED_DELETED_IND;
          }
          return true;
        }
      } else if (entry_val != HTAB_E48XH16_DELETED_DELETED_IND) {
        if (action == HTAB_E48XH16_DELETED_INSERT || action == HTAB_E48XH16_DELETED_REPLACE) {
          htab->els_num++;
          if (first_deleted_entry != nullptr) entry = first_deleted_entry;
          *res = &bin.els[bin.els_bound];
          *entry = ((htab_e48xh16_deleted_ind_t) h16_val << 48) | bin.els_bound;
          bin.els_bound++;
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
  static bool do_ (htab_e48xh16_deleted_t *htab, El &el, enum htab_e48xh16_deleted_action action, El **res) {
    htab_e48xh16_deleted_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HTAB_E48XH16_DELETED_DELETE && htab->bin.els_bound >= els_size) {
      htab_e48xh16_deleted_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin_e48xh16_deleted_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      htab_e48xh16_deleted_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.entries = (htab_e48xh16_deleted_ind_t *) std::malloc (size * sizeof (htab_e48xh16_deleted_ind_t));
      resize_bin.entries_mask = size - 1;
      for (htab_e48xh16_deleted_size_t j = 0; j < size; j++) resize_bin.entries[j] = HTAB_E48XH16_DELETED_EMPTY_IND;
      resize_bin.els_start = resize_bin.els_bound = 0;
      htab_e48xh16_deleted_size_t bound = htab->bin.els_bound;
      htab_e48xh16_deleted_size_t saved_els_num = htab->els_num;
      for (htab_e48xh16_deleted_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HTAB_E48XH16_DELETED_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static htab_e48xh16_deleted_size_t size (htab_e48xh16_deleted_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HTAB_E48XH16_DELETED_H */
