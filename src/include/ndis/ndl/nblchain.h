/*++

    Copyright (c) Microsoft. All rights reserved.

    This code is licensed under the MIT License.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
    PARTICULAR PURPOSE AND NONINFRINGEMENT.

Module Name:

    nblchain.h

Provenance:

    Version 1.2.0 from https://github.com/microsoft/ndis-driver-library

Abstract:

    Utility functions for handling lists of NET_BUFFER_LISTs (NBLs) and lists
    of NET_BUFFERs (NBs)

    NBLs are typically linked into a singly-linked list.  A list of NBLs is
    called an NBL chain.  Unless explicitly stated otherwise, an NBL chain must
    have at least 1 NBL in it.

    This module defines several small utility routines for operating on NBL
    chains.

Table of Contents:

        NdisNumNblsInNblChain
        NdisNumNbsInNbChain
        NdisNumNbsInNblChain
        NdisNumDataBytesInNbChain
        NdisNumDataBytesInNblChain
        NdisLastNblInNblChain
        NdisLastNblInNblChainWithCount
        NdisLastNbInNbChain
        NdisLastNbInNbChainWithCount
        NdisSetStatusInNblChain

Environment:

    Kernel mode

--*/

#pragma once

#pragma region System Family (kernel drivers) with Desktop Family for compat
#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_DESKTOP)

#ifndef NDIS_ASSERT
#    define NDIS_ASSERT(x) NT_ASSERT(x)
#endif

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
ULONG
NdisNumNblsInNblChain(
    _In_opt_ NET_BUFFER_LIST const *NblChain)
