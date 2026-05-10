#ifndef HTAB_H
#define HTAB_H

#include <stdbool.h>
#include "varr.h"

#if !defined(HTAB_ENABLE_CHECKING) && !defined(NDEBUG)
#define HTAB_ENABLE_CHECKING
#endif

#ifndef HTAB_ENABLE_CHECKING
#define HTAB_ASSERT(EXPR, OP) ((void) (EXPR))

#else
static inline void htab_assert_fail (const char *op) {
  fprintf (stderr, "wrong %s\n", op);
  assert (0);
}

#define HTAB_ASSERT(EXPR, OP) (void) ((EXPR) ? 0 : (htab_assert_fail (OP), 0))

#endif

#ifdef __GNUC__
#define HTAB_UNUSED __attribute__ ((unused))
#define HTAB_NO_RETURN __attribute__ ((noreturn))
#else
#define HTAB_UNUSED
#define HTAB_NO_RETURN
#endif

static inline void HTAB_NO_RETURN cjit_htab_error (const char *message) {
#ifdef HTAB_ERROR
  HTAB_ERROR (message);
  assert (false);
#else
  fprintf (stderr, "%s\n", message);
#endif
  exit (1);
}

/*---------------- Typed hash table -----------------------------*/
typedef unsigned int htab_ind_t;
typedef unsigned int htab_size_t;
typedef unsigned int htab_hash_t;
typedef unsigned short htab_depth_t;
typedef unsigned int hbin_ind_t;

#define HTAB_EMPTY_IND ((htab_ind_t) 0)
#define HTAB_DELETED_HASH 0
#define HTAB_MAX_BIN_SIZE_POWER 15

#define HTAB(T) htab_t

enum htab_action { HTAB_FIND, HTAB_INSERT, HTAB_REPLACE, HTAB_DELETE };

typedef struct {
  htab_depth_t depth;
  htab_hash_t mask;
  htab_size_t els_start, els_bound;
  VARR (T) els;
  VARR (htab_hash_t) hashes;
  VARR (htab_ind_t) entries;
} hbin_t;

typedef struct {
  htab_size_t els_num, collisions;
  void *arg;
  htab_hash_t (*hash_func) (void *el, void *arg);
  int (*eq_func) (void *el1, void *el2, void *arg);
  void (*free_func) (void *el, void *arg);
  htab_depth_t max_depth;
  htab_hash_t bin_mask;
  VARR (hbin_ind_t) dir; /* directory of bin indexes */
  VARR (hbin_t) bins;
} htab_t;

static inline hbin_ind_t _htab_create_bin (htab_t *htab, htab_size_t size, size_t el_size) {
  hbin_ind_t ind = (hbin_ind_t) VARR_LENGTH (hbin_t, htab->bins);
  VARR_SET_LENGTH (hbin_t, htab->bins, ind + 1);
  hbin_t *bin = VARR_ELADDR (hbin_t, htab->bins, ind);
  bin->els_start = bin->els_bound = 0;
  _varr_create (&bin->els, el_size, size);
  _varr_set_length (&bin->els, el_size, size);
  VARR_CREATE (htab_hash_t, bin->hashes, size);
  VARR_SET_LENGTH (htab_hash_t, bin->hashes, size);
  VARR_CREATE_ZERO (htab_ind_t, bin->entries, 2 * size);
  VARR_SET_LENGTH (htab_ind_t, bin->entries, 2 * size);
  return ind;
}

static inline void _htab_get_htab_params (size_t size, size_t *bins_num_ref, size_t *bin_power2_ref,
                                          size_t *bin_size_ref) {
  size_t bin_size = size, bins_num = 1, bin_power2 = 0;
  while (bin_size >= (1 << HTAB_MAX_BIN_SIZE_POWER)) {
    bins_num *= 2;
    bin_size /= 2;
    bin_power2++;
  }
  *bins_num_ref = bins_num;
  *bin_power2_ref = bin_power2;
  *bin_size_ref = bin_size;
}

