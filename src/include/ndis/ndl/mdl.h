/*++

    Copyright (c) Microsoft. All rights reserved.

    This code is licensed under the MIT License.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
    PARTICULAR PURPOSE AND NONINFRINGEMENT.

Module Name:

    mdl.h

Provenance:

    Version 1.1.0 from https://github.com/microsoft/ndis-driver-library

Abstract:

    Utility functions for operating on MDL chains

Data structures:

    An MDL is a data structure that represents a single virtually-contiguous
    region of memory. To represent multiple regions of memory, MDLs can be
    chained into a linked list:

        +------+         +------+         +------+         +------+
        | MDL1 | - - - > | MDL2 | - - - > | MDL3 | - - - > | MDL4 | - - > NULL
        +------+         +------+         +------+         +------+

    Although these buffers are not actually contiguous in virtual memory, it's
    useful to imagine them being contiguous in a conceptual space; the
    conceptual payload of an I/O is the concatenation of each of these
    discontiguous buffers:

                        +------+------+------+------+
                        | MDL1 | MDL2 | MDL3 | MDL4 |
                        +------+------+------+------+

    The network stack commonly handles subsets of an MDL chain, defined by a
    byte offset into the chain and a byte length:

        |----- Offset ----->|
        |                   |---- Length ---->|
        |                   |                 |

        +----------------+------+-------------------+------------+
        |      MDL1      | MDL2 |      MDL3         |    MDL4    |
        +----------------+------+-------------------+------------+

        |                   |                 |                  |
        |   (ignored data)  |  Actual payload |  (ignored data)  |

    While you can represent a subset of an MDL chain using the tuple of
    <MdlChain, Offset, Length>, it's a bit clumsy to carry around three
    variables everywhere. So this module declares a couple core data structures
    to save you some typing:

    MDL_POINTER
        An MDL pointer refers to any location within an MDL chain's buffers.

    MDL_SPAN
        An MDL span repsents any contiguous subset of the logical concatenation
        of the MDL chain's buffers. That is, an MDL span is a tuple of
        <MdlChain, Offset, Length>.

Inputs:

    While this module offers MDL_POINTER and MDL_SPAN to save you some typing,
    it generally does not force you to use these types. Most routines are
    offered in two or three variants that accept either an
    <MdlChain,Offset,Length> or a MDL_SPAN. The type of inputs accepted can be
    identified from the routine's name:

    MdlChain-
        Accepts a linked list of MDLs and operates on every byte of their
        associated buffers.

    MdlChain-AtOffset
        Accepts a linked list of MDLs and operates on a subset of their
        associated buffers that is identified by offset and length parameters.

    MdlSpan-
        Accepts an MDL_SPAN and operates on the subset of buffers identified by
        the span.

    MdlPointer-
        Accepts an MDL_POINTER and begins operating at that point.

Operations:

    This module provides several high-level operations on MDL chains:

    MdlChainEnsureMappedSystemAddress
        Map each MDL in the chain into system virtual address space.

    MdlChainGetInformation
    MdlChainGetMdlCount
    MdlChainGetByteCount
    MdlChainGetPageCount
        Queries some metadata about the buffers described by the MDL chain.
        MdlChainGetInformation collects all information in one batch, while the
        other routines are convenience wrappers that collect a single, specfic
        bit of information.

    MdlChainAdvanceBytes
        Calculates the position of the byte that is N bytes into the MDL chain.

    MdlPointerAdvanceBytes
        Updates an MDL pointer to point N bytes ahead of its current location.

    MdlPointerNormalize
        Updates an MDL pointer to normal form. See the topic "Normalization".

    MdlChainZeroBuffers
    MdlSpanZeroBuffers
    MdlChainZeroBuffersAtOffset
        Zeros the buffer(s) associated with the MDL chain.
        Conceptually: RtlZeroMemory(Mdl).

    MdlChainFillBuffers
    MdlSpanFillBuffers
    MdlChainFillBuffersAtOffset
        Fills the buffer(s) associated with the MDL chain with a specific byte.
        Conceptually: RtlFillMemory(Mdl, FillPattern).

    MdlCopyFlatBufferToMdlSpan
    MdlCopyFlatBufferToMdlChainAtOffset
        Copies data from a single buffer into some subset of an MDL chain.
        Conceptually: RtlCopyMemory(Mdl, Buffer).

    MdlCopyMdlSpanToFlatBuffer
    MdlCopyMdlChainAtOffsetToFlatBuffer
        Copies data from some subset of an MDL chain into a single buffer.
        Conceptually: RtlCopyMemory(Buffer, Mdl).

    MdlCopyMdlPointerToMdlPointer
    MdlCopyMdlChainToMdlChainAtOffset
        Copies data from some subset of an MDL chain to a subset of another MDL
        chain. Conceptually: RtlCopyMemory(Mdl1, Mdl2).

    MdlEqualBufferContents
    MdlEqualBufferContentsAtOffset
        Determines whether the data in subsets of 2 MDL chains is equal.
        Conceptually: RtlEqualMemory(Mdl1, Mdl2).

Variants:

    This module offers a number of variations on each routine. The variations
    use a consistent naming pattern to help you quickly identify the best
    variant for your needs. For example, MdlChainZeroBuffers is also available
    in the variant MdlChainZeroBuffersNonTemporal -- the variant is denoted
    with the suffix "NonTemporal".

    These are the suffixes used in this module:

    -NonTemporal
        The routine attempts to avoid dragging MDL buffers into the CPU cache,
        to avoid polluting the cache with data that won't be accessed soon
        again. Compare with RtlCopyMemoryNonTemporal.

    -Secure
        The routine suppresses compiler optimizations to ensure that writes to
        memory are definitely not optimized away for any reason. Compare with
        RtlSecureZeroMemory.

    -UpdateInputs-
        This routine accepts an MDL_POINTER to identify where to read from or
        write to, and modifies it in-place to point to the end of the region
        that was read from or written to. (This can be useful if you want to
        access an MDL chain in fixed-length batches. For example, you may want
        to copy the payload of an MDL chain in chunks no larger than 64kb.)

Iteration:

    The various high-level routines offered by this module are based on a few
    low-level iterator routines. You may use these directly if none of the
    high-level routines meet your needs.

    MdlChainIterateBuffers
        This routine invokes a callback for each non-zero-length buffer in an
        entire MDL chain.

    MdlSpanIterateBuffers
        This routine invokes a callback for each non-zero-length buffer in a
        subset of an MDL chain, defined by an MDL span.

    MdlPairwiseIterateBuffers
        This routine invokes a callback for successive pairs of equal-length
        buffers from a pair of MDL chains.

    The last one deserves a diagram. Suppose you have these two MDL chains:

                            +----------+----------+------+
                Chain1:     |   MDL1   |       MDL2      |
                            +----------+----------+------+

                            +--------------+------+------+
                Chain2:     |     MDL1     | MDL2 | MDL3 |
                            +--------------+------+------+

                            |          |   |      |      |
                Callbacks:  |     1    | 2 |   3  |   4  |

     In this example, one chain has 2 MDLs and another chain has 3 MDLs.
     However, MdlPairwiseIterateBuffers invokes the callback 4 times, because
     the MDLs of the two chains don't line up on the same boundaries.

     Another way to look at it: if you wanted to implement a routine to compare
     the contents of two MDL chains, you'd have to run memcmp on several little
     chunks, since the MDL chains' payloads aren't contiguous.
     MdlPairwiseIterateBuffers figures out the little chunks for you, so you
     only have to implement the per-chunk logic. Incidentally, you don't have
     to implement one for memcmp at all, since MdlEqualBufferContents is
     already implemented for you.

Normalization:

    A pointer is in normal form if the pointer points directly into the MDL
    that it's referring to. In practical terms, this means you don't have to
    dereference Mdl->Next to get to the next byte of data at the pointer.

    As a special case, a pointer is also in normal form if its MDL is NULL and
    its offset is zero. This represents a pointer that points at the end of
    some MDL chain, or at the empty MDL chain.

    Formally, a pointer is normal if and only if:
        (Pointer->Mdl == NULL && Pointer->Offset == 0)
            ||
        (Pointer->Offset < MmGetMdlByteCount(Pointer->Mdl))

    The routines in this module accept either normal or denormalized pointers
    as inputs. When any routine in this module makes a callback or updates a
    pointer, it always offers to you a normalized pointer. So you can generally
    assume that any pointer (or span) you get back is normalized.

    If you would like to normalize a pointer yourself, you can call
    MdlPointerNormalize.

Customization:

    You may optionally define any of the following macros to customize the
    behavior of these routines to suit your environment. If you do not define a
    macro, it will default to reasonable behavior for a production-grade
    network kernel driver.

    MDL_MAPPING_OPTIONS
        Control the 2nd parameter to MmGetSystemAddressForMdlSafe. For example,
        you may request a HighPagePriority, or readonly mappings.

    MDL_MAP_BUFFER
    MDL_MAP_CONST_BUFFER
        Select which routine performs MDL mapping. You may inject additional
        logic, for example, statistical counters.

    MDL_REPORT_FATAL_OVERFLOW
        Terminate the system when a programming error has been detected. This
        routine must not return back to the caller.

    MDL_PREFETCH_CACHELINE
        Prefetch data from RAM to improve performance.

Table of Contents:

    MdlChainIterateBuffers
    MdlSpanIterateBuffers
    MdlChainEnsureMappedSystemAddress
    MdlChainGetInformation
    MdlChainGetMdlCount
    MdlChainGetByteCount
    MdlChainGetPageCount
    MdlChainAdvanceBytes
    MdlPointerNormalize
    MdlPointerAdvanceBytes
    MdlPairwiseIterateBuffersUpdateInputs
    MdlPairwiseIterateBuffers
    MdlChainZeroBuffers
    MdlSpanZeroBuffers
    MdlChainZeroBuffersAtOffset
    MdlChainZeroBuffersNonTemporal
    MdlSpanZeroBuffersNonTemporal
    MdlChainZeroBuffersAtOffsetNonTemporal
    MdlChainZeroBuffersSecure
    MdlSpanZeroBuffersSecure
    MdlChainZeroBuffersAtOffsetSecure
    MdlChainFillBuffers
    MdlSpanFillBuffers
    MdlChainFillBuffersAtOffset
    MdlChainFillBuffersNonTemporal
    MdlSpanFillBuffersNonTemporal
    MdlChainFillBuffersAtOffsetNonTemporal
    MdlCopyFlatBufferToMdlSpan
    MdlCopyFlatBufferToMdlChainAtOffset
    MdlCopyMdlSpanToFlatBuffer
    MdlCopyMdlChainAtOffsetToFlatBuffer
    MdlCopyMdlPointerToMdlPointer
    MdlCopyMdlPointerToMdlPointerUpdateInputs
    MdlCopyMdlChainToMdlChainAtOffset
    MdlCopyFlatBufferToMdlSpanNonTemporal
    MdlCopyFlatBufferToMdlChainAtOffsetNonTemporal
    MdlCopyMdlSpanToFlatBufferNonTemporal
    MdlCopyMdlChainAtOffsetToFlatBufferNonTemporal
    MdlCopyMdlPointerToMdlPointerNonTemporal
    MdlCopyMdlPointerToMdlPointerUpdateInputsNonTemporal
    MdlCopyMdlChainToMdlChainAtOffsetNonTemporal
    MdlEqualBufferContents
    MdlEqualBufferContentsUpdateInputs
    MdlEqualBufferContentsAtOffset

Environment:

    Kernel mode

    This header does not depend on NDIS.H or any other network-specific stuff,
    so you can freely use it in your storage or HID driver. However, MDL
    chains are primarily of interest to network drivers, so you probably don't
    need this header in a storage driver.

--*/

