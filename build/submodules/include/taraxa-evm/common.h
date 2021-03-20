#ifndef TARAXA_EVM_COMMON_H
#define TARAXA_EVM_COMMON_H

#include <stdint.h>

#define SLICE(name, type) typedef struct { type *Data; size_t Len; } name
#define ARRAY(name, type, size) typedef struct { type Val[size]; } name
#define FUNCTION(name, in_t, out_t) \
    typedef struct {  \
        void *Receiver; \
        out_t (*Apply)(void *, in_t); \
    } name; \
    inline out_t name##Apply(name fn, in_t arg) { return fn.Apply(fn.Receiver, arg); } \

SLICE(taraxa_evm_Bytes, uint8_t);
ARRAY(taraxa_evm_Hash, uint8_t, 32);
ARRAY(taraxa_evm_Addr, uint8_t, 20);
FUNCTION(taraxa_evm_BytesCallback, taraxa_evm_Bytes, void);
FUNCTION(taraxa_evm_GetBlockHash, uint64_t, taraxa_evm_Hash);

#undef SLICE
#undef ARRAY
#undef FUNCTION

#endif