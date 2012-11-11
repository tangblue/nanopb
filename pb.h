#ifndef _PB_H_
#define _PB_H_

/* pb.h: Common parts for nanopb library.
 * Most of these are quite low-level stuff. For the high-level interface,
 * see pb_encode.h or pb_decode.h
 */

#define NANOPB_VERSION nanopb-0.1.7

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __GNUC__
/* This just reduces memory requirements, but is not required. */
#define pb_packed __attribute__((packed))
#else
#define pb_packed
#endif

/* Handly macro for suppressing unreferenced-parameter compiler warnings.    */
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/* Compile-time assertion, used for checking compatible compilation options. */
#ifndef STATIC_ASSERT
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1];
#endif

/* Number of required fields to keep track of
 * (change here or on compiler command line). */
#ifndef PB_MAX_REQUIRED_FIELDS
#define PB_MAX_REQUIRED_FIELDS 64
#endif

#if PB_MAX_REQUIRED_FIELDS < 64
#error You should not lower PB_MAX_REQUIRED_FIELDS from the default value (64).
#endif

/* List of possible field types. These are used in the autogenerated code.
 * Least-significant 4 bits tell the scalar type
 * Most-significant 4 bits specify repeated/required/packed etc.
 * 
 * INT32 and UINT32 are treated the same, as are (U)INT64 and (S)FIXED*
 * These types are simply casted to correct field type when they are
 * assigned to the memory pointer.
 * SINT* is different, though, because it is zig-zag coded.
 */

typedef enum {
    /************************
     * Field contents types *
     ************************/
    
    /* Numeric types */
    PB_LTYPE_VARINT = 0x00, /* int32, uint32, int64, uint64, bool, enum */
    PB_LTYPE_SVARINT = 0x01, /* sint32, sint64 */
    PB_LTYPE_FIXED32 = 0x02, /* fixed32, sfixed32, float */
    PB_LTYPE_FIXED64 = 0x03, /* fixed64, sfixed64, double */
    
    /* Marker for last packable field type. */
    PB_LTYPE_LAST_PACKABLE = 0x03,
    
    /* Byte array with pre-allocated buffer.
     * data_size is the length of the allocated PB_BYTES_ARRAY structure. */
    PB_LTYPE_BYTES = 0x04,
    
    /* String with pre-allocated buffer.
     * data_size is the maximum length. */
    PB_LTYPE_STRING = 0x05,
    
    /* Submessage
     * submsg_fields is pointer to field descriptions */
    PB_LTYPE_SUBMESSAGE = 0x06,
    
    /* Number of declared LTYPES */
    PB_LTYPES_COUNT = 7,
    PB_LTYPE_MASK = 0x0F,
    
    /******************
     * Modifier flags *
     ******************/
    
    /* Just the basic, write data at data_offset */
    PB_HTYPE_REQUIRED = 0x00,
    
    /* Write true at size_offset */
    PB_HTYPE_OPTIONAL = 0x10,
    
    /* Read to pre-allocated array
     * Maximum number of entries is array_size,
     * actual number is stored at size_offset */
    PB_HTYPE_ARRAY = 0x20,
    
    /* Works for all required/optional/repeated fields.
     * data_offset points to pb_callback_t structure.
     * LTYPE should be 0 (it is ignored, but sometimes
     * used to speculatively index an array). */
    PB_HTYPE_CALLBACK = 0x30,
    
    PB_HTYPE_MASK = 0xF0
} pb_packed pb_type_t;

#define PB_HTYPE(x) ((x) & PB_HTYPE_MASK)
#define PB_LTYPE(x) ((x) & PB_LTYPE_MASK)

/* This structure is used in auto-generated constants
 * to specify struct fields.
 * You can change field sizes if you need structures
 * larger than 256 bytes or field tags larger than 256.
 * The compiler should complain if your .proto has such
 * structures. Fix that by defining PB_FIELD_16BIT or
 * PB_FIELD_32BIT.
 */