#pragma once

#pragma warning(push)
#pragma warning(disable : 4514) // Unreferenced inline function has been removed

// You may replace MDL_MAPPING_OPTIONS if you want to customize the page
// priority used by MmGetSystemAddressForMdlSafe.
#ifndef MDL_MAPPING_OPTIONS
#  define MDL_MAPPING_OPTIONS (LowPagePriority)
#endif

// You may replace MDL_MAP_BUFFER and MDL_MAP_CONST_BUFFER if you have special
// needs for how MDLs are mapped into system address space.
#ifndef MDL_MAP_BUFFER
#  define MDL_MAP_BUFFER(Mdl) \
       (UCHAR*)MmGetSystemAddressForMdlSafe(Mdl, MDL_MAPPING_OPTIONS)
#endif
#ifndef MDL_MAP_CONST_BUFFER
#  define MDL_MAP_CONST_BUFFER(Mdl) \
       (UCHAR const*)MmGetSystemAddressForMdlSafe(Mdl, MDL_MAPPING_OPTIONS)
#endif

// You may replace MDL_REPORT_FATAL_OVERFLOW if you need to terminate the
// system in a different manner than a simple __fastfail instruction. Note that
// you MUST NOT allow execution to continue once a fatal error has been
// reported; the routines may be in an inconsistent state and are not safe to
// continue executing.
#ifndef MDL_REPORT_FATAL_OVERFLOW
#  define MDL_REPORT_FATAL_OVERFLOW(Mdl, Offset) \
       RtlFailFast(FAST_FAIL_INVALID_BUFFER_ACCESS)
#endif

// You may remove (or modify) all prefetches if you have special needs.
#ifndef MDL_PREFETCH_CACHELINE
#  define MDL_PREFETCH_CACHELINE(Address) \
       PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Address)
#endif

// This signals that the callback is done iterating over an MDL chain's buffers
#define STATUS_STOP_ITERATION ((NTSTATUS)1L)

// An MDL_POINTER represents a pointer to some byte in the payload an MDL
// chain.
typedef struct MDL_POINTER_t
{
    MDL* Mdl;
    SIZE_T Offset;
} MDL_POINTER;

// An MDL_SPAN represents a range of 0 or more bytes in the payload of an MDL
// chain. A span is conceptually contiguous, but may straddle multiple virtual
// buffers.
typedef struct MDL_SPAN_t
{
    MDL_POINTER Start;
    SIZE_T Length;
} MDL_SPAN;

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(MDL_BUFFER_OPERATOR)
NTSTATUS
MDL_BUFFER_OPERATOR(
    _In_ PVOID OperatorContext,
    _In_ MDL_SPAN const* Span);
/*++

Routine Description:

    A callback that is invoked for each non-empty buffer in some subset of an
    MDL chain

Arguments:

    OperatorContext
        An value that you provide to the iteration routine to hold your state

    Span
        An MDL, offset into the MDL's buffer, and length of the buffer

        The offset and length are guaranteed to be contained in the MDL's
        payload, i.e., you will never need to perform any overflow checks
        or dereference Mdl->Next.

        The MDL is *not* guranteed to be mapped into system address space.
        The iterators do not map any MDL; if you need the MDL to be mapped,
        you must either ensure the MDL is mapped prior to invoking the
        iterator, or map the MDL yourself in this callback.

Return Value:

    STATUS_SUCCESS
        Iteration will continue to the next buffer

    Any other NTSTATUS
        Iteration will immediately stop, and the iterator will return this
        status code back to the caller

--*/

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(MDL_BUFFER_PAIRWISE_OPERATOR)
NTSTATUS
MDL_BUFFER_PAIRWISE_OPERATOR(
    _In_ PVOID OperatorContext,
    _In_ MDL_POINTER const* MdlPointer1,
    _In_ MDL_POINTER const* MdlPointer2,
    _In_ SIZE_T BufferLength);