static inline void _htab_create (htab_t *htab, size_t el_size, htab_size_t min_size,
                                 htab_hash_t (*hash_func) (void *el, void *arg),
                                 int (*eq_func) (void *el1, void *el2, void *arg),
                                 void (*free_func) (void *el, void *arg), void *arg) {
  htab_size_t size;

  static_assert (HTAB_EMPTY_IND == 0, "non-zero HTAB_EMPTY_IND");
  for (size = 2; min_size > size; size *= 2);
  htab->arg = arg;
  htab->hash_func = hash_func;
  htab->eq_func = eq_func;
  htab->free_func = free_func;
  htab->els_num = htab->collisions = 0;
  size_t bins_num, bin_power2, bin_size;
  _htab_get_htab_params (size, &bins_num, &bin_power2, &bin_size);
  htab->max_depth = bin_power2;
  htab->bin_mask = ((htab_hash_t) 1 << bin_power2) - 1;
  VARR_CREATE (hbin_ind_t, htab->dir, bins_num);
  VARR_CREATE (hbin_t, htab->bins, bins_num);
  for (size_t i = 0; i < bins_num; i++) {
    VARR_PUSH (hbin_ind_t, htab->dir, i);
    size_t ind = _htab_create_bin (htab, bin_size, el_size);
    hbin_t *bin = VARR_ELADDR (hbin_t, htab->bins, ind);
    bin->depth = bin_power2;
    bin->mask = (htab_hash_t) i;
  }
}

static inline void _htab_destroy_bin (hbin_t *bin) {
  _varr_destroy (&bin->els);
  VARR_DESTROY (htab_hash_t, bin->hashes);
  VARR_DESTROY (htab_ind_t, bin->entries);
}

static inline void _htab_clear (htab_t *htab, size_t el_size, size_t new_size) {
  HTAB_ASSERT (htab != NULL, "clear");
  htab_size_t s;
  for (s = 2; new_size > s; s *= 2);
  new_size = s;
  size_t bins_num = VARR_LENGTH (hbin_t, htab->bins);
  size_t size = (1 << (HTAB_MAX_BIN_SIZE_POWER - 1)) * bins_num;
  size_t new_bin_size, new_bins_num, new_bin_power2;
  if (new_size != 0 && new_size < size) size = new_size;
  _htab_get_htab_params (size, &new_bins_num, &new_bin_power2, &new_bin_size);
  void *arg = htab->arg;
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);
  for (size_t i = 0; i < bins_num; i++) {
    hbin_t *bin = &bins_addr[i];
    if (htab->free_func != NULL) {
      char *els_addr = (char *) _varr_addr (&bin->els);
      htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
      for (i = bin->els_start; i < bin->els_bound; i++)
        if (hashes_addr[i] != HTAB_DELETED_HASH) htab->free_func (els_addr + i * el_size, arg);
    }
    if (i >= new_bins_num) {
      _htab_destroy_bin (bin);
      continue;
    }
    bin->els_start = bin->els_bound = 0;
    bin->depth = new_bin_power2;
    VARR_SET_LENGTH (htab_ind_t, bin->entries, new_bin_size * 2);
    _varr_set_length (&bin->els, el_size, new_bin_size);
    VARR_SET_LENGTH (htab_hash_t, bin->hashes, new_bin_size);
    htab_ind_t *addr = VARR_ADDR (htab_ind_t, bin->entries);
    for (htab_size_t j = 0; j < 2 * new_bin_size; j++) addr[j] = HTAB_EMPTY_IND;
  }
  VARR_SET_LENGTH (hbin_ind_t, htab->dir, new_bins_num);
  VARR_SET_LENGTH (hbin_t, htab->bins, new_bins_num);
  htab->max_depth = new_bin_power2;
  htab->els_num = 0;
}

static inline void _htab_destroy (htab_t *htab, size_t el_size) {
  HTAB_ASSERT (htab != NULL, "destroy");
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);
  size_t bins_num = VARR_LENGTH (htab_ind_t, htab->bins);
  for (size_t i = 0; i < bins_num; i++) {
    hbin_t *bin = &bins_addr[i];
    if (htab->free_func != NULL) {
      char *els_addr = (char *) _varr_addr (&bin->els);
      htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
      htab_size_t i;
      void *arg = htab->arg;
      for (i = bin->els_start; i < bin->els_bound; i++)
        if (hashes_addr[i] != HTAB_DELETED_HASH) htab->free_func (els_addr + i * el_size, arg);
    }
    _htab_destroy_bin (bin);
  }
  VARR_DESTROY (hbin_t, htab->bins);
  VARR_DESTROY (hbin_t, htab->dir);
  htab->els_num = 0;
}

