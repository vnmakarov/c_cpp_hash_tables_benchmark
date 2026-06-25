#ifndef IHTAB_H
#define IHTAB_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>

#ifndef IHTAB_FORCE_INLINE
#define IHTAB_FORCE_INLINE __attribute__ ((always_inline)) inline
#endif

#if !defined(IHTAB_USE_SWAR) \
  && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
#include <immintrin.h>
#elif !defined(IHTAB_USE_SWAR) && (defined(__aarch64__) || defined(_M_ARM64))
#include <arm_neon.h>
#endif

namespace iht {

typedef uint32_t ind_t;
typedef unsigned long index_ind_t;
typedef size_t hash_t;

static constexpr unsigned int GROUP_SIZE = 8;
static constexpr size_t GROUP_BYTES = GROUP_SIZE * (1 + sizeof(ind_t));
static constexpr unsigned char EMPTY_H7 = 0xc0;
static constexpr unsigned char DELETED_H7 = 0x80;
static constexpr unsigned int LF_FACTOR = 1;
static constexpr unsigned int LF_DIVISOR = 2;

#if !defined(IHTAB_USE_SWAR) \
  && (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))

static const bool mask_scale = false;
typedef __m128i group_t;
static IHTAB_FORCE_INLINE group_t group_load (const unsigned char *p) {
  return _mm_cvtsi64_si128 (*(const long long *) p);
}
static IHTAB_FORCE_INLINE uint64_t match_mask (group_t g, unsigned char h7_val) {
  __m128i h7_vec = _mm_set1_epi8 ((char) h7_val);
  return (uint64_t) _mm_movemask_epi8 (_mm_cmpeq_epi8 (g, h7_vec)) & 0xff;
}
static IHTAB_FORCE_INLINE uint64_t match_empty (group_t g) {
  return (uint64_t) _mm_movemask_epi8 (_mm_and_si128 (g, _mm_slli_epi64 (g, 1))) & 0xff;
}

#elif !defined(IHTAB_USE_SWAR) && (defined(__aarch64__) || defined(_M_ARM64))

static const bool mask_scale = false;
typedef uint64_t group_t;
static IHTAB_FORCE_INLINE group_t group_load (const unsigned char *p) { return *(const group_t *) p; }
static IHTAB_FORCE_INLINE uint64_t match_mask (group_t g, unsigned char h7_val) {
  uint8x8_t group = vcreate_u8 (g);
  uint8x8_t match_eq = vceq_u8 (group, vdup_n_u8 (h7_val));
  static const uint8x8_t bit_mask = {1, 2, 4, 8, 16, 32, 64, 128};
  return (uint64_t) vaddv_u8 (vand_u8 (match_eq, bit_mask));
}
static IHTAB_FORCE_INLINE uint64_t match_empty (group_t g) { return match_mask (g, EMPTY_H7); }

#else

static const bool mask_scale = true;
static constexpr uint64_t SWAR_LSB = 0x0101010101010101ULL;
static constexpr uint64_t SWAR_MSB = 0x8080808080808080ULL;
typedef uint64_t group_t;
static IHTAB_FORCE_INLINE group_t group_load (const unsigned char *p) { return *(const uint64_t *) p; }
static IHTAB_FORCE_INLINE uint64_t match_mask (group_t g, unsigned char h7_val) {
  uint64_t cmp = g ^ (SWAR_LSB * h7_val);
  return (cmp - SWAR_LSB) & ~cmp & SWAR_MSB;
}
static IHTAB_FORCE_INLINE uint64_t match_empty (group_t g) { return (g & SWAR_MSB) & (g << 1); }

#endif

enum action { FIND, DELETE, INSERT };

template <typename El>
struct hbin_t {
  index_ind_t els_bound;
  El *els;
  char *deleted;
  unsigned char *groups;
  index_ind_t groups_mask;
};

template <typename El, typename Hash, typename Eq>
class ihtab {
  index_ind_t els_num;
  hbin_t<El> bin;

 public:
  ihtab (index_ind_t min_size = 8) {
    index_ind_t indexes_size = GROUP_SIZE;
    while (indexes_size * LF_FACTOR / LF_DIVISOR < min_size) indexes_size *= 2;
    index_ind_t els_size = indexes_size * LF_FACTOR / LF_DIVISOR;
    els_num = 0;
    bin.els_bound = 0;
    bin.els = (El *) std::malloc (els_size * sizeof (El));
    index_ind_t del_bytes = (els_size + 7) / 8;
    bin.deleted = (char *) std::calloc (del_bytes, 1);
    index_ind_t num_groups = indexes_size / GROUP_SIZE;
    bin.groups = (unsigned char *) std::aligned_alloc (GROUP_SIZE, num_groups * GROUP_BYTES);
    for (index_ind_t i = 0; i < num_groups; i++)
      std::memset (bin.groups + i * GROUP_BYTES, EMPTY_H7, GROUP_SIZE);
    bin.groups_mask = num_groups - 1;
  }

  ~ihtab () {
    std::free (bin.els);
    std::free (bin.deleted);
    std::free (bin.groups);
  }