/*++

Routine Description:

    A callback that is invoked for each pair of non-empty buffers in
    equally-length subsets of two MDL chains

Arguments:

    OperatorContext
        An value that you provide to the iteration routine to hold your state

    MdlPointer1
        An MDL from the first MDL chain and offset into the MDL's buffer

    MdlPointer2
        An MDL from the second MDL chain and offset into the MDL's buffer

        Both offsets are guaranteed to be contained in each MDLs' payloads,
        i.e., you will never need to perform any overflow checks or dereference
        Mdl->Next.

        The MDLs are *not* guranteed to be mapped into system address space.
        The iterators do not map any MDL; if you need any MDL to be mapped,
        you must either ensure the MDL is mapped prior to invoking the
        iterator, or map the MDL yourself in this callback.

    BufferLength
        The length of the two buffers starting at MdlPointer1 and MdlPointer2

Return Value:

    STATUS_SUCCESS
        Iteration will continue to the next buffer

    Any other NTSTATUS
        Iteration will immediately stop, and the iterator will return this
        status code back to the caller

--*/

#define ReportFatalOverflow _MdlPrivate_ReportFatalOverflow

_IRQL_requires_max_(DISPATCH_LEVEL)
DECLSPEC_NORETURN
inline
void ReportFatalOverflow(
    _In_ MDL const* MdlChain,
    _In_ SIZE_T Offset)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(MdlChain);
    UNREFERENCED_PARAMETER(Offset);

    MDL_REPORT_FATAL_OVERFLOW(MdlChain, Offset);
}

#define MinSizeT _MdlPrivate_MinSizeT

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
SIZE_T MinSizeT(
    _In_ SIZE_T a,
    _In_ SIZE_T b)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    return (a < b) ? a : b;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainIterateBuffers(
    _In_opt_ MDL* MdlChain,
    _In_ MDL_BUFFER_OPERATOR* Operator,
    _In_opt_ PVOID OperatorContext)
