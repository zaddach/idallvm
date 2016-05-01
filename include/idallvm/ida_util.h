#ifndef _IDALLVM_IDAUTIL_H
#define _IDALLVM_IDAUTIL_H

#if !defined(__cplusplus)
#error Needs to be compiled with c++
#endif /* !defined(__cplusplus) */

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

bool ida_is_graphical_mode(void);
ProcessorInformation ida_get_processor_information(void);

#endif /* _IDALLVM_IDAUTIL_H */