/*++

Routine Description:

    Returns the number of NBLs in an NBL chain

Arguments:

    NblChain - Zero or more NBLs

Return Value:

    Number of NBLs the NBL chain

--*/
{
    ULONG Count = 0;

    while (NblChain)
    {
        Count++;
        NblChain = NblChain->Next;
    }

    return Count;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
ULONG
NdisNumNbsInNbChain(
    _In_opt_ NET_BUFFER const *NbChain)
/*++

Routine Description:

    Returns the number of NBs in an NB chain

Arguments:

    NbChain - Zero or more NET_BUFFERs

Return Value:

    Number of NBs in the NB chain

--*/
{
    ULONG Count = 0;

    while (NbChain)
    {
        Count++;
        NbChain = NbChain->Next;
    }

    return Count;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
ULONG
NdisNumNbsInNblChain(
    _In_opt_ NET_BUFFER_LIST const *NblChain)
/*++

Routine Description:

    Returns the number of NBs in each NBL an NBL chain

Arguments:

    NblChain - Zero or more NBLs

Return Value:

    Number of NBs in the NBL chain

--*/
{
    ULONG Count = 0;

    while (NblChain)
    {
        Count += NdisNumNbsInNbChain(NblChain->FirstNetBuffer);
        NblChain = NblChain->Next;
    }

    return Count;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
ULONG64
NdisNumDataBytesInNbChain(
    _In_opt_ NET_BUFFER const *NbChain)
/*++

Routine Description:

    Returns the number of bytes of data in each NB of an NB chain

Arguments:

    NbChain - Zero or more NET_BUFFERs

Return Value:

    Number of bytes of data in the NB chain

--*/
{
    ULONG64 ByteCount = 0;

    while (NbChain)
    {
        ByteCount += NbChain->DataLength;
        NbChain = NbChain->Next;
    }

    return ByteCount;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
ULONG64
NdisNumDataBytesInNblChain(
    _In_opt_ NET_BUFFER_LIST const *NblChain)
/*++

Routine Description:

    Returns the number of bytes of data in each NB of an NBL chain

Arguments:

    NblChain - Zero or more NBLs

Return Value:

    Number of bytes of data in the NBL chain

--*/
{
    ULONG64 ByteCount = 0;

    while (NblChain)
    {
        ByteCount += NdisNumDataBytesInNbChain(NblChain->FirstNetBuffer);
        NblChain = NblChain->Next;
    }

    return ByteCount;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER_LIST *
NdisLastNblInNblChain(
    _In_ NET_BUFFER_LIST *NblChain)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NblChain - One or more NBLs

Return Value:

    The last NBL in the chain

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *Next;

    while (NULL != (Next = Nbl->Next))
    {
        Nbl = Next;
    }

    return Nbl;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER_LIST *
NdisLastNblInNblChainWithCount(
    _In_ NET_BUFFER_LIST *NblChain,
    _Out_ SIZE_T *NblCount)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NblChain - One or more NBLs

    NblCount - Receives the number of NBLs in NblChain

Return Value:

    The last NBL in the chain

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *Next;
    SIZE_T Count = 1;

    while (NULL != (Next = Nbl->Next))
    {
        Count++;
        Nbl = Next;
    }

    *NblCount = Count;
    return Nbl;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER *
NdisLastNbInNbChain(
    _In_ NET_BUFFER *NbChain)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NbChain - One or more NET_BUFFERs

Return Value:

    The last NB in the chain

--*/
{
    NET_BUFFER *Nb = NbChain;
    NET_BUFFER *Next;

    while (NULL != (Next = Nb->Next))
    {
        Nb = Next;
    }

    return Nb;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER *
NdisLastNbInNbChainWithCount(
    _In_ NET_BUFFER *NbChain,
    _Out_ SIZE_T *NbCount)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NbChain - One or more NET_BUFFERs

    NblCount - Receives the number of NET_BUFFERs in NbChain

Return Value:

    The last NB in the chain

--*/
{
    NET_BUFFER *Nb = NbChain;
    NET_BUFFER *Next;
    SIZE_T Count = 1;

    while (NULL != (Next = Nb->Next))
    {
        Count++;
        Nb = Next;
    }

    *NbCount = Count;
    return Nb;
}

#ifdef __cplusplus

// Repeat the NdisLastXxxInXxxChain functions with `const` inputs & outputs.

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER_LIST const *
NdisLastNblInNblChain(
    _In_ NET_BUFFER_LIST const *NblChain)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NblChain - One or more NBLs

Return Value:

    The last NBL in the chain

--*/
{
    NET_BUFFER_LIST const *Nbl = NblChain;
    NET_BUFFER_LIST const *Next;

    while (NULL != (Next = Nbl->Next))
    {
        Nbl = Next;
    }

    return Nbl;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER_LIST const *
NdisLastNblInNblChainWithCount(
    _In_ NET_BUFFER_LIST const *NblChain,
    _Out_ SIZE_T *NblCount)
/*++

Routine Description:

    Returns the last NBL in an NBL chain

Arguments:

    NblChain - One or more NBLs

    NblCount - Receives the number of NBLs in NblChain

Return Value:

    The last NBL in the chain

--*/
{
    NET_BUFFER_LIST const *Nbl = NblChain;
    NET_BUFFER_LIST const *Next;
    SIZE_T Count = 1;

    while (NULL != (Next = Nbl->Next))
    {
        Count++;
        Nbl = Next;
    }

    *NblCount = Count;
    return Nbl;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER const *
NdisLastNbInNbChain(
    _In_ NET_BUFFER const *NbChain)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NbChain - One or more NET_BUFFERs

Return Value:

    The last NB in the chain

--*/
{
    NET_BUFFER const *Nb = NbChain;
    NET_BUFFER const *Next;

    while (NULL != (Next = Nb->Next))
    {
        Nb = Next;
    }

    return Nb;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
NET_BUFFER const *
NdisLastNbInNbChainWithCount(
    _In_ NET_BUFFER const *NbChain,
    _Out_ SIZE_T *NbCount)
/*++

Routine Description:

    Returns the last NB in an NB chain

Arguments:

    NbChain - One or more NET_BUFFERs

    NblCount - Receives the number of NET_BUFFERs in NbChain

Return Value:

    The last NB in the chain

--*/
{
    NET_BUFFER const *Nb = NbChain;
    NET_BUFFER const *Next;
    SIZE_T Count = 1;

    while (NULL != (Next = Nb->Next))
    {
        Count++;
        Nb = Next;
    }

    *NbCount = Count;
    return Nb;
}

#endif // __cplusplus

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NDIS_ASSERT_NBL_CHAINS_DO_NOT_OVERLAP(
    _In_ NET_BUFFER_LIST const *Chain1,
    _In_ NET_BUFFER_LIST const *Chain2)
/*++

Routine Description:

    Verifies that two NBL chains have no overlap

    Consider the following example:

        Chain1:  A->B->C->D->NULL
        Chain2:  C->D->NULL

    In this example, Chain1 and Chain2 overlap (they have elements C & D in
    common) so this routine would assert.

Arguments:

    Chain1

    Chain2

--*/
{
#if DBG

    if (Chain1 == NULL || Chain2 == NULL)
    {
        // The empty set has no common element with any other set.
        return;
    }

    NDIS_ASSERT(
        NdisLastNblInNblChain((NET_BUFFER_LIST *)Chain1) != NdisLastNblInNblChain((NET_BUFFER_LIST *)Chain2));

#else
    UNREFERENCED_PARAMETER((Chain1, Chain2));
#endif
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisSetStatusInNblChain(
    _In_opt_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_STATUS NdisStatus)
/*++

Routine Description:

    Sets the NBL->Status of each NBL in the chain to NdisStatus

Arguments:

    NblChain - Zero or more NBLs

    NdisStatus - A status code to assign to each NBL

--*/
{
    while (NblChain)
    {
        NblChain->Status = NdisStatus;
        NblChain = NblChain->Next;
    }
}

#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_DESKTOP)
#pragma endregion