/*++

Routine Description:

    Iterates over every buffer in an MDL chain and invokes a callback on each
    non-empty buffer

Arguments:

    MdlChain
        The MDL chain to iterate over

    Operator
        Your callback routine

    OperatorContext
        An opaque value that is passed to your callback routine

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    Any other NTSTATUS
        The iterator callback returned this status

--*/
{
    for (MDL* Mdl = MdlChain; Mdl; Mdl = Mdl->Next)
    {
        MDL_PREFETCH_CACHELINE(Mdl->Next);

        if (0 < MmGetMdlByteCount(Mdl))
        {
            MDL_SPAN Span = { { 0 } };
            Span.Start.Mdl = Mdl;
            Span.Start.Offset = 0;
            Span.Length = MmGetMdlByteCount(Mdl);

#pragma warning(suppress:6387) // 'OperatorContext' could be NULL
            NTSTATUS NtStatus = Operator(OperatorContext, &Span);
            if (STATUS_SUCCESS != NtStatus)
            {
                return NtStatus;
            }
        }
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanIterateBuffers(
    _In_ MDL_SPAN const* Span,
    _In_ MDL_BUFFER_OPERATOR* Operator,
    _In_opt_ PVOID OperatorContext)
/*++

Routine Description:

    Iterates over some subset of the buffers in an MDL chain and invokes a
    callback on each non-empty buffer

Arguments:

    Span
        The MDL span to iterate over

    Operator
        Your callback routine

    OperatorContext
        An opaque value that is passed to your callback routine

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    Any other NTSTATUS
        The iterator callback returned this status

--*/
{
    SIZE_T BytesRemaining = Span->Length;

    if (0 == BytesRemaining)
    {
        return STATUS_SUCCESS;
    }

    SIZE_T OffsetRemaining = Span->Start.Offset;

    for (MDL* Mdl = Span->Start.Mdl; Mdl; Mdl = Mdl->Next)
    {
        MDL_PREFETCH_CACHELINE(Mdl->Next);

        SIZE_T MdlOffset = MinSizeT(OffsetRemaining, MmGetMdlByteCount(Mdl));
        OffsetRemaining -= MdlOffset;

        if (MmGetMdlByteCount(Mdl) > MdlOffset)
        {
            SIZE_T BufferLength =
                MinSizeT(MmGetMdlByteCount(Mdl) - MdlOffset, BytesRemaining);

            MDL_SPAN Subspan = { { 0 } };
            Subspan.Start.Mdl = Mdl;
            Subspan.Start.Offset = MdlOffset;
            Subspan.Length = BufferLength;

#pragma warning(suppress:6387) // 'OperatorContext' could be NULL
            NTSTATUS NtStatus = Operator(OperatorContext, &Subspan);
            if (STATUS_SUCCESS != NtStatus)
            {
                return NtStatus;
            }

            BytesRemaining -= BufferLength;
            if (0 == BytesRemaining)
            {
                return STATUS_SUCCESS;
            }
        }
    }

    if (0 < BytesRemaining)
    {
        ReportFatalOverflow(Span->Start.Mdl, Span->Start.Offset + Span->Length);
    }

    return STATUS_SUCCESS;
}

#define MapOperator _MdlPrivate_MapOperator

MDL_BUFFER_OPERATOR MapOperator;

_Use_decl_annotations_
inline
NTSTATUS MapOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    if (0 < Span->Length)
    {
        UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
        if (!Buffer)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainEnsureMappedSystemAddress(
    _In_ MDL* MdlChain)
/*++

Routine Description:

    Ensures every MDL in an MDL chain is mapped into system address space

Arguments:

    MdlChain
        The MDL chain to map into system address space

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(MdlChain, MapOperator, NULL);
}

typedef struct MDL_CHAIN_INFORMATION_t
{
    // The total number of MDLs in this MDL chain
    SIZE_T NumberOfMdls;

    // The number of MDLs in this MDL chain where `0 < MmGetMdlByteCount(Mdl)`
    SIZE_T NumberOfNonEmptyMdls;

    // The number of MDLs in this MDL chain that are already mapped into system
    // address space
    SIZE_T NumberOfMdlsMappedToSystemVA;

    // The total number of bytes represented by this MDL chain, i.e. the sum of
    // `MmGetMdlByteCount(Mdl)` for each MDL in the chain
    SIZE_T TotalByteCount;

    // The total number of pages touched by this MDL chain This does not
    // attempt to de-dup any page that may appear twice in the MDL chain.
    SIZE_T TotalPageCount;

    // The most generous buffer alignment that matches every buffer in this MDL
    // chain. For example, if this value is 8, then every buffer in the MDL
    // chain is aligned to an 8-byte boundary, at at least one buffer is not
    // 16-byte aligned.
    //
    // Formally, MaximumAlignment is the largest power of 2 less than or equal
    // to PAGE_SIZE such that for each MDL in the MDL chain:
    //
    //     0 == (MmGetByteOffset(Mdl) % MaximumAlignment)
    //
    // If the MDL chain contains no payload, then this value is PAGE_SIZE.
    //
    // If every buffer has page-alignment, i.e. `0 == MmGetMdlByteOffset(Mdl)`,
    // then this value is PAGE_SIZE.
    //
    // In the worst case, at least one buffer is only byte-aligned at best
    // (e.g. 0x401 == MmGetMdlByteOffset(Mdl)) and then MaximumAlignment will
    // be 1.
    SIZE_T MaximumAlignment;
} MDL_CHAIN_INFORMATION;

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
MdlChainGetInformation(
    _In_opt_ MDL* MdlChain,
    _Out_ MDL_CHAIN_INFORMATION *Information)
/*++

Routine Description:

    Obtains some metadata about an MDL chain

Arguments:

    MdlChain
        The MDL chain to characterize

    Information
        Receives some metadata about the MDL chain

--*/
{
    Information->NumberOfMdls = 0;
    Information->NumberOfNonEmptyMdls = 0;
    Information->NumberOfMdlsMappedToSystemVA = 0;
    Information->TotalByteCount = 0;
    Information->TotalPageCount = 0;
    Information->MaximumAlignment = 0;

    for (MDL* Mdl = MdlChain; Mdl; Mdl = Mdl->Next)
    {
        MDL_PREFETCH_CACHELINE(Mdl->Next);

        Information->NumberOfMdls += 1;
        Information->TotalByteCount += MmGetMdlByteCount(Mdl);
        Information->TotalPageCount += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                (void*)MmGetMdlByteOffset(Mdl), MmGetMdlByteCount(Mdl));

        if (0 < MmGetMdlByteCount(Mdl))
        {
            Information->NumberOfNonEmptyMdls += 1;
            Information->MaximumAlignment |= MmGetMdlByteOffset(Mdl);
        }

        if (0 != (Mdl->MdlFlags & (MDL_MAPPED_TO_SYSTEM_VA | MDL_SOURCE_IS_NONPAGED_POOL)))
        {
            Information->NumberOfMdlsMappedToSystemVA += 1;
        }

        Information->MaximumAlignment &=
            ~(Information->MaximumAlignment - 1) & (PAGE_SIZE - 1);
    }

    if (0 == Information->MaximumAlignment)
    {
        Information->MaximumAlignment = PAGE_SIZE;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
SIZE_T
MdlChainGetMdlCount(
    _In_opt_ MDL* MdlChain)
/*++

Routine Description:

    Returns the number of MDLs in an MDL chain

Arguments:

    MdlChain
        The MDL chain to characterize

Return Value:

    The total number of MDLs in this MDL chain, including MDLs with zero-length
    buffer

--*/
{
    MDL_CHAIN_INFORMATION Information = { 0 };
    MdlChainGetInformation(MdlChain, &Information);
    return Information.NumberOfMdls;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
SIZE_T
MdlChainGetByteCount(
    _In_opt_ MDL* MdlChain)
/*++

Routine Description:

    Returns the number of bytes of payload associated with an MDL chain

Arguments:

    MdlChain
        The MDL chain to characterize

Return Value:

    The total number of bytes of payload associated with this MDL chain

--*/
{
    MDL_CHAIN_INFORMATION Information = { 0 };
    MdlChainGetInformation(MdlChain, &Information);
    return Information.TotalByteCount;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
SIZE_T
MdlChainGetPageCount(
    _In_opt_ MDL* MdlChain)
/*++

Routine Description:

    Returns the number of physical pages that are spanned by the MDLs in this
    MDL chain

    N.B. This routine makes no attempt to de-dup pages. If a pages is mapped
         into the MDL chain twice, then it will be double-counted.

    N.B. A page is counted if even a single byte of payload is placed on it.
         Therefore, you cannot assume an exact ratio between pages and bytes.
         The only relationships that hold are:

         MdlChainGetPageCount(Mdl) <= MdlChainGetByteCount(Mdl)
         MdlChainGetPageCount(Mdl) >= (MdlChainGetByteCount(Mdl) + PAGE_SIZE-1) / PAGE_SIZE

Arguments:

    MdlChain
        The MDL chain to characterize

Return Value:

    The number of physical pages that are spanned by the MDLs in this MDL chain

--*/
{
    MDL_CHAIN_INFORMATION Information = { 0 };
    MdlChainGetInformation(MdlChain, &Information);
    return Information.TotalPageCount;
}

#define SEEK_OPERATOR_CONTEXT_t _MdlPrivate_SEEK_OPERATOR_CONTEXT_t
#define SEEK_OPERATOR_CONTEXT _MdlPrivate_SEEK_OPERATOR_CONTEXT

typedef struct SEEK_OPERATOR_CONTEXT_t
{
    SIZE_T SeekBytesRemaining;
    MDL* FoundMdl;
} SEEK_OPERATOR_CONTEXT;

#define SeekOperator _MdlPrivate_SeekOperator

MDL_BUFFER_OPERATOR SeekOperator;

_Use_decl_annotations_
inline
NTSTATUS SeekOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    SEEK_OPERATOR_CONTEXT* Context = (SEEK_OPERATOR_CONTEXT*)OperatorContext;

    if (Context->SeekBytesRemaining >= Span->Length)
    {
        Context->SeekBytesRemaining -= Span->Length;
        return STATUS_SUCCESS;
    }
    else
    {
        Context->FoundMdl = Span->Start.Mdl;
        return STATUS_STOP_ITERATION;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
MdlChainAdvanceBytes(
    _In_ MDL* MdlChain,
    _In_ SIZE_T AdvanceOffset,
    _Out_ MDL** FoundMdl,
    _Out_ SIZE_T* FoundMdlOffset)
/*++

Routine Description:

    Given an offset into an MDL chain, calculates which individual MDL is
    associated with the buffer at that offsets, along with the remaining offset
    into that specific MDL's buffer

    For example, consider the MDL chain below:

      MdlChain
              \
               |----------- AdvanceOffset ---------->|
               |                                     |

               +--------+------+----------------------------+--------+
               |  MDL1  | MDL2 |            MDL3            |  MDL4  |
               +--------+------+----------------------------+--------+

                               |                     |
                               |-- FoundMdlOffset -->|
                              /
                      FoundMdl

    In this example, by advancing `AdvanceOffset` bytes into the MDL chain
    starting at MDL1, we wind up pointing some distance into MDL3. MDL3 is
    returned as `FoundMdl` and that distance is returned as `FoundMdlOffset`.

    As a special case, if `AdvanceOffset` is exactly equal to the length of the
    entire MDL chain, then `FoundMdl` receives NULL and `FoundMdlOffset`
    receives 0.

    If `AdvanceOffset` is greater than the length of the entire MDL chain, this
    routine crashes the system with a fatal overflow error.

Arguments:

    MdlChain
        The MDL at which to start iteration

    AdvanceOffset
        The number of bytes to advance

    FoundMdl
        Receives a pointer to the MDL that is associated with the buffer that
        contains the byte at AdvanceOffset from the start of the MDL chain

    FoundMdlOffset
        Receives the remaining offset into FoundMdl's buffer to the byte
        given by AdvanceOffset

--*/
{
    SEEK_OPERATOR_CONTEXT Context = { 0 };
    Context.FoundMdl = NULL;
    Context.SeekBytesRemaining = AdvanceOffset;

    NTSTATUS NtStatus = MdlChainIterateBuffers(MdlChain, SeekOperator, &Context);

    if (STATUS_STOP_ITERATION == NtStatus)
    {
        *FoundMdl = Context.FoundMdl;
        *FoundMdlOffset = Context.SeekBytesRemaining;
    }
    else if (STATUS_SUCCESS == NtStatus && 0 == Context.SeekBytesRemaining)
    {
        *FoundMdl = NULL;
        *FoundMdlOffset = 0;
    }
    else
    {
        ReportFatalOverflow(MdlChain, AdvanceOffset);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
MdlPointerNormalize(
    _Inout_ MDL_POINTER* Pointer)
/*++

Routine Description:

    Normalizes the given MDL pointer, i.e., ensures that the pointer's Offset
    points into the current MDL

    For more information, refer to the topic "Normalization" at the top of this
    header file.

    As a special case, if the pointer's Offset is exactly equal to the length
    of the entire MDL chain, then the Mdl is set to NULL and Offset is set to
    0.

    If the pointer's Offset is greater than the length of the entire MDL chain,
    this routine crashes the system with a fatal overflow error.

Arguments:

    Pointer
        The pointer to update into normal form

--*/
{
    if (Pointer->Offset >= MmGetMdlByteCount(Pointer->Mdl))
    {
        MdlChainAdvanceBytes(
            Pointer->Mdl,
            Pointer->Offset,
            &Pointer->Mdl,
            &Pointer->Offset);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
MdlPointerAdvanceBytes(
    _Inout_ MDL_POINTER* Pointer,
    _In_ SIZE_T Delta)
/*++

Routine Description:

    Moves the given MDL pointer forward by the specified number of bytes

    As a special case, if the pointer's existing offset plus Delta is exactly
    equal to the length of the entire MDL chain, then the pointer's Mdl is set
    to NULL and Offset is set to 0.

    If the pointer's existing offset plus Delta is greater than the length of
    the entire MDL chain, this routine crashes the system with a fatal overflow
    error.

Arguments:

    Pointer
        The pointer to update

    Delta
        The number of bytes to move forward by

--*/
{
    if (Delta + Pointer->Offset < MmGetMdlByteCount(Pointer->Mdl))
    {
        Pointer->Offset += Delta;
    }
    else
    {
        MdlChainAdvanceBytes(
            Pointer->Mdl,
            Delta + Pointer->Offset,
            &Pointer->Mdl,
            &Pointer->Offset);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlPairwiseIterateBuffersUpdateInputs(
    _Inout_ MDL_POINTER* MdlPointer1,
    _Inout_ MDL_POINTER* MdlPointer2,
    _In_ SIZE_T TotalLength,
    _In_ MDL_BUFFER_PAIRWISE_OPERATOR* Operator,
    _In_opt_ PVOID OperatorContext)
/*++

Routine Description:

    Iterates over each pair of equal-length buffers in two MDL chains

    This routine updates MdlPointer1 and MdlPointer2 in-place to point to the
    end of the regions that were succesfully processed.

    For more detail and a diagram, refer to the topic "Iteration" at the top of
    this header file.

    If TotalLength extends past the end of either MDL chain, this routine
    crashes the system with a fatal overflow error.

Arguments:

    MdlPointer1
        The starting point of the first MDL chain to process

    MdlPointer2
        The starting point of the second MDL chain to process

    TotalLength
        The number of bytes to process in each MDL chain

    Operator
        Your callback routine

    OperatorContext
        An opaque value that is passed to your callback routine

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    Any other NTSTATUS
        The iterator callback returned this status

--*/
{
    NTSTATUS NtStatus = STATUS_SUCCESS;

    SIZE_T BytesRemaining = TotalLength;

    MdlPointerNormalize(MdlPointer1);
    MdlPointerNormalize(MdlPointer2);

    while (BytesRemaining > 0)
    {
        SIZE_T CommonLength =
            MinSizeT(BytesRemaining,
            MinSizeT(MmGetMdlByteCount(MdlPointer1->Mdl) - MdlPointer1->Offset,
                     MmGetMdlByteCount(MdlPointer2->Mdl) - MdlPointer2->Offset));

#pragma warning(suppress:6387) // 'OperatorContext' could be NULL
        NtStatus = Operator(OperatorContext, MdlPointer1, MdlPointer2, CommonLength);
        if (STATUS_SUCCESS != NtStatus)
        {
            return NtStatus;
        }

        MdlPointerAdvanceBytes(MdlPointer1, CommonLength);
        MdlPointerAdvanceBytes(MdlPointer2, CommonLength);

        BytesRemaining -= CommonLength;
    }

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlPairwiseIterateBuffers(
    _In_ MDL_POINTER const* MdlPointer1,
    _In_ MDL_POINTER const* MdlPointer2,
    _In_ SIZE_T TotalLength,
    _In_ MDL_BUFFER_PAIRWISE_OPERATOR* Operator,
    _In_opt_ PVOID OperatorContext)
/*++

Routine Description:

    Iterates over each pair of equal-length buffers in two MDL chains

    For more detail and a diagram, refer to the topic "Iteration" at the top of
    this header file.

    If TotalLength extends past the end of either MDL chain, this routine
    crashes the system with a fatal overflow error.

Arguments:

    MdlPointer1
        The starting point of the first MDL chain to process

    MdlPointer2
        The starting point of the second MDL chain to process

    TotalLength
        The number of bytes to process in each MDL chain

    Operator
        Your callback routine

    OperatorContext
        An opaque value that is passed to your callback routine

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    Any other NTSTATUS
        The iterator callback returned this status

--*/
{
    if (0 == TotalLength)
    {
        return STATUS_SUCCESS;
    }

    MDL_POINTER LocalPointer1 = *MdlPointer1;
    MDL_POINTER LocalPointer2 = *MdlPointer2;

    return MdlPairwiseIterateBuffersUpdateInputs(
            &LocalPointer1,
            &LocalPointer2,
            TotalLength,
            Operator,
            OperatorContext);
}

#define ZeroOperator _MdlPrivate_ZeroOperator

MDL_BUFFER_OPERATOR ZeroOperator;

_Use_decl_annotations_
inline
NTSTATUS ZeroOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Buffer + Span->Start.Offset, Span->Length);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffers(
    _In_ MDL* MdlChain)
/*++

Routine Description:

    Zeros every byte of every buffer associated with the provided MDL chain

Arguments:

    MdlChain
        The MDL chain to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(MdlChain, ZeroOperator, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanZeroBuffers(
    _In_ MDL_SPAN const* Span)
/*++

Routine Description:

    Zeros the buffers contained in the MDL span

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

Arguments:

    Span
        The MDL span to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(Span, ZeroOperator, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffersAtOffset(
    _In_ MDL* MdlChain,
    _In_ SIZE_T Offset,
    _In_ SIZE_T ZeroLength)
/*++

Routine Description:

    Zeros the buffers at some subset of an MDL chain

    If Offset plus ZeroLength extends past the end of the MDL chain, this
    routine crashes the system with a fatal overflow error.

Arguments:

    MdlChain
        The MDL chain to process

    Offset
        The offset into the MDL chain's buffers at which to begin zeroing

    ZeroLength
        The total number of bytes to zero

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Span = { { 0 } };
    Span.Start.Mdl = MdlChain;
    Span.Start.Offset = Offset;
    Span.Length = ZeroLength;

    return MdlSpanZeroBuffers(&Span);
}

#define ZeroOperatorNonTemporal _MdlPrivate_ZeroOperatorNonTemporal

MDL_BUFFER_OPERATOR ZeroOperatorNonTemporal;

_Use_decl_annotations_
inline
NTSTATUS ZeroOperatorNonTemporal(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlFillMemoryNonTemporal(Buffer + Span->Start.Offset, Span->Length, 0);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffersNonTemporal(
    _In_ MDL* MdlChain)
/*++

Routine Description:

    Zeros every byte of every buffer associated with the provided MDL chain

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    MdlChain
        The MDL chain to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(MdlChain, ZeroOperatorNonTemporal, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanZeroBuffersNonTemporal(
    _In_ MDL_SPAN const* Span)
/*++

Routine Description:

    Zeros the buffers contained in the MDL span

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    Span
        The MDL span to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(Span, ZeroOperatorNonTemporal, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffersAtOffsetNonTemporal(
    _In_ MDL* MdlChain,
    _In_ SIZE_T Offset,
    _In_ SIZE_T ZeroLength)
/*++

Routine Description:

    Zeros the buffers at some subset of an MDL chain

    If Offset plus ZeroLength extends past the end of the MDL chain, this
    routine crashes the system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    MdlChain
        The MDL chain to process

    Offset
        The offset into the MDL chain's buffers at which to begin zeroing

    ZeroLength
        The total number of bytes to zero

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Span = { { 0 } };
    Span.Start.Mdl = MdlChain;
    Span.Start.Offset = Offset;
    Span.Length = ZeroLength;

    return MdlSpanZeroBuffersNonTemporal(&Span);
}

#define ZeroOperatorSecure _MdlPrivate_ZeroOperatorSecure

MDL_BUFFER_OPERATOR ZeroOperatorSecure;

_Use_decl_annotations_
inline
NTSTATUS ZeroOperatorSecure(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlSecureZeroMemory(Buffer + Span->Start.Offset, Span->Length);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffersSecure(
    _In_ MDL* MdlChain)
/*++

Routine Description:

    Zeros every byte of every buffer associated with the provided MDL chain

    This routine disables compiler and processor optimizations to guarantee the
    contents of RAM are updated.

Arguments:

    MdlChain
        The MDL chain to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(MdlChain, ZeroOperatorSecure, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanZeroBuffersSecure(
    _In_ MDL_SPAN const* Span)
/*++

Routine Description:

    Zeros the buffers contained in the MDL span

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

    This routine disables compiler and processor optimizations to guarantee the
    contents of RAM are updated.

Arguments:

    Span
        The MDL span to process

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(Span, ZeroOperatorSecure, NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainZeroBuffersAtOffsetSecure(
    _In_ MDL* MdlChain,
    _In_ SIZE_T Offset,
    _In_ SIZE_T ZeroLength)
/*++

Routine Description:

    Zeros the buffers at some subset of an MDL chain

    If Offset plus ZeroLength extends past the end of the MDL chain, this
    routine crashes the system with a fatal overflow error.

    This routine disables compiler and processor optimizations to guarantee the
    contents of RAM are updated.

Arguments:

    MdlChain
        The MDL chain to process

    Offset
        The offset into the MDL chain's buffers at which to begin zeroing

    ZeroLength
        The total number of bytes to zero

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Span = { { 0 } };
    Span.Start.Mdl = MdlChain;
    Span.Start.Offset = Offset;
    Span.Length = ZeroLength;

    return MdlSpanZeroBuffersSecure(&Span);
}

#define FillOperator _MdlPrivate_FillOperator

MDL_BUFFER_OPERATOR FillOperator;

_Use_decl_annotations_
inline
NTSTATUS FillOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlFillMemory(
        Buffer + Span->Start.Offset,
        Span->Length,
        (UCHAR)PtrToUlong(OperatorContext));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainFillBuffers(
    _In_ MDL* MdlChain,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills every byte of the buffers associated with an MDL chain with a byte value

Arguments:

    MdlChain
        The MDL chain to process

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(
        MdlChain,
        FillOperator,
        ULongToPtr(FillByte));
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanFillBuffers(
    _In_ MDL_SPAN const* Span,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills the buffers contained in the MDL span with a byte value

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

Arguments:

    Span
        The MDL span to process

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        Span,
        FillOperator,
        ULongToPtr(FillByte));
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainFillBuffersAtOffset(
    _In_ MDL* MdlChain,
    _In_ SIZE_T Offset,
    _In_ SIZE_T FillLength,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills the buffers at some subset of an MDL chain with a byte value

    If Offset plus FillLength extends past the end of the MDL chain, this
    routine crashes the system with a fatal overflow error.

Arguments:

    MdlChain
        The chain of MDLs to process

    Offset
        The offset into the MDL chain at which to begin writing the fill pattern

    FillLength
        The number of bytes to write

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Span = { { 0 } };
    Span.Start.Mdl = MdlChain;
    Span.Start.Offset = Offset;
    Span.Length = FillLength;

    return MdlSpanFillBuffers(&Span, FillByte);
}

#define FillOperatorNonTemporal _MdlPrivate_FillOperatorNonTemporal

MDL_BUFFER_OPERATOR FillOperatorNonTemporal;

_Use_decl_annotations_
inline
NTSTATUS FillOperatorNonTemporal(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlFillMemoryNonTemporal(
        Buffer + Span->Start.Offset,
        Span->Length,
        (UCHAR)PtrToUlong(OperatorContext));

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainFillBuffersNonTemporal(
    _In_ MDL* MdlChain,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills every byte of the buffers associated with an MDL chain with a byte value

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    MdlChain
        The MDL chain to process

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlChainIterateBuffers(
        MdlChain,
        FillOperatorNonTemporal,
        ULongToPtr(FillByte));
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlSpanFillBuffersNonTemporal(
    _In_ MDL_SPAN const* Span,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills the buffers contained in the MDL span with a byte value

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    Span
        The MDL span to process

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        Span,
        FillOperatorNonTemporal,
        ULongToPtr(FillByte));
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlChainFillBuffersAtOffsetNonTemporal(
    _In_ MDL* MdlChain,
    _In_ SIZE_T Offset,
    _In_ SIZE_T FillLength,
    _In_ UCHAR FillByte)
/*++

Routine Description:

    Fills the buffers at some subset of an MDL chain with a byte value

    If Offset plus FillLength extends past the end of the MDL chain, this
    routine crashes the system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    MdlChain
        The chain of MDLs to process

    Offset
        The offset into the MDL chain at which to begin writing the fill pattern

    FillLength
        The number of bytes to write

    FillByte
        The byte to fill the buffers with

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Span = { { 0 } };
    Span.Start.Mdl = MdlChain;
    Span.Start.Offset = Offset;
    Span.Length = FillLength;

    return MdlSpanFillBuffersNonTemporal(&Span, FillByte);
}

#define WriteOperator _MdlPrivate_WriteOperator

MDL_BUFFER_OPERATOR WriteOperator;

_Use_decl_annotations_
inline
NTSTATUS WriteOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR const** Source = (UCHAR const**)OperatorContext;

    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(Buffer + Span->Start.Offset, *Source, Span->Length);
    *Source += Span->Length;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyFlatBufferToMdlSpan(
    _In_ MDL_SPAN const* DestinationSpan,
    _In_reads_(DestinationSpan->Length) UCHAR const* SourceBuffer)
/*++

Routine Description:

    Copies data from a single buffer into an MDL chain

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

Arguments:

    DestinationSpan
        The span of bytes to write into

    SourceBuffer
        The buffer to read from

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        DestinationSpan,
        WriteOperator,
        &SourceBuffer);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyFlatBufferToMdlChainAtOffset(
    _In_ MDL* DestinationMdlChain,
    _In_ SIZE_T DestinationOffset,
    _In_reads_(CopyLength) UCHAR const* SourceBuffer,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from a single buffer into an MDL chain

    If the DestinationOffset plus CopyLength is greater than the total length
    of the MDL chain, this routine crashes the system wtih a fatal overflow
    error.

Arguments:

    DestinationMdlChain
        The MDL chain to write into

    DestinationOffset
        The byte offset at which to begin writing into

    SourceBuffer
        The buffer to read from

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Destination = { { 0 } };
    Destination.Start.Mdl = DestinationMdlChain;
    Destination.Start.Offset = DestinationOffset;
    Destination.Length = CopyLength;

    return MdlCopyFlatBufferToMdlSpan(
        &Destination,
        SourceBuffer);
}

#define ReadOperator _MdlPrivate_ReadOperator
MDL_BUFFER_OPERATOR ReadOperator;

_Use_decl_annotations_
inline
NTSTATUS ReadOperator(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR** Destination = (UCHAR**)OperatorContext;

    UCHAR const* Buffer = MDL_MAP_CONST_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(
        *Destination,
        Buffer + Span->Start.Offset,
        Span->Length);

    *Destination += Span->Length;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlSpanToFlatBuffer(
    _Out_writes_(Source->Length) UCHAR* DestinationBuffer,
    _In_ MDL_SPAN const* Source)
/*++

Routine Description:

    Copies data from an MDL span into a buffer

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

Arguments:

    DestinationBuffer
        The buffer to write into

    Source
        The MDL span to read from

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        Source,
        ReadOperator,
        &DestinationBuffer);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlChainAtOffsetToFlatBuffer(
    _Out_writes_(CopyLength) UCHAR* DestinationBuffer,
    _In_ MDL* SourceMdlChain,
    _In_ SIZE_T SourceOffset,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from an MDL chain into a buffer

    If SourceOffset plus CopyLength is greater than the total length of the MDL
    chain, this routine crashes the system with a fatal overflow error.

Arguments:

    DestinationBuffer
        The buffer to write into

    SourceMdlChain
        The MDL chain to read from

    SourceOffset
        The offset into the MDL chain at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Source = { { 0 } };
    Source.Start.Mdl = SourceMdlChain;
    Source.Start.Offset = SourceOffset;
    Source.Length = CopyLength;

    return MdlCopyMdlSpanToFlatBuffer(
        DestinationBuffer,
        &Source);
}

#define PairwiseCopy _MdlPrivate_PairwiseCopy

MDL_BUFFER_PAIRWISE_OPERATOR PairwiseCopy;

_Use_decl_annotations_
inline
NTSTATUS PairwiseCopy(
    PVOID OperatorContext,
    MDL_POINTER const* MdlPointer1,
    MDL_POINTER const* MdlPointer2,
    SIZE_T BufferLength)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR* DestinationBuffer = MDL_MAP_BUFFER(MdlPointer1->Mdl);
    UCHAR const* SourceBuffer = MDL_MAP_CONST_BUFFER(MdlPointer2->Mdl);

    if (!SourceBuffer || !DestinationBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(
        DestinationBuffer + MdlPointer1->Offset,
        SourceBuffer + MdlPointer2->Offset,
        BufferLength);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlPointerToMdlPointer(
    _In_ MDL_POINTER const* Destination,
    _In_ MDL_POINTER const* Source,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either pointer's Offset is greater than the length
    of that pointer's MDL chain, this routine crashes the system with a fatal
    overflow error.

Arguments:

    Destination
        A pointer at which to begin writing

    Source
        A pointer at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlPairwiseIterateBuffers(
        Destination,
        Source,
        CopyLength,
        PairwiseCopy,
        NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlPointerToMdlPointerUpdateInputs(
    _Inout_ MDL_POINTER* Destination,
    _Inout_ MDL_POINTER* Source,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either pointer's Offset is greater than the length
    of that pointer's MDL chain, this routine crashes the system with a fatal
    overflow error.

    This routine updates the MDL_POINTER parameters in-place to point to the
    end of the buffer that was processed. If the entire buffer was processed,
    the MDL_POINTER is set to a NULL Mdl with 0 Offset.

Arguments:

    Destination
        A pointer at which to begin writing

    Source
        A pointer at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlPairwiseIterateBuffersUpdateInputs(
        Destination,
        Source,
        CopyLength,
        PairwiseCopy,
        NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlChainToMdlChainAtOffset(
    _In_ MDL* DestinationMdlChain,
    _In_ SIZE_T DestinationOffset,
    _In_ MDL* SourceMdlChain,
    _In_ SIZE_T SourceOffset,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either Offset is greater than the length of the
    corresponding MDL chain, this routine crashes the system with a fatal
    overflow error.

Arguments:

    DestinationMdlChain
        The MDL chain to write into

    DestinationOffset
        The byte offset at which to begin writing

    SourceMdlChain
        The MDL chain to read from

    SourceOffset
        The byte offset at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_POINTER Source = { 0 };
    Source.Mdl = SourceMdlChain;
    Source.Offset = SourceOffset;

    MDL_POINTER Destination = { 0 };
    Destination.Mdl = DestinationMdlChain;
    Destination.Offset = DestinationOffset;

    return MdlCopyMdlPointerToMdlPointerUpdateInputs(
        &Destination,
        &Source,
        CopyLength);
}

#define WriteOperatorNonTemporal _MdlPrivate_WriteOperatorNonTemporal

MDL_BUFFER_OPERATOR WriteOperatorNonTemporal;

_Use_decl_annotations_
inline
NTSTATUS WriteOperatorNonTemporal(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR const** Source = (UCHAR const**)OperatorContext;

    UCHAR* Buffer = MDL_MAP_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemoryNonTemporal(Buffer + Span->Start.Offset, *Source, Span->Length);
    *Source += Span->Length;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyFlatBufferToMdlSpanNonTemporal(
    _In_ MDL_SPAN const* DestinationSpan,
    _In_reads_(DestinationSpan->Length) UCHAR const* SourceBuffer)
/*++

Routine Description:

    Copies data from a single buffer into an MDL chain

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    DestinationSpan
        The span of bytes to write into

    SourceBuffer
        The buffer to read from

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        DestinationSpan,
        WriteOperatorNonTemporal,
        &SourceBuffer);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyFlatBufferToMdlChainAtOffsetNonTemporal(
    _In_ MDL* DestinationMdlChain,
    _In_ SIZE_T DestinationOffset,
    _In_reads_(CopyLength) UCHAR const* SourceBuffer,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from a single buffer into an MDL chain

    If the DestinationOffset plus CopyLength is greater than the total length
    of the MDL chain, this routine crashes the system wtih a fatal overflow
    error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    DestinationMdlChain
        The MDL chain to write into

    DestinationOffset
        The byte offset at which to begin writing into

    SourceBuffer
        The buffer to read from

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Destination = { { 0 } };
    Destination.Start.Mdl = DestinationMdlChain;
    Destination.Start.Offset = DestinationOffset;
    Destination.Length = CopyLength;

    return MdlCopyFlatBufferToMdlSpanNonTemporal(
        &Destination,
        SourceBuffer);
}

#define ReadOperatorNonTemporal _MdlPrivate_ReadOperatorNonTemporal
MDL_BUFFER_OPERATOR ReadOperatorNonTemporal;

_Use_decl_annotations_
inline
NTSTATUS ReadOperatorNonTemporal(
    PVOID OperatorContext,
    MDL_SPAN const* Span)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UCHAR** Destination = (UCHAR**)OperatorContext;

    UCHAR const* Buffer = MDL_MAP_CONST_BUFFER(Span->Start.Mdl);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemoryNonTemporal(
        *Destination,
        Buffer + Span->Start.Offset,
        Span->Length);

    *Destination += Span->Length;

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlSpanToFlatBufferNonTemporal(
    _Out_writes_(Source->Length) UCHAR* DestinationBuffer,
    _In_ MDL_SPAN const* Source)
/*++

Routine Description:

    Copies data from an MDL span into a buffer

    If the Span extends past the end of the MDL chain, this routine crashes the
    system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    DestinationBuffer
        The buffer to write into

    Source
        The MDL span to read from

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlSpanIterateBuffers(
        Source,
        ReadOperatorNonTemporal,
        &DestinationBuffer);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlChainAtOffsetToFlatBufferNonTemporal(
    _Out_writes_(CopyLength) UCHAR* DestinationBuffer,
    _In_ MDL* SourceMdlChain,
    _In_ SIZE_T SourceOffset,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from an MDL chain into a buffer

    If SourceOffset plus CopyLength is greater than the total length of the MDL
    chain, this routine crashes the system with a fatal overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    DestinationBuffer
        The buffer to write into

    SourceMdlChain
        The MDL chain to read from

    SourceOffset
        The offset into the MDL chain at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_SPAN Source = { { 0 } };
    Source.Start.Mdl = SourceMdlChain;
    Source.Start.Offset = SourceOffset;
    Source.Length = CopyLength;

    return MdlCopyMdlSpanToFlatBufferNonTemporal(
        DestinationBuffer,
        &Source);
}

#define PairwiseCopyNonTemporal _MdlPrivate_PairwiseCopyNonTemporal

MDL_BUFFER_PAIRWISE_OPERATOR PairwiseCopyNonTemporal;

_Use_decl_annotations_
inline
NTSTATUS PairwiseCopyNonTemporal(
    PVOID OperatorContext,
    MDL_POINTER const* MdlPointer1,
    MDL_POINTER const* MdlPointer2,
    SIZE_T BufferLength)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR* DestinationBuffer = MDL_MAP_BUFFER(MdlPointer1->Mdl);
    UCHAR const* SourceBuffer = MDL_MAP_CONST_BUFFER(MdlPointer2->Mdl);

    if (!SourceBuffer || !DestinationBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemoryNonTemporal(
        DestinationBuffer + MdlPointer1->Offset,
        SourceBuffer + MdlPointer2->Offset,
        BufferLength);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlPointerToMdlPointerNonTemporal(
    _In_ MDL_POINTER const* Destination,
    _In_ MDL_POINTER const* Source,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either pointer's Offset is greater than the length
    of that pointer's MDL chain, this routine crashes the system with a fatal
    overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    Destination
        A pointer at which to begin writing

    Source
        A pointer at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlPairwiseIterateBuffers(
        Destination,
        Source,
        CopyLength,
        PairwiseCopyNonTemporal,
        NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlPointerToMdlPointerUpdateInputsNonTemporal(
    _Inout_ MDL_POINTER* Destination,
    _Inout_ MDL_POINTER* Source,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either pointer's Offset is greater than the length
    of that pointer's MDL chain, this routine crashes the system with a fatal
    overflow error.

    This routine updates the MDL_POINTER parameters in-place to point to the
    end of the buffer that was processed. If the entire buffer was processed,
    the MDL_POINTER is set to a NULL Mdl with 0 Offset.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    Destination
        A pointer at which to begin writing

    Source
        A pointer at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    return MdlPairwiseIterateBuffersUpdateInputs(
        Destination,
        Source,
        CopyLength,
        PairwiseCopyNonTemporal,
        NULL);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlCopyMdlChainToMdlChainAtOffsetNonTemporal(
    _In_ MDL* DestinationMdlChain,
    _In_ SIZE_T DestinationOffset,
    _In_ MDL* SourceMdlChain,
    _In_ SIZE_T SourceOffset,
    _In_ SIZE_T CopyLength)
/*++

Routine Description:

    Copies data from one MDL chain to another MDL chain

    If CopyLength plus either Offset is greater than the length of the
    corresponding MDL chain, this routine crashes the system with a fatal
    overflow error.

    If permitted by the processor, this routine uses non-temporal instructions
    to avoid placing the MDL buffers into the processor's data cache.

Arguments:

    DestinationMdlChain
        The MDL chain to write into

    DestinationOffset
        The byte offset at which to begin writing

    SourceMdlChain
        The MDL chain to read from

    SourceOffset
        The byte offset at which to begin reading

    CopyLength
        The number of bytes to copy

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_POINTER Source = { 0 };
    Source.Mdl = SourceMdlChain;
    Source.Offset = SourceOffset;

    MDL_POINTER Destination = { 0 };
    Destination.Mdl = DestinationMdlChain;
    Destination.Offset = DestinationOffset;

    return MdlCopyMdlPointerToMdlPointerUpdateInputsNonTemporal(
        &Destination,
        &Source,
        CopyLength);
}

#define PairwiseEqual _MdlPrivate_PairwiseEqual

MDL_BUFFER_PAIRWISE_OPERATOR PairwiseEqual;

_Use_decl_annotations_
inline
NTSTATUS PairwiseEqual(
    PVOID OperatorContext,
    MDL_POINTER const* MdlPointer1,
    MDL_POINTER const* MdlPointer2,
    SIZE_T BufferLength)
/*++

Routine Description:

    Implementation detail - do not invoke this routine directly

--*/
{
    UNREFERENCED_PARAMETER(OperatorContext);

    UCHAR const* Buffer1 = MDL_MAP_CONST_BUFFER(MdlPointer1->Mdl);
    UCHAR const* Buffer2 = MDL_MAP_CONST_BUFFER(MdlPointer2->Mdl);

    if (!Buffer1 || !Buffer2)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    BOOLEAN IsEqual = RtlEqualMemory(
        Buffer1 + MdlPointer1->Offset,
        Buffer2 + MdlPointer2->Offset,
        BufferLength);

    if (IsEqual)
    {
        // Keep going
        return STATUS_SUCCESS;
    }
    else
    {
        // At least one difference was found; stop iterating the MDL chain now
        return STATUS_STOP_ITERATION;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlEqualBufferContents(
    _In_ MDL_POINTER const* MdlPointer1,
    _In_ MDL_POINTER const* MdlPointer2,
    _In_ SIZE_T ComparisonLength,
    _Out_ BOOLEAN* AreBuffersEqual)
/*++

Routine Description:

    Determines if the contents of two MDL chains' buffers are bytewise equal

    If ComparisonLength plus either pointer's Offset is greater than the total
    length of the corresponding MDL chain, this routine crashes the system with
    a fatal overflow error.

Arguments:

    MdlPointer1
        The first MDL chain to compare

    MdlPointer2
        The second MDL chain to compare

    ComparisonLength
        The number of bytes to compare

    AreBuffersEqual
        Receives TRUE if the buffers are exactly equal, or FALSE otherwise

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    NTSTATUS NtStatus = MdlPairwiseIterateBuffers(
        MdlPointer1,
        MdlPointer2,
        ComparisonLength,
        PairwiseEqual,
        NULL);

    if (STATUS_SUCCESS == NtStatus)
    {
        *AreBuffersEqual = TRUE;
        return STATUS_SUCCESS;
    }
    else if (STATUS_STOP_ITERATION == NtStatus)
    {
        *AreBuffersEqual = FALSE;
        return STATUS_SUCCESS;
    }
    else
    {
        *AreBuffersEqual = FALSE;
        return NtStatus;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlEqualBufferContentsUpdateInputs(
    _Inout_ MDL_POINTER* MdlPointer1,
    _Inout_ MDL_POINTER* MdlPointer2,
    _In_ SIZE_T ComparisonLength,
    _Out_ BOOLEAN* AreBuffersEqual)
/*++

Routine Description:

    Determines if the contents of two MDL chains' buffers are bytewise equal

    If ComparisonLength plus either pointer's Offset is greater than the total
    length of the corresponding MDL chain, this routine crashes the system with
    a fatal overflow error.

    This routine updates the MDL_POINTER parameters in-place to point to the
    end of the buffer that was processed. If the entire buffer was processed,
    the MDL_POINTER is set to a NULL Mdl with 0 Offset.

Arguments:

    MdlPointer1
        The first MDL chain to compare

    MdlPointer2
        The second MDL chain to compare

    ComparisonLength
        The number of bytes to compare

    AreBuffersEqual
        Receives TRUE if the buffers are exactly equal, or FALSE otherwise

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    NTSTATUS NtStatus = MdlPairwiseIterateBuffersUpdateInputs(
        MdlPointer1,
        MdlPointer2,
        ComparisonLength,
        PairwiseEqual,
        NULL);

    if (STATUS_SUCCESS == NtStatus)
    {
        *AreBuffersEqual = TRUE;
        return STATUS_SUCCESS;
    }
    else if (STATUS_STOP_ITERATION == NtStatus)
    {
        *AreBuffersEqual = FALSE;
        return STATUS_SUCCESS;
    }
    else
    {
        *AreBuffersEqual = FALSE;
        return NtStatus;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NTSTATUS
MdlEqualBufferContentsAtOffset(
    _In_ MDL* MdlChain1,
    _In_ SIZE_T Offset1,
    _In_ MDL* MdlChain2,
    _In_ SIZE_T Offset2,
    _In_ SIZE_T ComparisonLength,
    _Out_ BOOLEAN* AreBuffersEqual)
/*++

Routine Description:

    Determines if the contents of two MDL chains' buffers are bytewise equal

    If ComparisonLength plus either Offset is greater than the total length of
    the corresponding MDL chain, this routine crashes the system with a fatal
    overflow error.

Arguments:

    MdlChain1
        The first MDL to compare

    Offset1
        The byte offset into the first MDL chain at which to begin the comparison

    MdlChain2
        The second MDL to compare

    Offset2
        The byte offset into the second MDL chain at which to begin the comparison

    ComparisonLength
        The number of bytes to compare

    AreBuffersEqual
        Receives TRUE if the buffers are exactly equal, or FALSE otherwise

Return Value:

    STATUS_SUCCESS
        Every buffer was processed successfully

    STATUS_INSUFFICIENT_RESOURCES
        The system was unable to map an MDL into system address space

--*/
{
    MDL_POINTER MdlPointer1 = { 0 };
    MdlPointer1.Mdl = MdlChain1;
    MdlPointer1.Offset = Offset1;

    MDL_POINTER MdlPointer2 = { 0 };
    MdlPointer2.Mdl = MdlChain2;
    MdlPointer2.Offset = Offset2;

    return MdlEqualBufferContentsUpdateInputs(
        &MdlPointer1,
        &MdlPointer2,
        ComparisonLength,
        AreBuffersEqual);
}

#undef STATUS_STOP_ITERATION
#undef ReportFatalOverflow
#undef MinSizeT
#undef MapOperator
#undef SEEK_OPERATOR_CONTEXT_t
#undef SEEK_OPERATOR_CONTEXT
#undef SeekOperator
#undef ZeroOperator
#undef ZeroOperatorNonTemporal
#undef ZeroOperatorSecure
#undef FillOperator
#undef FillOperatorNonTemporal
#undef WriteOperator
#undef ReadOperator
#undef PairwiseCopy
#undef WriteOperatorNonTemporal
#undef ReadOperatorNonTemporal
#undef PairwiseCopyNonTemporal
#undef PairwiseEqual

#pragma warning(pop)