__attribute__((always_inline))
static inline bool _htab_do_1 (htab_t *htab, hbin_t *bin, htab_hash_t hash, size_t el_size,
                               void *el, enum htab_action action, void **res) {
  htab_ind_t el_ind, *entry, *first_deleted_entry = NULL;
  htab_ind_t *addr;
  htab_hash_t *hashes_addr;
  char *els_addr;
  htab_size_t ind, mask = (htab_size_t) VARR_LENGTH (htab_ind_t, bin->entries) - 1;
  void *arg = htab->arg;
  htab_hash_t peterb = hash;

  ind = hash & mask;
  addr = VARR_ADDR (htab_ind_t, bin->entries);
  els_addr = (char *) _varr_addr (&bin->els);
  hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
  for (;; htab->collisions++) {
    entry = addr + ind;
    el_ind = *entry;
    if (el_ind != HTAB_EMPTY_IND) {
      el_ind--;
      if (hashes_addr[el_ind] == HTAB_DELETED_HASH) {
        first_deleted_entry = entry;
      } else {
        if (hashes_addr[el_ind] == hash
            && (*htab->eq_func) (els_addr + el_ind * el_size, el, arg)) {
          if (action == HTAB_REPLACE && htab->free_func != NULL)
            htab->free_func (els_addr + el_ind * el_size, arg);
          if (action != HTAB_DELETE) {
            *res = els_addr + el_ind * el_size;
          } else {
            htab->els_num--;
            if (htab->free_func != NULL) htab->free_func (els_addr + el_ind * el_size, arg);
            hashes_addr[el_ind] = HTAB_DELETED_HASH;
          }
          return true;
        }
      }
    } else {
      if (action == HTAB_INSERT || action == HTAB_REPLACE) {
        htab->els_num++;
        if (first_deleted_entry != NULL) entry = first_deleted_entry;
        hashes_addr[bin->els_bound] = hash;
        *res = els_addr + bin->els_bound * el_size;
        *entry = ++bin->els_bound;
      }
      return false;
    }
    peterb >>= 11;
    ind = (5 * ind + peterb + 1) & mask;
  }
}

static inline void _htab_split_bin (htab_t *htab, size_t el_size, hbin_ind_t bin_ind) {
  hbin_t *bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);
  htab_size_t size = VARR_LENGTH (htab_hash_t, bin->hashes);
  size_t new_ind = _htab_create_bin (htab, size, el_size);
  for (;;) {
    hbin_t *new_bin = VARR_ELADDR (hbin_t, htab->bins, new_ind);
    bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);
    htab_ind_t *addr = VARR_ADDR (htab_ind_t, bin->entries);
    for (htab_size_t i = 0; i < size * 2; i++) addr[i] = HTAB_EMPTY_IND;
    htab_hash_t split_mask = 1 << bin->depth;
    new_bin->depth = ++bin->depth;
    hbin_ind_t *dir_addr = VARR_ADDR (hbin_ind_t, htab->dir);
    if (bin->depth > htab->max_depth) { /* double the directory */
      htab->max_depth = bin->depth;
      htab->bin_mask = ~(~(htab_hash_t) 0 << htab->max_depth);
      size_t len = VARR_LENGTH (hbin_ind_t, htab->dir);
      VARR_SET_LENGTH (hbin_ind_t, htab->dir, 2 * len);
      dir_addr = VARR_ADDR (hbin_ind_t, htab->dir);
      for (size_t j = 0; j < len; j++) dir_addr[j + len] = dir_addr[j];
    }
    new_bin->mask = bin->mask | split_mask;
    dir_addr[new_bin->mask] = new_ind;
    char *els_addr = (char *) _varr_addr (&bin->els);
    htab_size_t start = bin->els_start, bound = bin->els_bound;
    htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
    bin->els_start = bin->els_bound = 0;
    htab_size_t els_num = htab->els_num;
    bool old_added = false, new_added = false;
    for (htab_size_t i = start; i < bound; i++) {
      htab_hash_t hash = hashes_addr[i];
      if (hash == HTAB_DELETED_HASH) continue;
      void *res;
      bool old = (hash & split_mask) == 0;
      if (old)
        old_added = true;
      else
        new_added = true;
      bool exists_p = _htab_do_1 (htab, old ? bin : new_bin, hash, el_size, els_addr + i * el_size,
                                  HTAB_INSERT, &res);
      HTAB_ASSERT (!exists_p, "do split");
      memcpy (res, els_addr + i * el_size, el_size);
    }
    htab->els_num = els_num;
    if (!old_added) {
      htab_ind_t temp = bin_ind;
      bin_ind = new_ind;
      new_ind = temp;
    } else if (new_added) {
      break;
    }
  }
}

