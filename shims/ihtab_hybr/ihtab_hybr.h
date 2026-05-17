#ifndef IHTAB_HYBR_H
#define IHTAB_HYBR_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

#define FORCE_INLINE __attribute__((always_inline)) inline

typedef uint32_t ihtab_hybr_ind_t;
typedef unsigned long ihtab_hybr_size_t;
typedef size_t ihtab_hybr_hash_t;

static constexpr unsigned int IHTAB_HYBR_GROUP_SIZE = 8;
static constexpr unsigned char IHTAB_HYBR_EMPTY_H7 = 0x80;
static constexpr unsigned char IHTAB_HYBR_DELETED_H7 = 0xfe;
static constexpr ihtab_hybr_ind_t IHTAB_HYBR_ENTRY_DELETED = ~(ihtab_hybr_ind_t) 0;
static constexpr unsigned int IHTAB_HYBR_LF_FACTOR = 1;
static constexpr unsigned int IHTAB_HYBR_LF_DIVISOR = 2;
static constexpr unsigned int IHTAB_HYBR_SMALL_LF_FACTOR = 4;
static constexpr unsigned int IHTAB_HYBR_SMALL_LF_DIVISOR = 5;


#if !defined(IHTAB_HYBR_USE_SWAR) && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))

#include <immintrin.h>
static const bool ihtab_hybr_mask_scale = false;
typedef __m128i ihtab_hybr_group_t;
static FORCE_INLINE ihtab_hybr_group_t ihtab_hybr_group_load (const unsigned char *p) {
  return _mm_cvtsi64_si128 (*(const long long *) p);
}
static FORCE_INLINE uint64_t ihtab_hybr_match_mask (ihtab_hybr_group_t g, unsigned char h7_val) {
  __m128i h7_vec = _mm_set1_epi8 ((char) h7_val);
  return (uint64_t) _mm_movemask_epi8 (_mm_cmpeq_epi8 (g, h7_vec)) & 0xff;
}
static const __m128i IHTAB_HYBR_EMPTY_MASK = _mm_set1_epi8 ((char) 0x80);
static FORCE_INLINE uint64_t ihtab_hybr_match_empty (ihtab_hybr_group_t g) {
  return (uint64_t) _mm_movemask_epi8 (_mm_and_si128 (g, IHTAB_HYBR_EMPTY_MASK)) & 0xff;
}

#elif !defined(IHTAB_HYBR_USE_SWAR) && (defined(__aarch64__) || defined(_M_ARM64))

#include <arm_neon.h>
static const bool ihtab_hybr_mask_scale = false;
typedef uint64_t ihtab_hybr_group_t;
static FORCE_INLINE ihtab_hybr_group_t ihtab_hybr_group_load (const unsigned char *p) {
  return *(const ihtab_hybr_group_t *) p;
}
static FORCE_INLINE uint64_t ihtab_hybr_match_mask (ihtab_hybr_group_t g, unsigned char h7_val) {
  uint8x8_t group = vcreate_u8 (g);
  uint8x8_t match_eq = vceq_u8 (group, vdup_n_u8 (h7_val));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (uint64_t) vaddv_u8 (vand_u8 (match_eq, bit_mask));
}
static FORCE_INLINE uint64_t ihtab_hybr_match_empty (ihtab_hybr_group_t g) {
  return ihtab_hybr_match_mask (g, IHTAB_HYBR_EMPTY_H7);
}

#else

static const bool ihtab_hybr_mask_scale = true;
static constexpr uint64_t IHTAB_HYBR_SWAR_LSB = 0x0101010101010101ULL;
static constexpr uint64_t IHTAB_HYBR_SWAR_MSB = 0x8080808080808080ULL;
typedef uint64_t ihtab_hybr_group_t;
static FORCE_INLINE ihtab_hybr_group_t ihtab_hybr_group_load (const unsigned char *p) {
  return *(const uint64_t *) p;
}
static FORCE_INLINE uint64_t ihtab_hybr_match_mask (ihtab_hybr_group_t g, unsigned char h7_val) {
  uint64_t cmp = g ^ (IHTAB_HYBR_SWAR_LSB * h7_val);
  return (cmp - IHTAB_HYBR_SWAR_LSB) & ~cmp & IHTAB_HYBR_SWAR_MSB;
}
static FORCE_INLINE uint64_t ihtab_hybr_match_empty (ihtab_hybr_group_t g) {
  return g & IHTAB_HYBR_SWAR_MSB;
}

