#ifndef HT_SWAR_H
#define HT_SWAR_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

static constexpr unsigned int HT_SWAR_GROUP_SIZE = 8;

typedef uint32_t ht_swar_ind_t;
typedef unsigned long ht_swar_size_t;
typedef size_t ht_swar_hash_t;

static constexpr unsigned char HT_SWAR_EMPTY_H7 = 0x80;
static constexpr unsigned char HT_SWAR_DELETED_H7 = 0xfe;
static constexpr uint64_t HT_SWAR_LSB = 0x0101010101010101ULL;
static constexpr uint64_t HT_SWAR_MSB = 0x8080808080808080ULL;

enum ht_swar_action { HT_SWAR_FIND, HT_SWAR_INSERT, HT_SWAR_REPLACE, HT_SWAR_DELETE };

__attribute__((always_inline))
static inline uint64_t ht_swar_match_mask (uint64_t group, unsigned char h7_val) {
  uint64_t cmp = group ^ (HT_SWAR_LSB * h7_val);
  return (cmp - HT_SWAR_LSB) & ~cmp & HT_SWAR_MSB;
}

__attribute__((always_inline))
static inline uint64_t ht_swar_empty_mask (uint64_t group) {
  uint64_t cmp = group ^ (HT_SWAR_LSB * HT_SWAR_EMPTY_H7);
  return (cmp - HT_SWAR_LSB) & ~cmp & HT_SWAR_MSB;
}

__attribute__((always_inline))
static inline uint64_t ht_swar_deleted_mask (uint64_t group) {
  uint64_t cmp = group ^ (HT_SWAR_LSB * HT_SWAR_DELETED_H7);
  return (cmp - HT_SWAR_LSB) & ~cmp & HT_SWAR_MSB;
}

template<typename El>
struct hbin_swar_t {
  ht_swar_size_t els_start, els_bound;
  El *els;
  char *deleted;
  unsigned char *h7;
  ht_swar_ind_t *entries;
  ht_swar_size_t groups_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_swar_t {
  ht_swar_size_t els_num;
  hbin_swar_t<El> bin;

  static void create (ht_swar_t *htab, ht_swar_size_t min_size) {
    ht_swar_size_t size;
    for (size = 2; min_size > size; size *= 2);
    if (size < HT_SWAR_GROUP_SIZE) size = HT_SWAR_GROUP_SIZE;
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    ht_swar_size_t entries_size = 2 * size;
    b.els = (El *) std::malloc (size * sizeof (El));
    ht_swar_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    b.h7 = (unsigned char *) std::aligned_alloc (HT_SWAR_GROUP_SIZE, entries_size);
    std::memset (b.h7, HT_SWAR_EMPTY_H7, entries_size);
    b.entries = (ht_swar_ind_t *) std::malloc (entries_size * sizeof (ht_swar_ind_t));
    b.groups_mask = entries_size / HT_SWAR_GROUP_SIZE - 1;
  }

  static void destroy_bin (hbin_swar_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ht_swar_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_swar_t *htab, hbin_swar_t<El> &bin, El &el,
                    enum ht_swar_action action, El **res) {
    Hash hash_fn;
    Eq eq_fn;
    ht_swar_hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    ht_swar_size_t group_ind = (hash / HT_SWAR_GROUP_SIZE) & bin.groups_mask;
    ht_swar_size_t first_deleted_slot = ~(ht_swar_size_t) 0;

    for (;;) {
      unsigned char *group_h7 = bin.h7 + group_ind * HT_SWAR_GROUP_SIZE;
      uint64_t group = *(const uint64_t *) group_h7;
      uint64_t match_mask = ht_swar_match_mask (group, h7_val);
      while (match_mask) {
        unsigned int bit = __builtin_ctzll (match_mask) / 8;
        ht_swar_size_t slot = group_ind * HT_SWAR_GROUP_SIZE + bit;
        ht_swar_ind_t el_ind = bin.entries[slot];
        if (eq_fn (bin.els[el_ind], el)) {
          if (action != HT_SWAR_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            group_h7[bit] = HT_SWAR_DELETED_H7;
          }
          return true;
        }
        match_mask &= match_mask - (uint64_t) 1;
      }

      uint64_t empty_mask = ht_swar_empty_mask (group);
      if (empty_mask) {
        if (action == HT_SWAR_INSERT || action == HT_SWAR_REPLACE) {
          htab->els_num++;
          ht_swar_size_t slot;
          if (first_deleted_slot != ~(ht_swar_size_t) 0) {
            slot = first_deleted_slot;
          } else {
            unsigned int bit = __builtin_ctzll (empty_mask) / 8;
            slot = group_ind * HT_SWAR_GROUP_SIZE + bit;
          }
          bin.h7[slot] = h7_val;
          bin.entries[slot] = (ht_swar_ind_t) bin.els_bound;
          *res = &bin.els[bin.els_bound];
          bin.els_bound++;
        }
        return false;
      }

      if (first_deleted_slot == ~(ht_swar_size_t) 0) {
        uint64_t del_mask = ht_swar_deleted_mask (group);
        if (del_mask) {
          unsigned int bit = __builtin_ctzll (del_mask) / 8;
          first_deleted_slot = group_ind * HT_SWAR_GROUP_SIZE + bit;
        }
      }

      group_ind = (group_ind + 1) & bin.groups_mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_swar_t *htab, El &el, enum ht_swar_action action, El **res) {
    ht_swar_size_t els_size = (htab->bin.groups_mask + 1) * HT_SWAR_GROUP_SIZE / 2;
    if (action != HT_SWAR_DELETE && __builtin_expect(htab->bin.els_bound >= els_size, 0)) {
      ht_swar_size_t entries_size = (htab->bin.groups_mask + 1) * HT_SWAR_GROUP_SIZE;
      if (2 * htab->els_num >= entries_size) {
        entries_size *= 2;
        els_size *= 2;
      }
      hbin_swar_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ht_swar_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::aligned_alloc (HT_SWAR_GROUP_SIZE, entries_size);
      std::memset (resize_bin.h7, HT_SWAR_EMPTY_H7, entries_size);
      resize_bin.entries = (ht_swar_ind_t *) std::malloc (entries_size * sizeof (ht_swar_ind_t));
      resize_bin.groups_mask = entries_size / HT_SWAR_GROUP_SIZE - 1;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_swar_size_t bound = htab->bin.els_bound;
      ht_swar_size_t saved_els_num = htab->els_num;
      for (ht_swar_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_SWAR_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_swar_size_t size (ht_swar_t *htab) {
    return (htab->bin.groups_mask + 1) * HT_SWAR_GROUP_SIZE / 2;
  }
};

#endif /* #ifndef HT_SWAR_H */