__attribute__((always_inline))
static inline bool _htab_do (htab_t *htab, size_t el_size, void *el, enum htab_action action,
                             void **res) {
  htab_size_t els_size, size;

  HTAB_ASSERT (htab != NULL, "do htab");
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);
  void *arg = htab->arg;
  htab_hash_t hash = (*htab->hash_func) (el, arg);
  if (hash == HTAB_DELETED_HASH) hash += 1;
  htab_hash_t dir_ind = hash & htab->bin_mask;
  hbin_ind_t bin_ind = VARR_GET (hbin_ind_t, htab->dir, dir_ind);
  hbin_t *bin = &bins_addr[bin_ind];
  size = (htab_size_t) VARR_LENGTH (htab_ind_t, bin->entries);
  els_size = (htab_size_t) _varr_length (&bin->els);
  HTAB_ASSERT (els_size * 2 == size, "do size");
  if ((action == HTAB_INSERT || action == HTAB_REPLACE) && bin->els_bound == els_size) {
    bool grow = false;
    if (2 * htab->els_num >= size) {
      size *= 2;
      els_size *= 2;
      grow = true;
    }
    if (grow && els_size >= (1 << HTAB_MAX_BIN_SIZE_POWER)) {
      _htab_split_bin (htab, el_size, bin_ind);
      bin_ind = VARR_GET (hbin_ind_t, htab->dir, hash & htab->bin_mask);
      bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);
    } else {
      _varr_set_length (&bin->els, el_size, els_size);
      VARR_SET_LENGTH (htab_hash_t, bin->hashes, els_size);
      VARR_SET_LENGTH (htab_ind_t, bin->entries, size);
      htab_ind_t *addr = VARR_ADDR (htab_ind_t, bin->entries);
      for (htab_size_t i = 0; i < size; i++) addr[i] = HTAB_EMPTY_IND;
      htab_size_t start = bin->els_start, bound = bin->els_bound;
      htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
      char *els_addr = (char *) _varr_addr (&bin->els);
      bin->els_start = bin->els_bound = 0;
      htab->els_num = 0;
      for (htab_size_t i = start; i < bound; i++) {
        htab_hash_t hash2 = hashes_addr[i];
        if (hash2 != HTAB_DELETED_HASH) {
          bool exists_p
            = _htab_do_1 (htab, bin, hash2, el_size, els_addr + i * el_size, HTAB_INSERT, res);
          HTAB_ASSERT (!exists_p, "do resize");
          memcpy (*res, els_addr + i * el_size, el_size);
        }
      }
    }
  }
  bool ret = _htab_do_1 (htab, bin, hash, el_size, el, action, res);
  return ret;
}

static inline htab_size_t _htab_els_num (htab_t *htab) {
  HTAB_ASSERT (htab != NULL, "els_num");
  return htab->els_num;
}

static inline htab_size_t _htab_size (htab_t *htab) {
  HTAB_ASSERT (htab != NULL, "els_size");
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);
  size_t bins_num = VARR_LENGTH (htab_ind_t, htab->bins);
  htab_size_t size = 0;
  for (size_t i = 0; i < bins_num; i++) {
    hbin_t *bin = &bins_addr[i];
    size += VARR_LENGTH (htab_hash_t, bin->hashes);
  }
  return size;
}

static inline htab_size_t _htab_collisions (htab_t *htab) {
  HTAB_ASSERT (htab != NULL, "collisions");
  return htab->collisions;
}

