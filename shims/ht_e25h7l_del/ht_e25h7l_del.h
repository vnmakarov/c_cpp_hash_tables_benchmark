#ifndef HT_E25H7L_DEL_H
#define HT_E25H7L_DEL_H

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>

typedef uint32_t ht_e25h7l_del_entry_t;
typedef unsigned long ht_e25h7l_del_size_t;
typedef size_t ht_e25h7l_del_hash_t;

static constexpr ht_e25h7l_del_entry_t HT_E25H7L_DEL_EMPTY_IND = 0xfffffffe;
static constexpr ht_e25h7l_del_entry_t HT_E25H7L_DEL_DELETED_IND = 0xffffffff;
static constexpr unsigned int HT_E25H7L_DEL_IND_MASK = 0x1ffffff;

enum ht_e25h7l_del_action { HT_E25H7L_DEL_FIND, HT_E25H7L_DEL_INSERT, HT_E25H7L_DEL_REPLACE, HT_E25H7L_DEL_DELETE };

template<typename El>
struct hbin_e25h7l_del_t {
  ht_e25h7l_del_size_t els_start, els_bound;
  El *els;
  char *deleted;
  ht_e25h7l_del_entry_t *entries;
  ht_e25h7l_del_size_t entries_mask;
};

template<typename El, typename Hash, typename Eq>
struct ht_e25h7l_del_t {
  ht_e25h7l_del_size_t els_num;
  hbin_e25h7l_del_t<El> bin;

  static void create (ht_e25h7l_del_t *htab, ht_e25h7l_del_size_t min_size) {
    ht_e25h7l_del_size_t size;
    for (size = 2; min_size > size; size *= 2);
    htab->els_num = 0;
    auto &b = htab->bin;
    b.els_start = b.els_bound = 0;
    b.els = (El *) std::malloc (size * sizeof (El));
    ht_e25h7l_del_size_t del_bytes = (size + 7) / 8;
    b.deleted = (char *) std::calloc (del_bytes, 1);
    b.entries = (ht_e25h7l_del_entry_t *) std::malloc (2 * size * sizeof (ht_e25h7l_del_entry_t));
    b.entries_mask = 2 * size - 1;
    for (ht_e25h7l_del_size_t i = 0; i <= b.entries_mask; i++) b.entries[i] = HT_E25H7L_DEL_EMPTY_IND;
  }

  static void destroy_bin (hbin_e25h7l_del_t<El> &b) {
    std::free (b.els);
    std::free (b.deleted);
    std::free (b.entries);
  }

  static void destroy (ht_e25h7l_del_t *htab) {
    destroy_bin (htab->bin);
    htab->els_num = 0;
  }

  __attribute__((always_inline))
  static bool do_1 (ht_e25h7l_del_t *htab, hbin_e25h7l_del_t<El> &bin, El &el,
                    enum ht_e25h7l_del_action action, El **res) {
    ht_e25h7l_del_entry_t *entry, *first_deleted_entry = nullptr;
    ht_e25h7l_del_size_t mask = bin.entries_mask;
    Hash hash_fn;
    Eq eq_fn;
    ht_e25h7l_del_hash_t hash = hash_fn (el);
    uint32_t h7_val = (uint32_t) ((hash >> (sizeof (size_t) * 8 - 7)) & 0x7f) << 25;

    ht_e25h7l_del_size_t ind = hash & mask;
    for (;;) {
      entry = bin.entries + ind;
      ht_e25h7l_del_entry_t entry_val = *entry;
      if (entry_val < HT_E25H7L_DEL_EMPTY_IND) {
        if ((entry_val & ~HT_E25H7L_DEL_IND_MASK) == h7_val
            && eq_fn (bin.els[entry_val & HT_E25H7L_DEL_IND_MASK], el)) {
          if (action != HT_E25H7L_DEL_DELETE) {
            *res = &bin.els[entry_val & HT_E25H7L_DEL_IND_MASK];
          } else {
            htab->els_num--;
            bin.deleted[(entry_val & HT_E25H7L_DEL_IND_MASK) / 8]
              |= 1 << ((entry_val & HT_E25H7L_DEL_IND_MASK) % 8);
            *entry = HT_E25H7L_DEL_DELETED_IND;
          }
          return true;
        }
      } else if (entry_val != HT_E25H7L_DEL_DELETED_IND) {
        if (action == HT_E25H7L_DEL_INSERT || action == HT_E25H7L_DEL_REPLACE) {
          htab->els_num++;
          if (first_deleted_entry != nullptr) entry = first_deleted_entry;
          *res = &bin.els[bin.els_bound];
          *entry = h7_val | (uint32_t) bin.els_bound;
          bin.els_bound++;
        }
        return false;
      } else {
        first_deleted_entry = entry;
      }
      ind = (ind + 1) & mask;
    }
  }

  __attribute__((always_inline))
  static bool do_ (ht_e25h7l_del_t *htab, El &el, enum ht_e25h7l_del_action action, El **res) {
    ht_e25h7l_del_size_t els_size = (htab->bin.entries_mask + 1) / 2;
    if (action != HT_E25H7L_DEL_DELETE && htab->bin.els_bound >= els_size) {
      ht_e25h7l_del_size_t size = htab->bin.entries_mask + 1;
      if (2 * htab->els_num >= size) {
        size *= 2;
        els_size *= 2;
      }
      hbin_e25h7l_del_t<El> resize_bin;
      resize_bin.els = (El *) std::malloc (els_size * sizeof (El));
      ht_e25h7l_del_size_t del_bytes = (els_size + 7) / 8;
      resize_bin.deleted = (char *) std::calloc (del_bytes, 1);
      resize_bin.entries = (ht_e25h7l_del_entry_t *) std::malloc (size * sizeof (ht_e25h7l_del_entry_t));
      resize_bin.entries_mask = size - 1;
      for (ht_e25h7l_del_size_t j = 0; j < size; j++) resize_bin.entries[j] = HT_E25H7L_DEL_EMPTY_IND;
      resize_bin.els_start = resize_bin.els_bound = 0;
      ht_e25h7l_del_size_t bound = htab->bin.els_bound;
      ht_e25h7l_del_size_t saved_els_num = htab->els_num;
      for (ht_e25h7l_del_size_t i = htab->bin.els_start; i < bound; i++)
        if (!(htab->bin.deleted[i / 8] & (1 << (i % 8)))) {
          El *r;
          do_1 (htab, resize_bin, htab->bin.els[i], HT_E25H7L_DEL_INSERT, &r);
          *r = htab->bin.els[i];
        }
      htab->els_num = saved_els_num;
      destroy_bin (htab->bin);
      htab->bin = resize_bin;
    }
    return do_1 (htab, htab->bin, el, action, res);
  }

  static ht_e25h7l_del_size_t size (ht_e25h7l_del_t *htab) {
    return (htab->bin.entries_mask + 1) / 2;
  }
};

#endif /* #ifndef HT_E25H7L_DEL_H */
