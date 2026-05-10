#ifndef EHT_SIMD_H
#define EHT_SIMD_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define EHT_SIMD_USE_SSE2 1
static constexpr unsigned int EHT_SIMD_GROUP_SIZE = 8;
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define EHT_SIMD_USE_NEON 1
static constexpr unsigned int EHT_SIMD_GROUP_SIZE = 8;
#else
#error "eht_simd requires SSE2 or NEON"
#endif

typedef uint16_t eht_simd_ind_t;
typedef size_t eht_simd_hash_t;
typedef unsigned short eht_simd_depth_t;
typedef unsigned int ebin_simd_ind_t;

static constexpr unsigned char EHT_SIMD_EMPTY_H7 = 0x80;
static constexpr unsigned char EHT_SIMD_DELETED_H7 = 0xfe;
static constexpr unsigned int EHT_SIMD_MAX_BIN_SIZE_POWER = 15;

static_assert (EHT_SIMD_MAX_BIN_SIZE_POWER <= 15, "bin size must fit in uint16_t entries");

enum eht_simd_action { EHT_SIMD_FIND, EHT_SIMD_INSERT, EHT_SIMD_REPLACE, EHT_SIMD_DELETE };

#if EHT_SIMD_USE_SSE2

__attribute__((always_inline))
static inline unsigned int eht_simd_match_mask (const unsigned char *group_h7,
                                                unsigned char h7_val) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i h7_vec = _mm_set1_epi8 ((char) h7_val);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, h7_vec)) & 0xff;
}

__attribute__((always_inline))
static inline unsigned int eht_simd_empty_mask (const unsigned char *group_h7) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i empty_vec = _mm_set1_epi8 ((char) EHT_SIMD_EMPTY_H7);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, empty_vec)) & 0xff;
}

__attribute__((always_inline))
static inline unsigned int eht_simd_deleted_mask (const unsigned char *group_h7) {
  __m128i group = _mm_loadl_epi64 ((const __m128i *) group_h7);
  __m128i del_vec = _mm_set1_epi8 ((char) EHT_SIMD_DELETED_H7);
  return (unsigned int) _mm_movemask_epi8 (_mm_cmpeq_epi8 (group, del_vec)) & 0xff;
}

#elif EHT_SIMD_USE_NEON

__attribute__((always_inline))
static inline unsigned int eht_simd_match_mask (const unsigned char *group_h7,
                                                unsigned char h7_val) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t match_eq = vceq_u8 (group, vdup_n_u8 (h7_val));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (match_eq, bit_mask));
}

__attribute__((always_inline))
static inline unsigned int eht_simd_empty_mask (const unsigned char *group_h7) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t empty_eq = vceq_u8 (group, vdup_n_u8 (EHT_SIMD_EMPTY_H7));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (empty_eq, bit_mask));
}

__attribute__((always_inline))
static inline unsigned int eht_simd_deleted_mask (const unsigned char *group_h7) {
  uint8x8_t group = vld1_u8 (group_h7);
  uint8x8_t del_eq = vceq_u8 (group, vdup_n_u8 (EHT_SIMD_DELETED_H7));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (unsigned int) vaddv_u8 (vand_u8 (del_eq, bit_mask));
}

#endif

template<typename El>
struct ebin_simd_t {
  eht_simd_depth_t depth;
  eht_simd_hash_t mask;
  unsigned int els_start, els_bound;
  El *els;
  unsigned char *h7;
  eht_simd_ind_t *entries;
  unsigned int groups_mask;
};

template<typename El, typename Hash, typename Eq>
struct eht_simd_t {
  unsigned int els_num;
  eht_simd_depth_t max_depth;
  eht_simd_hash_t bin_mask;
  ebin_simd_ind_t *dir;
  unsigned int dir_capacity;
  ebin_simd_t<El> *bins;
  unsigned int bins_num;
  unsigned int bins_capacity;