static inline void _htab_foreach_elem (htab_t *htab, size_t el_size,
                                       void (*func) (void *el, void *arg), void *arg) {
  HTAB_ASSERT (htab != NULL, "foreach_elem");
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);
  size_t bins_num = VARR_LENGTH (htab_ind_t, htab->bins);
  for (size_t i = 0; i < bins_num; i++) {
    hbin_t *bin = &bins_addr[i];
    char *els_addr = (char *) _varr_addr (&bin->els);
    htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);
    for (htab_size_t i = bin->els_start; i < bin->els_bound; i++)
      if (hashes_addr[i] != HTAB_DELETED_HASH) func (els_addr + i * el_size, arg);
  }
}

#define HTAB_CREATE(T, V, S, H, EQ, A) (_htab_create (&(V), sizeof (T), S, H, EQ, NULL, A))
#define HTAB_CREATE_WITH_FREE_FUNC(T, V, S, H, EQ, F, A) \
  (_htab_create (&(V), sizeof (T), S, H, EQ, F, A))
#define HTAB_CLEAR(T, V, S) (_htab_clear (&(V), sizeof (T), S))
#define HTAB_DESTROY(T, V) (_htab_destroy (&(V), sizeof (T)))
/* It returns true if the element existed in the table.  */
#define HTAB_DO(T, V, EL, A, FLAG, TAB_EL)                              \
  do {                                                                  \
    void *tab_el_addr;                                                  \
    if (((FLAG) = _htab_do (&(V), sizeof (T), &EL, A, &tab_el_addr))) { \
      if ((A) == HTAB_REPLACE) *(T *) tab_el_addr = (EL);               \
      if ((A) != HTAB_DELETE) TAB_EL = *(T *) tab_el_addr;              \
    } else if ((A) == HTAB_INSERT || (A) == HTAB_REPLACE) {             \
      *(T *) tab_el_addr = (EL);                                        \
      TAB_EL = *(T *) tab_el_addr;                                      \
    }                                                                   \
  } while (0)
#define HTAB_ELS_NUM(T, V) (_htab_els_num (&(V)))
#define HTAB_SIZE(T, V) (_htab_size (&(V)))
#define HTAB_COLLISIONS(T, V) (_htab_collisions (&(V)))
#define HTAB_FOREACH_ELEM(T, V, F, A) (_htab_foreach_elem (&(V), sizeof (T), F, A))

/* Generate per-NAME versions of _htab_do_1, _htab_split_bin, _htab_do
   with HASH_FUNC/EQ_FUNC inlined instead of going through function pointers.  */
#define EHTAB_INIT(NAME, HASH_FUNC, EQ_FUNC)                                                      \
                                                                                                    \
__attribute__((always_inline))                                                                      \
static inline bool _htab_do_1_##NAME (htab_t *htab, hbin_t *bin, htab_hash_t hash,                 \
                                       size_t el_size, void *el,                                    \
                                       enum htab_action action, void **res) {                       \
  htab_ind_t el_ind, *entry, *first_deleted_entry = NULL;                                           \
  htab_ind_t *addr;                                                                                 \
  htab_hash_t *hashes_addr;                                                                         \
  char *els_addr;                                                                                   \
  htab_size_t ind, mask = (htab_size_t) VARR_LENGTH (htab_ind_t, bin->entries) - 1;                 \
  void *arg = htab->arg;                                                                            \
  htab_hash_t peterb = hash;                                                                        \
                                                                                                    \
  ind = hash & mask;                                                                                \
  addr = VARR_ADDR (htab_ind_t, bin->entries);                                                      \
  els_addr = (char *) _varr_addr (&bin->els);                                                       \
  hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);                                               \
  for (;; htab->collisions++) {                                                                     \
    entry = addr + ind;                                                                             \
    el_ind = *entry;                                                                                \
    if (el_ind != HTAB_EMPTY_IND) {                                                                 \
      el_ind--;                                                                                     \
      if (hashes_addr[el_ind] == HTAB_DELETED_HASH) {                                               \
        first_deleted_entry = entry;                                                                \
      } else {                                                                                      \
        if (hashes_addr[el_ind] == hash                                                             \
            && EQ_FUNC (els_addr + el_ind * el_size, el, arg)) {                                    \
          if (action == HTAB_REPLACE && htab->free_func != NULL)                                    \
            htab->free_func (els_addr + el_ind * el_size, arg);                                     \
          if (action != HTAB_DELETE) {                                                               \
            *res = els_addr + el_ind * el_size;                                                     \
          } else {                                                                                  \
            htab->els_num--;                                                                        \
            if (htab->free_func != NULL) htab->free_func (els_addr + el_ind * el_size, arg);        \
            hashes_addr[el_ind] = HTAB_DELETED_HASH;                                                \
          }                                                                                         \
          return true;                                                                              \
        }                                                                                           \
      }                                                                                             \
    } else {                                                                                        \
      if (action == HTAB_INSERT || action == HTAB_REPLACE) {                                        \
        htab->els_num++;                                                                            \
        if (first_deleted_entry != NULL) entry = first_deleted_entry;                                \
        hashes_addr[bin->els_bound] = hash;                                                         \
        *res = els_addr + bin->els_bound * el_size;                                                 \
        *entry = ++bin->els_bound;                                                                  \
      }                                                                                             \
      return false;                                                                                 \
    }                                                                                               \
    peterb >>= 11;                                                                                  \
    ind = (5 * ind + peterb + 1) & mask;                                                            \
  }                                                                                                 \
}                                                                                                   \
                                                                                                    \
