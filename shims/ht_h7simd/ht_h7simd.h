#ifndef HT_H7SIMD_H
#define HT_H7SIMD_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <immintrin.h>

typedef uint32_t ht_h7simd_ind_t;
typedef unsigned long ht_h7simd_size_t;
typedef size_t ht_h7simd_hash_t;

static constexpr unsigned char HT_H7SIMD_EMPTY_H7 = 0x80;
static constexpr unsigned char HT_H7SIMD_DELETED_H7 = 0xfe;
static constexpr unsigned int HT_H7SIMD_GROUP_SIZE = 16;

enum ht_h7simd_action { HT_H7SIMD_FIND, HT_H7SIMD_INSERT, HT_H7SIMD_REPLACE, HT_H7SIMD_DELETE };

template<typename El>
struct hbin_h7simd_t {
  ht_h7simd_size_t els_start, els_bound;
  El *els;
  char *deleted;
  unsigned char *h7;
  ht_h7simd_ind_t *entries;
  ht_h7simd_size_t groups_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_h7simd_t {
  ht_h7simd_size_t els_num;
  hbin_h7simd_t<El> bin;

  static void create (ht_h7simd_t *htab, ht_h7simd_size_t min_size) {
    ht_h7simd_size_t size;
    for (size = 2; min_size > size; size *= 2);
    if (size < HT_H7SIMD_GROUP_SIZE) size = HT_H7SIMD_GROUP_SIZE;
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    ht_h7simd_size_t entries_size = 2 * size;
    b.els = (El *) std::malloc (size * sizeof (El));
    ht_h7simd_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    b.h7 = (unsigned char *) std::aligned_alloc (16, entries_size);
    std::memset (b.h7, HT_H7SIMD_EMPTY_H7, entries_size);
    b.entries = (ht_h7simd_ind_t *) std::malloc (entries_size * sizeof (ht_h7simd_ind_t));
    b.groups_mask = entries_size / HT_H7SIMD_GROUP_SIZE - 1;
  }

  static void destroy_bin (hbin_h7simd_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ht_h7simd_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_h7simd_t *htab, hbin_h7simd_t<El> &bin, El &el,
                    enum ht_h7simd_action action, El **res) {
    Hash hash_fn;
    Eq eq_fn;
    ht_h7simd_hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    ht_h7simd_size_t group_ind = (hash / HT_H7SIMD_GROUP_SIZE) & bin.groups_mask;

    __m128i h7_vec = _mm_set1_epi8 ((char) h7_val);
    __m128i empty_vec = _mm_set1_epi8 ((char) HT_H7SIMD_EMPTY_H7);
    ht_h7simd_size_t first_deleted_slot = ~(ht_h7simd_size_t) 0;

    for (;;) {
      unsigned char *group_h7 = bin.h7 + group_ind * HT_H7SIMD_GROUP_SIZE;
      __m128i group = _mm_load_si128 ((const __m128i *) group_h7);

      unsigned int match_mask = (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, h7_vec));
      unsigned int empty_mask = (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, empty_vec));

      while (match_mask) {
        unsigned int bit = __builtin_ctz (match_mask);
        ht_h7simd_size_t slot = group_ind * HT_H7SIMD_GROUP_SIZE + bit;
        ht_h7simd_ind_t el_ind = bin.entries[slot];
        if (eq_fn (bin.els[el_ind], el)) {
          if (action != HT_H7SIMD_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            group_h7[bit] = HT_H7SIMD_DELETED_H7;
          }
          return true;
        }
        match_mask &= match_mask - 1;
      }

      if (empty_mask) {
        if (action == HT_H7SIMD_INSERT || action == HT_H7SIMD_REPLACE) {
          htab->els_num++;
          ht_h7simd_size_t slot;
          if (first_deleted_slot != ~(ht_h7simd_size_t) 0) {
            slot = first_deleted_slot;
          } else {
            unsigned int bit = __builtin_ctz (empty_mask);
            slot = group_ind * HT_H7SIMD_GROUP_SIZE + bit;
          }
          bin.h7[slot] = h7_val;
          bin.entries[slot] = (ht_h7simd_ind_t) bin.els_bound;
          *res = &bin.els[bin.els_bound];
          bin.els_bound++;
        }
        return false;
      }

      if (first_deleted_slot == ~(ht_h7simd_size_t) 0) {
        __m128i del_vec = _mm_set1_epi8 ((char) HT_H7SIMD_DELETED_H7);
        unsigned int del_mask = (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, del_vec));
        if (del_mask) {
          unsigned int bit = __builtin_ctz (del_mask);
          first_deleted_slot = group_ind * HT_H7SIMD_GROUP_SIZE + bit;
        }
      }

      group_ind = (group_ind + 1) & bin.groups_mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_h7simd_t *htab, El &el, enum ht_h7simd_action action, El **res) {
    ht_h7simd_size_t els_size = (htab->bin.groups_mask + 1) * HT_H7SIMD_GROUP_SIZE / 2;
    if (action != HT_H7SIMD_DELETE && htab->bin.els_bound >= els_size) {
      ht_h7simd_size_t entries_size = (htab->bin.groups_mask + 1) * HT_H7SIMD_GROUP_SIZE;
      if (2 * htab->els_num >= entries_size) {
        entries_size *= 2;
        els_size *= 2;
      }
      hbin_h7simd_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ht_h7simd_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::aligned_alloc (16, entries_size);
      std::memset (resize_bin.h7, HT_H7SIMD_EMPTY_H7, entries_size);
      resize_bin.entries = (ht_h7simd_ind_t *) std::malloc (entries_size * sizeof (ht_h7simd_ind_t));
      resize_bin.groups_mask = entries_size / HT_H7SIMD_GROUP_SIZE - 1;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_h7simd_size_t bound = htab->bin.els_bound;
      ht_h7simd_size_t saved_els_num = htab->els_num;
      for (ht_h7simd_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_H7SIMD_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_h7simd_size_t size (ht_h7simd_t *htab) {
    return (htab->bin.groups_mask + 1) * HT_H7SIMD_GROUP_SIZE / 2;
  }
};

#endif /* #ifndef HT_H7SIMD_H */
