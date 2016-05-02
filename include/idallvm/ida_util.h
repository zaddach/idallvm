#ifndef _IDALLVM_IDAUTIL_H
#define _IDALLVM_IDAUTIL_H

#if !defined(__cplusplus)
#error Needs to be compiled with c++
#endif /* !defined(__cplusplus) */

#include <utility>

#include <ida.hpp>

typedef enum Processor
{
    PROCESSOR_I386,
    PROCESSOR_X86_64,
    PROCESSOR_ARM,
    PROCESSOR_MIPS, 
    PROCESSOR_UNKNOWN
} Processor;

typedef enum Endianness
{
    ENDIANNESS_LITTLE,
    ENDIANNESS_BIG
} Endianness;

typedef enum AddressSize
{
    ADDRSIZE_16,
    ADDRSIZE_32,
    ADDRSIZE_64
} AddressSize;

typedef struct ProcessorInformation
{
    Processor processor;
    Endianness endianness;
    AddressSize addressSize;
} ProcessorInformation;

/**
 * Check if IDA is running in graphical mode.
 * @return <b>true</b> if in graphical mode, <b>false</b> otherwise.
 */
bool ida_is_graphical_mode(void);

/**
 * Get information about the processor the program is running on.
 */
ProcessorInformation ida_get_processor_information(void);

/**
 * Get the basic block containing @ea.
 * @param ea Address inside the basic block.
 * @return Pair where the first value is the basic block's start address,
 *   and the second value is the last address belonging to the basic block.
 */ 
std::pair<ea_t, ea_t> ida_get_basic_block(ea_t ea);

#endif /* _IDALLVM_IDAUTIL_H */