static inline void _htab_split_bin_##NAME (htab_t *htab, size_t el_size,                            \
                                            hbin_ind_t bin_ind) {                                    \
  hbin_t *bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);                                          \
  htab_size_t size = VARR_LENGTH (htab_hash_t, bin->hashes);                                        \
  size_t new_ind = _htab_create_bin (htab, size, el_size);                                           \
  for (;;) {                                                                                        \
    hbin_t *new_bin = VARR_ELADDR (hbin_t, htab->bins, new_ind);                                    \
    bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);                                                \
    htab_ind_t *addr = VARR_ADDR (htab_ind_t, bin->entries);                                        \
    for (htab_size_t i = 0; i < size * 2; i++) addr[i] = HTAB_EMPTY_IND;                            \
    htab_hash_t split_mask = 1 << bin->depth;                                                       \
    new_bin->depth = ++bin->depth;                                                                  \
    hbin_ind_t *dir_addr = VARR_ADDR (hbin_ind_t, htab->dir);                                      \
    if (bin->depth > htab->max_depth) {                                                             \
      htab->max_depth = bin->depth;                                                                 \
      htab->bin_mask = ~(~(htab_hash_t) 0 << htab->max_depth);                                     \
      size_t len = VARR_LENGTH (hbin_ind_t, htab->dir);                                             \
      VARR_SET_LENGTH (hbin_ind_t, htab->dir, 2 * len);                                             \
      dir_addr = VARR_ADDR (hbin_ind_t, htab->dir);                                                \
      for (size_t j = 0; j < len; j++) dir_addr[j + len] = dir_addr[j];                             \
    }                                                                                               \
    new_bin->mask = bin->mask | split_mask;                                                          \
    dir_addr[new_bin->mask] = new_ind;                                                              \
    char *els_addr = (char *) _varr_addr (&bin->els);                                                \
    htab_size_t start = bin->els_start, bound = bin->els_bound;                                     \
    htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);                                \
    bin->els_start = bin->els_bound = 0;                                                            \
    htab_size_t els_num = htab->els_num;                                                            \
    bool old_added = false, new_added = false;                                                      \
    for (htab_size_t i = start; i < bound; i++) {                                                   \
      htab_hash_t hash = hashes_addr[i];                                                            \
      if (hash == HTAB_DELETED_HASH) continue;                                                      \
      void *res;                                                                                    \
      bool old = (hash & split_mask) == 0;                                                          \
      if (old)                                                                                      \
        old_added = true;                                                                           \
      else                                                                                          \
        new_added = true;                                                                           \
      bool exists_p = _htab_do_1_##NAME (htab, old ? bin : new_bin, hash, el_size,                  \
                                          els_addr + i * el_size, HTAB_INSERT, &res);                \
      HTAB_ASSERT (!exists_p, "do split");                                                          \
      memcpy (res, els_addr + i * el_size, el_size);                                                \
    }                                                                                               \
    htab->els_num = els_num;                                                                        \
    if (!old_added) {                                                                               \
      htab_ind_t temp = bin_ind;                                                                    \
      bin_ind = new_ind;                                                                            \
      new_ind = temp;                                                                               \
    } else if (new_added) {                                                                         \
      break;                                                                                        \
    }                                                                                               \
  }                                                                                                 \
}                                                                                                   \
                                                                                                    \
