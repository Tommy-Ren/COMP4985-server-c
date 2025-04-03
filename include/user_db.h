#ifndef USER_DB_H
#define USER_DB_H
#include <inttypes.h>
#include <string.h>    // for strlen

#ifdef __APPLE__
    #include <ndbm.h>
typedef size_t datum_size;
#elif defined(__FreeBSD__)
    #include <gdbm.h>
typedef int datum_size;
#else
    /* Assume Linux/Ubuntu; using gdbm_compat which provides an ndbm-like interface */
    #include <ndbm.h>
typedef int datum_size;
#endif

#define TO_SIZE_T(x) ((size_t)(x))

typedef struct
{
    // cppcheck-suppress unusedStructMember
    const void *dptr;
    // cppcheck-suppress unusedStructMember
    datum_size dsize;
} const_datum;

#define MAKE_CONST_DATUM(str) ((const_datum){(str), (datum_size)strlen(str) + 1})
#define MAKE_CONST_DATUM_BYTE(str, size) ((const_datum){(str), (datum_size)(size)})

typedef struct DBO
{
    char *name;    // cppcheck-suppress unusedStructMember
    DBM  *db;      // cppcheck-suppress unusedStructMember
} DBO;

/* Opens the database specified in dbo->name in read/write mode (creating it if needed).
   Returns 0 on success, -1 on failure. */
ssize_t database_open(DBO *dbo);

/* Stores the string value under the key into the given DBM.
   Returns 0 on success, -1 on failure. */
int store_string(DBM *db, const char *key, const char *value);

/* Stores an integer value in the DBM.
   Returns 0 on success, -1 on failure. */
int store_int(DBM *db, const char *key, int value);

/* Stores raw bytes in the DBM.
   Returns 0 on success, -1 on failure. */
int store_byte(DBM *db, const void *key, size_t k_size, const void *value, size_t v_size);

/* Retrieves the stored string value associated with key from the given DBM.
   On success, returns a pointer to a newly allocated copy of the value (which must be freed by the caller);
   returns NULL if the key is not found or on error. */
char *retrieve_string(DBM *db, const char *key);

/* Retrieves an integer from the DBM.
   Returns 0 on success, -1 on failure. */
int retrieve_int(DBM *db, const char *key, int *result);

/* Retrieves raw bytes from the DBM.
   Returns pointer on success, or NULL if not found. */
void *retrieve_byte(DBM *db, const void *key, size_t size);

/* Initializes the primary key in the database. Returns 0 on success, -1 on failure. */
ssize_t init_pk(DBO *dbo, const char *pk_name);

#endif    // USER_DB_H
