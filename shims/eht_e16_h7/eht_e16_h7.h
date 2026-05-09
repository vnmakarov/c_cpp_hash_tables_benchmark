#ifndef EHT_E16_H7_H
#define EHT_E16_H7_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint16_t eht_e16_h7_ind_t;
typedef size_t eht_e16_h7_hash_t;
typedef unsigned short eht_e16_h7_depth_t;
typedef unsigned int ebin_e16_h7_ind_t;

static constexpr eht_e16_h7_ind_t EHT_E16_H7_EMPTY_IND = 0xfffe;
static constexpr eht_e16_h7_ind_t EHT_E16_H7_DELETED_IND = 0xffff;
static constexpr unsigned char EHT_E16_H7_DELETED_H7 = 0xff;
static constexpr unsigned int EHT_E16_H7_MAX_BIN_SIZE_POWER = 15;

static_assert (EHT_E16_H7_MAX_BIN_SIZE_POWER <= 15, "bin size must fit in uint16_t entries");

enum eht_e16_h7_action { EHT_E16_H7_FIND, EHT_E16_H7_INSERT, EHT_E16_H7_REPLACE, EHT_E16_H7_DELETE };

template<typename El>
struct ebin_e16_h7_t {
  eht_e16_h7_depth_t depth;
  eht_e16_h7_hash_t mask;
  unsigned int els_start, els_bound;
  El *els;
  unsigned char *h7;
  eht_e16_h7_ind_t *entries;
  unsigned int entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct eht_e16_h7_t {
  unsigned int els_num;
  eht_e16_h7_depth_t max_depth;
  eht_e16_h7_hash_t bin_mask;
  ebin_e16_h7_ind_t *dir;
  unsigned int dir_capacity;
  ebin_e16_h7_t<El> *bins;
  unsigned int bins_num;
  unsigned int bins_capacity;

  __attribute__((always_inline))
  static inline void init_entries (eht_e16_h7_ind_t *entries, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) entries[i] = EHT_E16_H7_EMPTY_IND;
  }

  static ebin_e16_h7_ind_t create_bin (eht_e16_h7_t *htab, unsigned int size) {
    if (htab->bins_num == htab->bins_capacity) {
      htab->bins_capacity = htab->bins_capacity ? htab->bins_capacity * 2 : 4;
      htab->bins = (ebin_e16_h7_t<El> *) std::realloc (htab->bins,
                                                  htab->bins_capacity * sizeof (ebin_e16_h7_t<El>));
    }
    ebin_e16_h7_ind_t ind = htab->bins_num++;
    auto &b = htab->bins[ind];
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    b.h7 = (unsigned char *) std::malloc (size);
    unsigned int entries_size = 2 * size;
    b.entries = (eht_e16_h7_ind_t *) std::malloc (entries_size * sizeof (eht_e16_h7_ind_t));
    b.entries_mask = entries_size - 1;
    init_entries (b.entries, entries_size);
    return ind;
  }