__attribute__((always_inline))                                                                      \
static inline bool _htab_do_##NAME (htab_t *htab, size_t el_size, void *el,                         \
                                     enum htab_action action, void **res) {                          \
  htab_size_t els_size, size;                                                                       \
                                                                                                    \
  HTAB_ASSERT (htab != NULL, "do htab");                                                            \
  hbin_t *bins_addr = VARR_ADDR (hbin_t, htab->bins);                                               \
  void *arg = htab->arg;                                                                            \
  htab_hash_t hash = HASH_FUNC (el, arg);                                                           \
  if (hash == HTAB_DELETED_HASH) hash += 1;                                                         \
  htab_hash_t dir_ind = hash & htab->bin_mask;                                                      \
  hbin_ind_t bin_ind = VARR_GET (hbin_ind_t, htab->dir, dir_ind);                                   \
  hbin_t *bin = &bins_addr[bin_ind];                                                                \
  size = (htab_size_t) VARR_LENGTH (htab_ind_t, bin->entries);                                      \
  els_size = (htab_size_t) _varr_length (&bin->els);                                                \
  HTAB_ASSERT (els_size * 2 == size, "do size");                                                    \
  if ((action == HTAB_INSERT || action == HTAB_REPLACE) && bin->els_bound == els_size) {             \
    bool grow = false;                                                                              \
    if (2 * htab->els_num >= size) {                                                                \
      size *= 2;                                                                                    \
      els_size *= 2;                                                                                \
      grow = true;                                                                                  \
    }                                                                                               \
    if (grow && els_size >= (1 << HTAB_MAX_BIN_SIZE_POWER)) {                                       \
      _htab_split_bin_##NAME (htab, el_size, bin_ind);                                               \
      bin_ind = VARR_GET (hbin_ind_t, htab->dir, hash & htab->bin_mask);                             \
      bin = VARR_ELADDR (hbin_t, htab->bins, bin_ind);                                              \
    } else {                                                                                        \
      _varr_set_length (&bin->els, el_size, els_size);                                               \
      VARR_SET_LENGTH (htab_hash_t, bin->hashes, els_size);                                          \
      VARR_SET_LENGTH (htab_ind_t, bin->entries, size);                                              \
      htab_ind_t *addr = VARR_ADDR (htab_ind_t, bin->entries);                                      \
      for (htab_size_t i = 0; i < size; i++) addr[i] = HTAB_EMPTY_IND;                              \
      htab_size_t start = bin->els_start, bound = bin->els_bound;                                    \
      htab_hash_t *hashes_addr = VARR_ADDR (htab_hash_t, bin->hashes);                              \
      char *els_addr = (char *) _varr_addr (&bin->els);                                              \
      bin->els_start = bin->els_bound = 0;                                                          \
      htab->els_num = 0;                                                                            \
      for (htab_size_t i = start; i < bound; i++) {                                                 \
        htab_hash_t hash2 = hashes_addr[i];                                                         \
        if (hash2 != HTAB_DELETED_HASH) {                                                           \
          bool exists_p                                                                             \
            = _htab_do_1_##NAME (htab, bin, hash2, el_size, els_addr + i * el_size,                 \
                                  HTAB_INSERT, res);                                                 \
          HTAB_ASSERT (!exists_p, "do resize");                                                     \
          memcpy (*res, els_addr + i * el_size, el_size);                                            \
        }                                                                                           \
      }                                                                                             \
    }                                                                                               \
  }                                                                                                 \
  bool ret = _htab_do_1_##NAME (htab, bin, hash, el_size, el, action, res);                          \
  return ret;                                                                                       \
}

#endif /* #ifndef HTAB_H */
