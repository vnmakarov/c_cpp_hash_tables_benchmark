#ifndef HT_SIMD_NM_H
#define HT_SIMD_NM_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define HT_SIMD_NM_USE_SSE2 1
static constexpr unsigned int HT_SIMD_NM_GROUP_SIZE = 8;
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define HT_SIMD_NM_USE_NEON 1
static constexpr unsigned int HT_SIMD_NM_GROUP_SIZE = 8;
#else
#error "ht_simd_nm requires SSE2 or NEON"
#endif

typedef uint32_t ht_simd_nm_ind_t;
typedef unsigned long ht_simd_nm_size_t;
typedef size_t ht_simd_nm_hash_t;

static constexpr unsigned char HT_SIMD_NM_EMPTY_H7 = 0x80;
static constexpr unsigned char HT_SIMD_NM_DELETED_H7 = 0xfe;

enum ht_simd_nm_action { HT_SIMD_NM_FIND, HT_SIMD_NM_INSERT, HT_SIMD_NM_REPLACE, HT_SIMD_NM_DELETE };

#if HT_SIMD_NM_USE_SSE2

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_match_mask (const unsigned char *group_h7,
                                                  unsigned char h7_val) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i h7_vec = _mm_set1_epi8 ((char) h7_val);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, h7_vec)) & 0xff;
}

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_empty_mask (const unsigned char *group_h7) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i empty_vec = _mm_set1_epi8 ((char) HT_SIMD_NM_EMPTY_H7);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, empty_vec)) & 0xff;
}

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_deleted_mask (const unsigned char *group_h7) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i del_vec = _mm_set1_epi8 ((char) HT_SIMD_NM_DELETED_H7);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, del_vec)) & 0xff;
}

#elif HT_SIMD_NM_USE_NEON

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_match_mask (const unsigned char *group_h7,
                                                  unsigned char h7_val) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t match_eq = vceq_u8 (group, vdup_n_u8 (h7_val));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (match_eq, bit_mask));
}

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_empty_mask (const unsigned char *group_h7) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t empty_eq = vceq_u8 (group, vdup_n_u8 (HT_SIMD_NM_EMPTY_H7));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (empty_eq, bit_mask));
}

__attribute__((always_inline))
static inline unsigned int ht_simd_nm_deleted_mask (const unsigned char *group_h7) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t del_eq = vceq_u8 (group, vdup_n_u8 (HT_SIMD_NM_DELETED_H7));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (del_eq, bit_mask));
}

#endif