  static void destroy_bin (ebin_e16_h7_t<El> &b) {
    std::free (b.els);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void get_params (size_t size, size_t &bins_num, size_t &bin_power2, size_t &bin_size) {
    bin_size = size; bins_num = 1; bin_power2 = 0;
    while (bin_size >= (1u << EHT_E16_H7_MAX_BIN_SIZE_POWER)) {
      bins_num *= 2;
      bin_size /= 2;
      bin_power2++;
    }
  }

  static void create (eht_e16_h7_t *htab, unsigned int min_size) {
    unsigned int size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    htab->bins = nullptr;
    htab->bins_num = 0;
    htab->bins_capacity = 0;
    size_t bins_num, bin_power2, bin_size;
    get_params (size, bins_num, bin_power2, bin_size);
    htab->max_depth = (eht_e16_h7_depth_t) bin_power2;
    htab->bin_mask = ((eht_e16_h7_hash_t) 1 << bin_power2) - 1;
    htab->dir = (ebin_e16_h7_ind_t *) std::malloc (bins_num * sizeof (ebin_e16_h7_ind_t));
    htab->dir_capacity = (unsigned int) bins_num;
    for (size_t i = 0; i < bins_num; i++) {
      htab->dir[i] = (ebin_e16_h7_ind_t) i;
      ebin_e16_h7_ind_t ind = create_bin (htab, (unsigned int) bin_size);
      htab->bins[ind].depth = (eht_e16_h7_depth_t) bin_power2;
      htab->bins[ind].mask = (eht_e16_h7_hash_t) i;
    }
  }

  static void destroy (eht_e16_h7_t *htab) {
    for (unsigned int i = 0; i < htab->bins_num; i++)
      destroy_bin (htab->bins[i]);
    std::free (htab->bins);
    std::free (htab->dir);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static inline bool do_1 (eht_e16_h7_t *htab, ebin_e16_h7_t<El> &bin, eht_e16_h7_hash_t hash, El &el,
			   enum eht_e16_h7_action action, El **res) {
    eht_e16_h7_ind_t el_ind, *entry, *first_deleted_entry = nullptr;
    unsigned int mask = bin.entries_mask;
    Eq eq_fn;
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    eht_e16_h7_hash_t peterb = hash;

    unsigned int ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      el_ind = *entry;
      if (el_ind < EHT_E16_H7_EMPTY_IND) {
        if (bin.h7[el_ind] == h7_val && eq_fn (bin.els[el_ind], el)) {
          if (action != EHT_E16_H7_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.h7[el_ind] = EHT_E16_H7_DELETED_H7;
            *entry = EHT_E16_H7_DELETED_IND;
          }
          return true;
        }
      } else if (el_ind != EHT_E16_H7_DELETED_IND) {
        if (action == EHT_E16_H7_INSERT || action == EHT_E16_H7_REPLACE) {
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

  static void split_bin (eht_e16_h7_t *htab, ebin_e16_h7_ind_t bin_ind) {
    unsigned int size = (htab->bins[bin_ind].entries_mask + 1) / 2;
    ebin_e16_h7_ind_t new_ind = create_bin (htab, size);
    Hash hash_fn;
    for (;;) {
      auto &new_bin = htab->bins[new_ind];
      auto &bin = htab->bins[bin_ind];
      init_entries (bin.entries, bin.entries_mask + 1);
      eht_e16_h7_hash_t split_mask = (eht_e16_h7_hash_t) 1 << bin.depth;
      new_bin.depth = ++bin.depth;
      if (bin.depth > htab->max_depth) {
        htab->max_depth = bin.depth;
        htab->bin_mask = ~(~(eht_e16_h7_hash_t) 0 << htab->max_depth);
        unsigned int len = htab->dir_capacity;
        htab->dir = (ebin_e16_h7_ind_t *) std::realloc (htab->dir, 2 * len * sizeof (ebin_e16_h7_ind_t));
        htab->dir_capacity = 2 * len;
        for (unsigned int j = 0; j < len; j++) htab->dir[j + len] = htab->dir[j];
      }
      new_bin.mask = bin.mask | split_mask;
      htab->dir[new_bin.mask] = new_ind;
      unsigned int start = bin.els_start, bound = bin.els_bound;
      bin.els_start = bin.els_bound = 0;
      unsigned int els_num = htab->els_num;
      bool old_added = false, new_added = false;
      for (unsigned int i = start; i < bound; i++) {
        if (bin.h7[i] == EHT_E16_H7_DELETED_H7) continue;
        eht_e16_h7_hash_t hash = hash_fn (bin.els[i]);
        if (hash == 0) hash = 1;
        bool is_old = (hash & split_mask) == 0;
        if (is_old) old_added = true; else new_added = true;
        El *r;
        do_1 (htab, is_old ? htab->bins[bin_ind] : htab->bins[new_ind],
              hash, bin.els[i], EHT_E16_H7_INSERT, &r);
        *r = bin.els[i];
      }
      htab->els_num = els_num;
      if (!old_added) {
        ebin_e16_h7_ind_t temp = bin_ind;
        bin_ind = new_ind;
        new_ind = temp;
      } else if (new_added) {
        break;
      }
    }
  }

  __attribute__((always_inline))
  static bool do_ (eht_e16_h7_t *htab, El &el, enum eht_e16_h7_action action, El **res) {
    Hash hash_fn;
    eht_e16_h7_hash_t hash = hash_fn (el);
    if (hash == 0) hash = 1;
    eht_e16_h7_hash_t dir_ind = hash & htab->bin_mask;
    ebin_e16_h7_ind_t bin_ind = htab->dir[dir_ind];
    auto &bin = htab->bins[bin_ind];
    unsigned int entries_size = bin.entries_mask + 1;
    unsigned int els_size = entries_size / 2;
    if ((action == EHT_E16_H7_INSERT || action == EHT_E16_H7_REPLACE) && bin.els_bound == els_size) {
      bool grow = false;
      if (2 * htab->els_num >= entries_size) {
        entries_size *= 2;
        els_size *= 2;
        grow = true;
      }
      if (grow && els_size >= (1u << EHT_E16_H7_MAX_BIN_SIZE_POWER)) {
        split_bin (htab, bin_ind);
        bin_ind = htab->dir[hash & htab->bin_mask];
      } else {
        auto &b = htab->bins[bin_ind];
        b.els = (El *) std::realloc (b.els, els_size * sizeof (El));
        b.h7 = (unsigned char *) std::realloc (b.h7, els_size);
        b.entries = (eht_e16_h7_ind_t *) std::realloc (b.entries,
                                                    entries_size * sizeof (eht_e16_h7_ind_t));
        b.entries_mask = entries_size - 1;
        init_entries (b.entries, entries_size);
        unsigned int start = b.els_start, bound = b.els_bound;
        b.els_start = b.els_bound = 0;
        htab->els_num = 0;
        for (unsigned int i = start; i < bound; i++) {
          if (b.h7[i] != EHT_E16_H7_DELETED_H7) {
            eht_e16_h7_hash_t hash2 = hash_fn (b.els[i]);
            if (hash2 == 0) hash2 = 1;
            El *r;
            do_1 (htab, b, hash2, b.els[i], EHT_E16_H7_INSERT, &r);
            *r = b.els[i];
          }
        }
      }
    }
    return do_1 (htab, htab->bins[bin_ind], hash, el, action, res);
  }
};

#endif /* #ifndef EHT_E16_H7_H */