#endif

enum ihtab_hybr_action { IHTAB_HYBR_FIND, IHTAB_HYBR_INSERT, IHTAB_HYBR_REPLACE, IHTAB_HYBR_DELETE };

template<typename El>
struct hbin_ihtab_hybr_t {
  ihtab_hybr_size_t els_bound;
  El *els;
  char *deleted;
  unsigned char *h7;
  ihtab_hybr_ind_t *entries;
  ihtab_hybr_size_t groups_mask;
};

template<typename El, typename Hash, typename Eq>
struct ihtab_hybr_t {
  static constexpr bool small_el = sizeof (El) <= 16;

  ihtab_hybr_size_t els_num;
  hbin_ihtab_hybr_t<El> bin;

  static void create (ihtab_hybr_t *htab, ihtab_hybr_size_t min_size) {
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_bound = 0;
    if constexpr (small_el) {
      ihtab_hybr_size_t sz = IHTAB_HYBR_GROUP_SIZE;
      while (sz * IHTAB_HYBR_SMALL_LF_FACTOR / IHTAB_HYBR_SMALL_LF_DIVISOR < min_size) sz *= 2;
      b.els = (El *) std::malloc (sz * sizeof (El));
      ihtab_hybr_size_t del_bytes = (sz + 7) / 8;
      b.deleted = (char *) std::calloc (del_bytes, 1);
      b.h7 = (unsigned char *) std::aligned_alloc (IHTAB_HYBR_GROUP_SIZE, sz);
      std::memset (b.h7, IHTAB_HYBR_EMPTY_H7, sz);
      b.entries = nullptr;
      b.groups_mask = sz / IHTAB_HYBR_GROUP_SIZE - 1;
    } else {
      ihtab_hybr_size_t entries_size = IHTAB_HYBR_GROUP_SIZE;
      while (entries_size * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR < min_size) entries_size *= 2;
      ihtab_hybr_size_t els_size = entries_size * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR;
      b.els = (El *) std::malloc (els_size * sizeof (El));
      ihtab_hybr_size_t del_bytes = (els_size + 7) / 8;
      b.deleted = (char *) std::calloc (del_bytes, 1);
      b.h7 = (unsigned char *) std::aligned_alloc (IHTAB_HYBR_GROUP_SIZE, entries_size);
      std::memset (b.h7, IHTAB_HYBR_EMPTY_H7, entries_size);
      b.entries = (ihtab_hybr_ind_t *) std::malloc (entries_size * sizeof (ihtab_hybr_ind_t));
      b.groups_mask = entries_size / IHTAB_HYBR_GROUP_SIZE - 1;
    }
  }