typedef struct _pb_field_t pb_field_t;
struct _pb_field_t {

#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
    uint8_t tag;
    pb_type_t type;
    uint8_t data_offset; /* Offset of field data, relative to previous field. */
    int8_t size_offset; /* Offset of array size or has-boolean, relative to data */
    uint8_t data_size; /* Data size in bytes for a single item */
    uint8_t array_size; /* Maximum number of entries in array */
#elif defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
    uint16_t tag;
    pb_type_t type;
    uint8_t data_offset;
    int8_t size_offset;
    uint16_t data_size;
    uint16_t array_size;
#else
    uint32_t tag;
    pb_type_t type;
    uint8_t data_offset;
    int8_t size_offset;
    uint32_t data_size;
    uint32_t array_size;
#endif
    
    /* Field definitions for submessage
     * OR default value for all other non-array, non-callback types
     * If null, then field will zeroed. */
    const void *ptr;
} pb_packed;

/* This structure is used for 'bytes' arrays.
 * It has the number of bytes in the beginning, and after that an array.
 * Note that actual structs used will have a different length of bytes array.
 */
struct _pb_bytes_array_t {
    size_t size;
    uint8_t bytes[1];
};

typedef struct _pb_bytes_array_t pb_bytes_array_t;

/* This structure is used for giving the callback function.
 * It is stored in the message structure and filled in by the method that
 * calls pb_decode.
 *
 * The decoding callback will be given a limited-length stream
 * If the wire type was string, the length is the length of the string.
 * If the wire type was a varint/fixed32/fixed64, the length is the length
 * of the actual value.
 * The function may be called multiple times (especially for repeated types,
 * but also otherwise if the message happens to contain the field multiple
 * times.)
 *
 * The encoding callback will receive the actual output stream.
 * It should write all the data in one call, including the field tag and
 * wire type. It can write multiple fields.
 *
 * The callback can be null if you want to skip a field.
 */
typedef struct _pb_istream_t pb_istream_t;
typedef struct _pb_ostream_t pb_ostream_t;
typedef struct _pb_callback_t pb_callback_t;
struct _pb_callback_t {
    union {
        bool (*decode)(pb_istream_t *stream, const pb_field_t *field, void *arg);
        bool (*encode)(pb_ostream_t *stream, const pb_field_t *field, const void *arg);
    } funcs;
    
    /* Free arg for use by callback */
    void *arg;
};

/* Wire types. Library user needs these only in encoder callbacks. */
typedef enum {
    PB_WT_VARINT = 0,
    PB_WT_64BIT  = 1,
    PB_WT_STRING = 2,
    PB_WT_32BIT  = 5
} pb_wire_type_t;

/* These macros are used to declare pb_field_t's in the constant array. */
#define pb_membersize(st, m) (sizeof ((st*)0)->m)
#define pb_arraysize(st, m) (pb_membersize(st, m) / pb_membersize(st, m[0]))
#define pb_delta(st, m1, m2) ((int)offsetof(st, m1) - (int)offsetof(st, m2))
#define pb_delta_end(st, m1, m2) (offsetof(st, m1) - offsetof(st, m2) - pb_membersize(st, m2))
#define PB_LAST_FIELD {0,(pb_type_t) 0,0,0,0,0,0}

/* These macros are used for giving out error messages.
 * They are mostly a debugging aid; the main error information
 * is the true/false return value from functions.
 * Some code space can be saved by disabling the error
 * messages if not used.
 */
#ifdef PB_NO_ERRMSG
#define PB_RETURN_ERROR(stream,msg) return false
#define PB_GET_ERROR(stream) "(errmsg disabled)"
#else
#define PB_RETURN_ERROR(stream,msg) \
    do {\
        if ((stream)->errmsg == NULL) \
            (stream)->errmsg = (msg); \
        return false; \
    } while(0)
#define PB_GET_ERROR(stream) ((stream)->errmsg ? (stream)->errmsg : "(none)")
#endif

#endif