  static ebin_simd_ind_t create_bin (eht_simd_t *htab, unsigned int size) {
    if (size < EHT_SIMD_GROUP_SIZE) size = EHT_SIMD_GROUP_SIZE;
    if (htab->bins_num == htab->bins_capacity) {
      htab->bins_capacity = htab->bins_capacity ? htab->bins_capacity * 2 : 4;
      htab->bins = (ebin_simd_t<El> *) std::realloc (htab->bins,
                                                      htab->bins_capacity * sizeof (ebin_simd_t<El>));
    }
    ebin_simd_ind_t ind = htab->bins_num++;
    auto &b = htab->bins[ind];
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    unsigned int entries_size = 2 * size;
    b.h7 = (unsigned char *) std::aligned_alloc (EHT_SIMD_GROUP_SIZE, entries_size);
    std::memset (b.h7, EHT_SIMD_EMPTY_H7, entries_size);
    b.entries = (eht_simd_ind_t *) std::malloc (entries_size * sizeof (eht_simd_ind_t));
    b.groups_mask = entries_size / EHT_SIMD_GROUP_SIZE - 1;
    return ind;
  }

  static void destroy_bin (ebin_simd_t<El> &b) {
    std::free (b.els);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void get_params (size_t size, size_t &bins_num, size_t &bin_power2, size_t &bin_size) {
    bin_size = size; bins_num = 1; bin_power2 = 0;
    while (bin_size >= (1u << EHT_SIMD_MAX_BIN_SIZE_POWER)) {
      bins_num *= 2;
      bin_size /= 2;
      bin_power2++;
    }
  }

  static void create (eht_simd_t *htab, unsigned int min_size) {
    unsigned int size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    htab->bins = nullptr;
    htab->bins_num = 0;
    htab->bins_capacity = 0;
    size_t bins_num, bin_power2, bin_size;
    get_params (size, bins_num, bin_power2, bin_size);
    htab->max_depth = (eht_simd_depth_t) bin_power2;
    htab->bin_mask = ((eht_simd_hash_t) 1 << bin_power2) - 1;
    htab->dir = (ebin_simd_ind_t *) std::malloc (bins_num * sizeof (ebin_simd_ind_t));
    htab->dir_capacity = (unsigned int) bins_num;
    for (size_t i = 0; i < bins_num; i++) {
      htab->dir[i] = (ebin_simd_ind_t) i;
      ebin_simd_ind_t ind = create_bin (htab, (unsigned int) bin_size);
      htab->bins[ind].depth = (eht_simd_depth_t) bin_power2;
      htab->bins[ind].mask = (eht_simd_hash_t) i;
    }
  }

  static void destroy (eht_simd_t *htab) {
    for (unsigned int i = 0; i < htab->bins_num; i++)
      destroy_bin (htab->bins[i]);
    std::free (htab->bins);
    std::free (htab->dir);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static inline bool do_1 (eht_simd_t *htab, ebin_simd_t<El> &bin, eht_simd_hash_t hash, El &el,
                           enum eht_simd_action action, El **res) {
    Eq eq_fn;
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    unsigned int group_ind = (unsigned int) (hash / EHT_SIMD_GROUP_SIZE) & bin.groups_mask;
    unsigned int first_deleted_slot = ~0u;

    for (;;) {
      unsigned char *group_h7 = bin.h7 + group_ind * EHT_SIMD_GROUP_SIZE;
      unsigned int match_mask = eht_simd_match_mask (group_h7, h7_val);
      while (match_mask) {
        unsigned int bit = __builtin_ctz (match_mask);
        unsigned int slot = group_ind * EHT_SIMD_GROUP_SIZE + bit;
        eht_simd_ind_t el_ind = bin.entries[slot];
        if (eq_fn (bin.els[el_ind], el)) {
          if (action != EHT_SIMD_DELETE) {
            *res = &bin.els[el_ind];
          } else {
            htab->els_num--;
            group_h7[bit] = EHT_SIMD_DELETED_H7;
          }
          return true;
        }
        match_mask &= match_mask - 1;
      }

      unsigned int empty_mask = eht_simd_empty_mask (group_h7);
      if (empty_mask) {
        if (action == EHT_SIMD_INSERT || action == EHT_SIMD_REPLACE) {
          htab->els_num++;
          unsigned int slot;
          if (first_deleted_slot != ~0u) {
            slot = first_deleted_slot;
          } else {
            unsigned int bit = __builtin_ctz (empty_mask);
            slot = group_ind * EHT_SIMD_GROUP_SIZE + bit;
          }
          bin.h7[slot] = h7_val;
          bin.entries[slot] = (eht_simd_ind_t) bin.els_bound;
          *res = &bin.els[bin.els_bound];
          bin.els_bound++;
        }
        return false;
      }

      if (first_deleted_slot == ~0u) {
        unsigned int del_mask = eht_simd_deleted_mask (group_h7);
        if (del_mask) {
          unsigned int bit = __builtin_ctz (del_mask);
          first_deleted_slot = group_ind * EHT_SIMD_GROUP_SIZE + bit;
        }
      }

      group_ind = (group_ind + 1) & bin.groups_mask;
    }
  }

  static void split_bin (eht_simd_t *htab, ebin_simd_ind_t bin_ind) {
    unsigned int size = (htab->bins[bin_ind].groups_mask + 1) * EHT_SIMD_GROUP_SIZE / 2;
    ebin_simd_ind_t new_ind = create_bin (htab, size);
    Hash hash_fn;
    for (;;) {
      auto &new_bin = htab->bins[new_ind];
      auto &bin = htab->bins[bin_ind];
      unsigned int entries_size = (bin.groups_mask + 1) * EHT_SIMD_GROUP_SIZE;
      std::memset (bin.h7, EHT_SIMD_EMPTY_H7, entries_size);
      eht_simd_hash_t split_mask = (eht_simd_hash_t) 1 << bin.depth;
      new_bin.depth = ++bin.depth;
      if (bin.depth > htab->max_depth) {
        htab->max_depth = bin.depth;
        htab->bin_mask = ~(~(eht_simd_hash_t) 0 << htab->max_depth);
        unsigned int len = htab->dir_capacity;
        htab->dir = (ebin_simd_ind_t *) std::realloc (htab->dir, 2 * len * sizeof (ebin_simd_ind_t));
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
        eht_simd_hash_t hash = hash_fn (bin.els[i]);
        if (hash == 0) hash = 1;
        bool is_old = (hash & split_mask) == 0;
        if (is_old) old_added = true; else new_added = true;
        El *r;
        do_1 (htab, is_old ? htab->bins[bin_ind] : htab->bins[new_ind],
              hash, bin.els[i], EHT_SIMD_INSERT, &r);
        *r = bin.els[i];
      }
      htab->els_num = els_num;
      if (!old_added) {
        ebin_simd_ind_t temp = bin_ind;
        bin_ind = new_ind;
        new_ind = temp;
      } else if (new_added) {
        break;
      }
    }
  }

  __attribute__((always_inline))
  static bool do_ (eht_simd_t *htab, El &el, enum eht_simd_action action, El **res) {
    Hash hash_fn;
    eht_simd_hash_t hash = hash_fn (el);
    if (hash == 0) hash = 1;
    eht_simd_hash_t dir_ind = hash & htab->bin_mask;
    ebin_simd_ind_t bin_ind = htab->dir[dir_ind];
    auto &bin = htab->bins[bin_ind];
    unsigned int entries_size = (bin.groups_mask + 1) * EHT_SIMD_GROUP_SIZE;
    unsigned int els_size = entries_size / 2;
    if ((action == EHT_SIMD_INSERT || action == EHT_SIMD_REPLACE) && bin.els_bound == els_size) {
      bool grow = false;
      if (2 * htab->els_num >= entries_size) {
        entries_size *= 2;
        els_size *= 2;
        grow = true;
      }
      if (grow && els_size >= (1u << EHT_SIMD_MAX_BIN_SIZE_POWER)) {
        split_bin (htab, bin_ind);
        bin_ind = htab->dir[hash & htab->bin_mask];
      } else {
        auto &b = htab->bins[bin_ind];
        b.els = (El *) std::realloc (b.els, els_size * sizeof (El));
        b.h7 = (unsigned char *) std::realloc (b.h7, entries_size);
        std::memset (b.h7, EHT_SIMD_EMPTY_H7, entries_size);
        b.entries = (eht_simd_ind_t *) std::realloc (b.entries,
                                                      entries_size * sizeof (eht_simd_ind_t));
        b.groups_mask = entries_size / EHT_SIMD_GROUP_SIZE - 1;
        unsigned int start = b.els_start, bound = b.els_bound;
        b.els_start = b.els_bound = 0;
        htab->els_num = 0;
        for (unsigned int i = start; i < bound; i++) {
          eht_simd_hash_t hash2 = hash_fn (b.els[i]);
          if (hash2 == 0) hash2 = 1;
          El *r;
          do_1 (htab, b, hash2, b.els[i], EHT_SIMD_INSERT, &r);
          *r = b.els[i];
        }
      }
    }
    return do_1 (htab, htab->bins[bin_ind], hash, el, action, res);
  }
};

#endif /* #ifndef EHT_SIMD_H */
