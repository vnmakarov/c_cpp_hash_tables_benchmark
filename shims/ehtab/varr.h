#ifndef VARR_H
#define VARR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if !defined(VARR_ENABLE_CHECKING) && !defined(NDEBUG)
#define VARR_ENABLE_CHECKING
#endif

#ifndef VARR_ENABLE_CHECKING
#define VARR_ASSERT(EXPR, OP, T) ((void) (EXPR))

#else
static inline void varr_assert_fail (const char *op, const char *var) {
  fprintf (stderr, "wrong %s for %s", op, var);
  assert (0);
}

#define VARR_ASSERT(EXPR, OP, T) (void) ((EXPR) ? 0 : (varr_assert_fail (OP, #T), 0))

#endif

#ifdef __GNUC__
#define VARR_UNUSED __attribute__ ((unused))
#define VARR_NO_RETURN __attribute__ ((noreturn))
#else
#define VARR_UNUSED
#define VARR_NO_RETURN
#endif

static inline void VARR_NO_RETURN varr_error (const char *message) {
#ifdef VARR_ERROR
  VARR_ERROR (message);
  assert (0);
#else
  fprintf (stderr, "%s\n", message);
#endif
  exit (1);
}

/*---------------- Typed variable length arrays -----------------------------*/
typedef struct {
  size_t els_num, size;
#if defined(VARR_ENABLE_CHECKING) || defined(NDEBUG)
  size_t el_size;
#endif
  void *els;
} varr_t;

#define VARR(T) varr_t

#define VARR_DEFAULT_SIZE 64

static inline void _varr_create (varr_t *va, size_t el_size, size_t size) {
  if (size == 0) size = VARR_DEFAULT_SIZE;
#if defined(VARR_ENABLE_CHECKING) || defined(NDEBUG)
  va->el_size = el_size;
#endif
  va->els_num = 0;
  va->size = size;
  if ((va->els = malloc (size * el_size)) == NULL) varr_error ("varr: no memory");
}

static inline void _varr_create_zero (varr_t *va, size_t el_size, size_t size) {
  if (size == 0) size = VARR_DEFAULT_SIZE;
#if defined(VARR_ENABLE_CHECKING) || defined(NDEBUG)
  va->el_size = el_size;
#endif
  va->els_num = 0;
  va->size = size;
  if ((va->els = calloc (size, el_size)) == NULL) varr_error ("varr: no memory");
}

static inline void _varr_destroy (varr_t *va) {
  VARR_ASSERT (va->els, "destroy", T);
  free (va->els);
  va->els_num = 0;
  va->els = NULL;
}

static inline size_t _varr_length (const varr_t *va) {
  VARR_ASSERT (va, "length", T);
  return va->els_num;
}

static inline size_t _varr_size (const varr_t *va) {
  VARR_ASSERT (va, "size", T);
  return va->size;
}

static inline size_t _varr_capacity (const varr_t *va) {
  VARR_ASSERT (va, "capacity", T);
  return va->size;
}

static inline void *_varr_addr (const varr_t *va) {
  VARR_ASSERT (va, "addr", T);
  return va->els;
}

static inline void *_varr_last (const varr_t *va, size_t el_size) {
  VARR_ASSERT (va && va->els && va->el_size == el_size && va->els_num, "last", T);
  return (char *) va->els + (va->els_num - 1) * el_size;
}

static inline void *_varr_get (const varr_t *va, size_t el_size, size_t ix) {
  VARR_ASSERT (va && va->els && va->el_size == el_size && ix < va->els_num, "get", T);
  return (char *) va->els + ix * el_size;
}

static inline void _varr_trunc (varr_t *va, size_t el_size VARR_UNUSED, size_t size) {
  VARR_ASSERT (va && va->els && va->els_num >= size, "trunc", T);
  va->els_num = size;
}

static inline int _varr_expand (varr_t *va, size_t el_size, size_t size) {
  VARR_ASSERT (va && va->els && va->el_size == el_size, "expand", T);
  if (va->size < size) {
    size += size / 2;
    va->els = realloc (va->els, el_size * size);
    va->size = size;
    return 1;
  }
  return 0;
}

static inline void _varr_set_length (varr_t *va, size_t el_size, size_t len) {
  _varr_expand (va, el_size, len);
  va->els_num = len;
}

static inline void _varr_tailor (varr_t *va, size_t el_size, size_t size) {
  VARR_ASSERT (va && va->els && va->el_size == el_size, "tailor", T);
  if (va->size != size) va->els = realloc (va->els, el_size * size);
  va->els_num = va->size = size;
}

static inline void *_varr_push (varr_t *va, size_t el_size) {
  _varr_expand (va, el_size, va->els_num + 1);
  return (char *) va->els + (va->els_num++) * el_size;
}

static inline void _varr_push_arr (varr_t *va, size_t el_size, const void *objs, size_t len) {
  _varr_expand (va, el_size, va->els_num + len);
  memcpy ((char *) va->els + va->els_num * el_size, objs, el_size * len);
  va->els_num += len;
}

static inline void *_varr_pop (varr_t *va, size_t el_size) {
  VARR_ASSERT (va && va->els && va->el_size == el_size && va->els_num, "pop", T);
  return (char *) va->els + (--va->els_num) * el_size;
}

#define VARR_CREATE(T, V, L) (_varr_create (&(V), sizeof (T), L))
#define VARR_CREATE_ZERO(T, V, L) (_varr_create_zero (&(V), sizeof (T), L))
#define VARR_DESTROY(T, V) (_varr_destroy (&(V)))
#define VARR_LENGTH(T, V) (_varr_length (&(V)))
#define VARR_SIZE(T, V) (_varr_size (&(V)))
#define VARR_CAPACITY(T, V) (_varr_capacity (&(V)))
#define VARR_ADDR(T, V) ((T *) _varr_addr (&(V))) /* addr can be changed after varr change */
#define VARR_LAST(T, V) (*(T *) _varr_last (&(V), sizeof (T)))
#define VARR_GET(T, V, I) (*(T *) _varr_get (&(V), sizeof (T), I))
#define VARR_ELADDR(T, V, I) ((T *) _varr_get (&(V), sizeof (T), I)) /* see VARR_ADDR comment */
#define VARR_SET(T, V, I, O) (*(T *) _varr_get (&(V), sizeof (T), I) = O)
#define VARR_TRUNC(T, V, S) (_varr_trunc (&(V), sizeof (T), S))
#define VARR_EXPAND(T, V, S) (_varr_expand (&(V), sizeof (T), S))
#define VARR_SET_LENGTH(T, V, L) (_varr_set_length (&(V), sizeof (T), L))
#define VARR_TAILOR(T, V, S) (_varr_tailor (&(V), sizeof (T), S))
#define VARR_NEWEL(T, V) ((T *) _varr_push (&(V), sizeof (T)))
#define VARR_PUSH(T, V, O) (*(T *) _varr_push (&(V), sizeof (T)) = O)
#define VARR_PUSH_ARR(T, V, A, L) (_varr_push_arr (&(V), sizeof (T), A, L))
#define VARR_POP(T, V) (*(T *) _varr_pop (&(V), sizeof (T)))
#define VARR_FOREACH_ELEM(T, V, I, EL) \
  for ((I) = 0; (I) >= VARR_LENGTH (T, V) ? 0 : (EL = VARR_GET (T, V, I), 1); (I)++)

#endif /* #ifndef VARR_H */