template<typename El>
struct hbin_simd_nm_t {
  ht_simd_nm_size_t els_start, els_bound;
  El *els;
  char *deleted;
  unsigned char *h7;
  ht_simd_nm_ind_t *entries;
  ht_simd_nm_size_t groups_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_simd_nm_t {
  ht_simd_nm_size_t els_num;
  hbin_simd_nm_t<El> bin;

  static void create (ht_simd_nm_t *htab, ht_simd_nm_size_t min_size) {
    ht_simd_nm_size_t size;
    for (size = 2; min_size > size; size *= 2);
    if (size < HT_SIMD_NM_GROUP_SIZE) size = HT_SIMD_NM_GROUP_SIZE;
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    ht_simd_nm_size_t entries_size = 2 * size;
    b.els = (El *) std::malloc (size * sizeof (El));
    ht_simd_nm_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    b.h7 = (unsigned char *) std::aligned_alloc (HT_SIMD_NM_GROUP_SIZE, entries_size);
    std::memset (b.h7, HT_SIMD_NM_EMPTY_H7, entries_size);
    b.entries = (ht_simd_nm_ind_t *) std::malloc (entries_size * sizeof (ht_simd_nm_ind_t));
    b.groups_mask = entries_size / HT_SIMD_NM_GROUP_SIZE - 1;
  }

  static void destroy_bin (hbin_simd_nm_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ht_simd_nm_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_simd_nm_t *htab, hbin_simd_nm_t<El> &bin, El &el,
                    enum ht_simd_nm_action action, El **res) {
    Hash hash_fn;
    Eq eq_fn;
    ht_simd_nm_hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    ht_simd_nm_size_t group_ind = (hash / HT_SIMD_NM_GROUP_SIZE) & bin.groups_mask;
    ht_simd_nm_size_t first_deleted_slot = ~(ht_simd_nm_size_t) 0;

    for (;;) {
      unsigned char *group_h7 = bin.h7 + group_ind * HT_SIMD_NM_GROUP_SIZE;
      unsigned int match_mask = ht_simd_nm_match_mask (group_h7, h7_val);
      while (match_mask) {
        unsigned int bit = __builtin_ctz (match_mask);
        ht_simd_nm_size_t slot = group_ind * HT_SIMD_NM_GROUP_SIZE + bit;
        ht_simd_nm_ind_t el_ind = bin.entries[slot];
        if (eq_fn (bin.els[el_ind], el)) {
          if (action != HT_SIMD_NM_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            group_h7[bit] = HT_SIMD_NM_DELETED_H7;
          }
          return true;
        }
        match_mask &= match_mask - 1;
      }

      unsigned int empty_mask = ht_simd_nm_empty_mask (group_h7);
      if (empty_mask) {
        if (action == HT_SIMD_NM_INSERT || action == HT_SIMD_NM_REPLACE) {
          htab->els_num++;
          ht_simd_nm_size_t slot;
          if (first_deleted_slot != ~(ht_simd_nm_size_t) 0) {
            slot = first_deleted_slot;
          } else {
            unsigned int bit = __builtin_ctz (empty_mask);
            slot = group_ind * HT_SIMD_NM_GROUP_SIZE + bit;
          }
          bin.h7[slot] = h7_val;
          bin.entries[slot] = (ht_simd_nm_ind_t) bin.els_bound;
          *res = &bin.els[bin.els_bound];
          bin.els_bound++;
        }
        return false;
      }

      if (first_deleted_slot == ~(ht_simd_nm_size_t) 0) {
        unsigned int del_mask = ht_simd_nm_deleted_mask (group_h7);
        if (del_mask) {
          unsigned int bit = __builtin_ctz (del_mask);
          first_deleted_slot = group_ind * HT_SIMD_NM_GROUP_SIZE + bit;
        }
      }

      group_ind = (group_ind + 1) & bin.groups_mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_simd_nm_t *htab, El &el, enum ht_simd_nm_action action, El **res) {
    ht_simd_nm_size_t els_size = (htab->bin.groups_mask + 1) * HT_SIMD_NM_GROUP_SIZE / 2;
    if (action != HT_SIMD_NM_DELETE && __builtin_expect(htab->bin.els_bound >= els_size, 0)) {
      ht_simd_nm_size_t entries_size = (htab->bin.groups_mask + 1) * HT_SIMD_NM_GROUP_SIZE;
      if (2 * htab->els_num >= entries_size) {
        entries_size *= 2;
        els_size *= 2;
      }
      hbin_simd_nm_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ht_simd_nm_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::aligned_alloc (HT_SIMD_NM_GROUP_SIZE, entries_size);
      std::memset (resize_bin.h7, HT_SIMD_NM_EMPTY_H7, entries_size);
      resize_bin.entries = (ht_simd_nm_ind_t *) std::malloc (entries_size * sizeof (ht_simd_nm_ind_t));
      resize_bin.groups_mask = entries_size / HT_SIMD_NM_GROUP_SIZE - 1;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_simd_nm_size_t bound = htab->bin.els_bound;
      ht_simd_nm_size_t saved_els_num = htab->els_num;
      for (ht_simd_nm_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_SIMD_NM_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_simd_nm_size_t size (ht_simd_nm_t *htab) {
    return (htab->bin.groups_mask + 1) * HT_SIMD_NM_GROUP_SIZE / 2;
  }
};

#endif /* #ifndef HT_SIMD_NM_H */