  static void destroy_bin (hbin_ihtab_hybr_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.h7);
    std::free (b.entries);
  }

  static void destroy (ihtab_hybr_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ihtab_hybr_t *htab, hbin_ihtab_hybr_t<El> &bin, El &el,
                    enum ihtab_hybr_action action, El **res) {
    Hash hash_fn;
    Eq eq_fn;
    ihtab_hybr_hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (size_t) * 8 - 7)) & 0x7f;
    ihtab_hybr_size_t group_ind = (hash / IHTAB_HYBR_GROUP_SIZE) & bin.groups_mask;
    ihtab_hybr_size_t first_deleted_slot = ~(ihtab_hybr_size_t) 0;

    for (;;) {
      unsigned char *group_h7 = bin.h7 + group_ind * IHTAB_HYBR_GROUP_SIZE;
      ihtab_hybr_group_t group = ihtab_hybr_group_load (group_h7);
      uint64_t match_mask = ihtab_hybr_match_mask (group, h7_val);
      while (match_mask) {
        unsigned int bit = __builtin_ctzll (match_mask);
	if (ihtab_hybr_mask_scale) bit /= 8;
        ihtab_hybr_size_t slot = group_ind * IHTAB_HYBR_GROUP_SIZE + bit;
        if constexpr (small_el) {
          if (bin.deleted[slot / 8] & (1 << (slot % 8))) {
            if (first_deleted_slot == ~(ihtab_hybr_size_t) 0)
              first_deleted_slot = slot;
          } else if (eq_fn (bin.els[slot], el)) {
            if (action != IHTAB_HYBR_DELETE) {
              *res = &bin.els[slot];
            } else {
              htab->els_num--;
              bin.deleted[slot / 8] |= 1 << (slot % 8);
            }
            return true;
          }
        } else {
          ihtab_hybr_ind_t el_ind = bin.entries[slot];
          if (el_ind == IHTAB_HYBR_ENTRY_DELETED) {
            if (first_deleted_slot == ~(ihtab_hybr_size_t) 0)
              first_deleted_slot = slot;
          } else if (eq_fn (bin.els[el_ind], el)) {
            if (action != IHTAB_HYBR_DELETE) {
              *res = &bin.els[el_ind];
            } else {
              htab->els_num--;
              bin.deleted[el_ind / 8] |= 1 << (el_ind % 8);
              bin.entries[slot] = IHTAB_HYBR_ENTRY_DELETED;
            }
            return true;
          }
        }
        match_mask &= match_mask - 1;
      }

      uint64_t empty_mask = ihtab_hybr_match_empty (group);
      if (empty_mask) {
        if (action == IHTAB_HYBR_INSERT || action == IHTAB_HYBR_REPLACE) {
          htab->els_num++;
          ihtab_hybr_size_t slot;
          if (first_deleted_slot != ~(ihtab_hybr_size_t) 0) {
            slot = first_deleted_slot;
          } else {
            unsigned int bit = __builtin_ctzll (empty_mask);
	    if (ihtab_hybr_mask_scale) bit /= 8;
            slot = group_ind * IHTAB_HYBR_GROUP_SIZE + bit;
          }
          bin.h7[slot] = h7_val;
          if constexpr (small_el) {
            bin.deleted[slot / 8] &= ~(1 << (slot % 8));
            *res = &bin.els[slot];
          } else {
            bin.entries[slot] = (ihtab_hybr_ind_t) bin.els_bound;
            *res = &bin.els[bin.els_bound];
            bin.els_bound++;
          }
        }
        return false;
      }

      group_ind = (group_ind + 1) & bin.groups_mask;
    }
  }

  static void rebuild (ihtab_hybr_t *htab) {
    ihtab_hybr_size_t old_size = (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE;
    hbin_ihtab_hybr_t<El> resize_bin;
    if constexpr (small_el) {
      ihtab_hybr_size_t sz = old_size;
      if (IHTAB_HYBR_SMALL_LF_DIVISOR * htab->els_num >= IHTAB_HYBR_SMALL_LF_FACTOR * sz)
        sz *= 2;
      resize_bin.els = (El *) std::malloc (sz * sizeof (El));
      ihtab_hybr_size_t del_bytes = (sz + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::aligned_alloc (IHTAB_HYBR_GROUP_SIZE, sz);
      std::memset (resize_bin.h7, IHTAB_HYBR_EMPTY_H7, sz);
      resize_bin.entries = nullptr;
      resize_bin.groups_mask = sz / IHTAB_HYBR_GROUP_SIZE - 1;
      resize_bin.els_bound = 0;
      ihtab_hybr_size_t saved_els_num = htab->els_num;
      htab->els_num = 0;
      for (ihtab_hybr_size_t i = 0; i < old_size; i++)
        if (htab->bin.h7[i] != IHTAB_HYBR_EMPTY_H7
            && !(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], IHTAB_HYBR_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
    } else {
      ihtab_hybr_size_t entries_size = old_size;
      ihtab_hybr_size_t els_size = entries_size * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR;
      if (2 * IHTAB_HYBR_LF_DIVISOR * htab->els_num >= IHTAB_HYBR_LF_FACTOR * entries_size) {
        entries_size *= 2;
        els_size = entries_size * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR;
      }
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ihtab_hybr_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.h7 = (unsigned char *) std::aligned_alloc (IHTAB_HYBR_GROUP_SIZE, entries_size);
      std::memset (resize_bin.h7, IHTAB_HYBR_EMPTY_H7, entries_size);
      resize_bin.entries = (ihtab_hybr_ind_t *) std::malloc (entries_size * sizeof (ihtab_hybr_ind_t));
      resize_bin.groups_mask = entries_size / IHTAB_HYBR_GROUP_SIZE - 1;
      resize_bin.els_bound = 0;
      ihtab_hybr_size_t bound = htab->bin.els_bound;
      ihtab_hybr_size_t saved_els_num = htab->els_num;
      htab->els_num = 0;
      for (ihtab_hybr_size_t i = 0; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], IHTAB_HYBR_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
    }
    destroy_bin (htab->bin);
    htab->bin = resize_bin;
  }

  __attribute__((always_inline))
  static bool do_ (ihtab_hybr_t *htab, El &el, enum ihtab_hybr_action action, El **res) {
    if constexpr (small_el) {
      ihtab_hybr_size_t sz = (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE;
      if (action != IHTAB_HYBR_DELETE
          && __builtin_expect(IHTAB_HYBR_SMALL_LF_DIVISOR * htab->els_num
                              >= IHTAB_HYBR_SMALL_LF_FACTOR * sz, 0))
        rebuild (htab);
    } else {
      ihtab_hybr_size_t entries_size = (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE;
      ihtab_hybr_size_t els_size = entries_size * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR;
      if (action != IHTAB_HYBR_DELETE && __builtin_expect(htab->bin.els_bound >= els_size, 0))
        rebuild (htab);
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ihtab_hybr_size_t size (ihtab_hybr_t *htab) {
    if constexpr (small_el)
      return (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE;
    else
      return (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE * IHTAB_HYBR_LF_FACTOR / IHTAB_HYBR_LF_DIVISOR;
  }

  struct iterator {
    ihtab_hybr_size_t el_idx;
    El *ptr;
  };

  __attribute__((always_inline))
  static void iter_advance (ihtab_hybr_t *htab, iterator &it) {
    if constexpr (small_el) {
      ihtab_hybr_size_t end = (htab->bin.groups_mask + 1) * IHTAB_HYBR_GROUP_SIZE;
      while (it.el_idx < end) {
        if (htab->bin.h7[it.el_idx] != IHTAB_HYBR_EMPTY_H7
            && !(htab->bin.deleted[it.el_idx / 8] & (1 << (it.el_idx % 8)))) {
          it.ptr = &htab->bin.els[it.el_idx];
          return;
        }
        ++it.el_idx;
      }
    } else {
      while (it.el_idx < htab->bin.els_bound) {
        if (!(htab->bin.deleted[it.el_idx / 8] & (1 << (it.el_idx % 8)))) {
          it.ptr = &htab->bin.els[it.el_idx];
          return;
        }
        ++it.el_idx;
      }
    }
    it.ptr = nullptr;
  }

  __attribute__((always_inline))
  static iterator iter_begin (ihtab_hybr_t *htab) {
    iterator it;
    it.el_idx = 0;
    it.ptr = nullptr;
    iter_advance (htab, it);
    return it;
  }

  __attribute__((always_inline))
  static bool iter_valid (iterator &it) {
    return it.ptr != nullptr;
  }

  __attribute__((always_inline))
  static void iter_next (ihtab_hybr_t *htab, iterator &it) {
    ++it.el_idx;
    iter_advance (htab, it);
  }
};

#endif /* #ifndef IHTAB_HYBR_H */