 private:
  static void destroy_bin (hbin_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.groups);
  }

  IHTAB_FORCE_INLINE bool do_1 (hbin_t<El> &b, El &el, enum action action, El **res) {
    Hash hash_fn;
    Eq eq_fn;
    hash_t hash = hash_fn (el);
    unsigned char h7_val = (hash >> (sizeof (hash_t) * 8 - 7)) & 0x7f;
    index_ind_t group_ind = (hash / GROUP_SIZE) & b.groups_mask;

    for (;;) {
      unsigned char *group_base = b.groups + group_ind * GROUP_BYTES;
      group_t group = group_load (group_base);
      ind_t *group_indexes = (ind_t *) (group_base + GROUP_SIZE);
      uint64_t mmask = match_mask (group, h7_val);
      while (mmask) {
        unsigned int bit = __builtin_ctzll (mmask);
        if (mask_scale) bit /= 8;
        ind_t el_ind = group_indexes[bit];
        if (eq_fn (b.els[el_ind], el)) {
          if (action != DELETE) {
            *res = &b.els[el_ind];
          } else {
            els_num--;
            b.deleted[el_ind / 8] |= 1 << (el_ind % 8);
            group_base[bit] = DELETED_H7;
          }
          return true;
        }
        mmask &= mmask - 1;
      }

      uint64_t empty_mask = match_empty (group);
      if (empty_mask) {
        if (action >= INSERT) {
          els_num++;
          unsigned int bit = __builtin_ctzll (empty_mask);
          if (mask_scale) bit /= 8;
          group_base[bit] = h7_val;
          group_indexes[bit] = (ind_t) b.els_bound;
          *res = &b.els[b.els_bound];
          b.els_bound++;
        }
        return false;
      }

      group_ind = (group_ind + 1) & b.groups_mask;
    }
  }

  void rebuild () {
    index_ind_t indexes_size = (bin.groups_mask + 1) * GROUP_SIZE;
    index_ind_t els_size = indexes_size * LF_FACTOR / LF_DIVISOR;
    if (2 * LF_DIVISOR * els_num >= LF_FACTOR * indexes_size) {
      indexes_size *= 2;
      els_size = indexes_size * LF_FACTOR / LF_DIVISOR;
    }
    hbin_t<El> resize_bin;
    resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
    index_ind_t del_bytes = (els_size + 7) / 8;
    resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
    index_ind_t num_groups = indexes_size / GROUP_SIZE;
    resize_bin.groups = (unsigned char *) std::aligned_alloc (GROUP_SIZE, num_groups * GROUP_BYTES);
    for (index_ind_t i = 0; i < num_groups; i++)
      std::memset (resize_bin.groups + i * GROUP_BYTES, EMPTY_H7, GROUP_SIZE);
    resize_bin.groups_mask = num_groups - 1;
    resize_bin.els_bound = 0;
    index_ind_t bound = bin.els_bound;
    index_ind_t saved_els_num = els_num;
    for (index_ind_t i = 0; i < bound; i++)
      if (!(bin.deleted[i / 8] & (1 << (i % 8)))) {
        El *r;
        do_1 (resize_bin, bin.els[i], INSERT, &r);
        *r = bin.els[i];
      }
    els_num = saved_els_num;
    destroy_bin (bin);
    bin = resize_bin;
  }

 public:
  IHTAB_FORCE_INLINE bool perform (El &el, enum action action, El **res) {
    if (action >= INSERT) {
      index_ind_t indexes_size = (bin.groups_mask + 1) * GROUP_SIZE;
      index_ind_t els_size = indexes_size * LF_FACTOR / LF_DIVISOR - 1;
      if (__builtin_expect (bin.els_bound >= els_size, 0)) rebuild ();
    }
    return do_1 (bin, el, action, res);
  }

  index_ind_t els_count () const { return els_num; }

  index_ind_t size () const { return (bin.groups_mask + 1) * GROUP_SIZE * LF_FACTOR / LF_DIVISOR; }

  struct iter {
    index_ind_t el_idx;
    El *ptr;
  };

  IHTAB_FORCE_INLINE void iter_advance (iter &it) {
    while (it.el_idx < bin.els_bound) {
      if (!(bin.deleted[it.el_idx / 8] & (1 << (it.el_idx % 8)))) {
        it.ptr = &bin.els[it.el_idx];
        return;
      }
      ++it.el_idx;
    }
    it.ptr = nullptr;
  }

  IHTAB_FORCE_INLINE iter iter_begin () {
    iter it;
    it.el_idx = 0;
    it.ptr = nullptr;
    iter_advance (it);
    return it;
  }

  static IHTAB_FORCE_INLINE bool iter_valid (iter &it) { return it.ptr != nullptr; }

  IHTAB_FORCE_INLINE void iter_next (iter &it) {
    ++it.el_idx;
    iter_advance (it);
  }

  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = El;
    using difference_type = std::ptrdiff_t;
    using pointer = El *;
    using reference = El &;

    ihtab *htab;
    index_ind_t el_idx;

    IHTAB_FORCE_INLINE void advance () {
      while (el_idx < htab->bin.els_bound) {
        if (!(htab->bin.deleted[el_idx / 8] & (1 << (el_idx % 8)))) return;
        ++el_idx;
      }
    }

    IHTAB_FORCE_INLINE El &operator* () const { return htab->bin.els[el_idx]; }
    IHTAB_FORCE_INLINE El *operator->() const { return &htab->bin.els[el_idx]; }
    IHTAB_FORCE_INLINE iterator &operator++ () {
      ++el_idx;
      advance ();
      return *this;
    }
    IHTAB_FORCE_INLINE iterator operator++ (int) {
      iterator t = *this;
      ++(*this);
      return t;
    }
    IHTAB_FORCE_INLINE bool operator== (const iterator &o) const { return htab == o.htab && el_idx == o.el_idx; }
    IHTAB_FORCE_INLINE bool operator!= (const iterator &o) const { return htab != o.htab || el_idx != o.el_idx; }
  };

  IHTAB_FORCE_INLINE iterator begin () {
    iterator it{this, 0};
    it.advance ();
    return it;
  }

  IHTAB_FORCE_INLINE iterator end () { return {this, bin.els_bound}; }
};

}  // namespace iht

#endif /* #ifndef IHTAB_H */
