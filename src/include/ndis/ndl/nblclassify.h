/*++

    Copyright (c) Microsoft. All rights reserved.

    This code is licensed under the MIT License.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
    PARTICULAR PURPOSE AND NONINFRINGEMENT.

Module Name:

    nblclassify.h

Provenance:

    Version 1.0.0 from https://github.com/microsoft/ndis-driver-library

Abstract:

    Routines for classifying NET_BUFFER_LISTs (NBLs) into different buckets

    Drivers often take a single linked list of NBLs and separate it out into
    two or more lists of NBLs.  This header provides several easy-to-use
    classification routines.  These routines improve the readability of your
    code, since you don't have to worry about the tedious mechanics of slicing
    singly-linked lists, and you can focus on higher-level problems.

    This ease-of-use does *not* come with a performance penalty.  In fact, the
    implementations here are quite a bit faster than the naive implementation
    of list-slicing.

    The high-level concept is that you have a bunch of NBLs come in, and you
    split that into two or more buckets of NBLs.  Typical use cases:

        * Drop invalid packets
        * Separate out TCP, UDP, and "other" traffic
        * Divert NBLs that your LWF had previously injected

    The most generic algorithm can classify NBLs into N buckets, where N is
    very large and is not known at compile time.  But the algorithm can be made
    more efficient if you know there's a small and constant number of buckets.
    So this header offers a few algorithms, each optimized for the
    number of buckets you'll need to classify into.

Two buckets:

    NdisClassifyNblChain2 classifies NBLs into exactly 2 buckets.  For example,
    you can use this routine to drop malformed packets:

        void ReceivePackets(NET_BUFFER_LIST *incomingNbls)
        {
            NBL_QUEUE drop, good;
            NdisInitializeNblQueue(&drop);
            NdisInitializeNblQueue(&good);

            NdisClassifyNblChain2(incomingNbls, IsValidNbl, ..., &drop, &good);

            DropPackets(&drop);
            ProcessPackets(&good);
        }

        ULONG_PTR IsValidNbl(PVOID ClassificationContext, NET_BUFFER_LIST *Nbl)
        {
            if (. . .)
                return 0; // put the NBL into the "drop" queue

            return 1; // put the NBL into the "good" queue
        }

A few buckets:

    NdisClassifyNblChainByIndex classifies NBLs into a small number of buckets,
    where you can easily identify the bucket by index.  For example, you can
    classify packets into the 3 buckets of: IPv4, IPv6, and NonIP.

        void ReceivePackets(NET_BUFFER_LIST *incomingNbls)
        {
            NBL_QUEUE queues[3];
            for (int i = 0; i < ARRAYSIZE(queues); i++)
                NdisInitializeNblQueue(queues[i]);

            NdisClassifyNblChainByIndex(
                incomingNbls, ClassifyProtocol, ..., queues, ARRAYSIZE(queues));

            ProcessIPv4Packets(&queues[0]);
            ProcessIPv6Packets(&queues[1]);
            ProcessNonIPPackets(&queues[2]);
        }

        SIZE_T ClassifyProtocol(PVOID ClassificationContext, NET_BUFFER_LIST *Nbl)
        {
            if (. . . Nbl is IPv4 . . .)
                return 0;
            if (. . . Nbl is IPv6 . . .)
                return 1;

            return 2; // put the NBL into the NonIP queue
        }

Many buckets (with callback):

    NdisClassifyNblChainByValue classifies NBLs into a potentially very large
    number of buckets.  For example, 802.1Q has 4094 possible values for VLAN
    IDs (not counting untagged traffic).  It would be wasteful to allocate four
    thousand NBL queues, as NdisClassifyNblChainByIndex would require.
    Instead, NdisClassifyNblChainByValue allows you to bucket NBLs by any
    opaque integer or pointer value.  The algorithm doesn't need unbounded
    storage, because it invokes a "flush batch" callback each time a new bucket
    is encountered.

    Suppose we are given this chain of 5 NBLs with various VLAN IDs, and we
    need to dispatch the NBLs to different places depending on their VLAN:

        A[VLAN=1] -> B[VLAN=1] -> C[VLAN=2] -> D[VLAN=2] -> E[VLAN=1]

    We can bucket this list by VLAN like by providing two callbacks -- one to
    return the VLAN of an NBL, and the other to actually do work once a batch
    of similar NBLs has been identified:

        void ReceivePackets(NET_BUFFER_LIST *incomingNbls)
        {
            NdisClassifyNblChainByValue(
                incomingNbls, GetVlan, NULL, ReceivePacketsOnVlan, NULL);
        }

        ULONG_PTR GetVlan(void *unused, NET_BUFFER_LIST *nbl)
        {
            return NDIS_GET_NET_BUFFER_LIST_VLAN_ID(nbl);
        }

        VOID ReceivePacketsOnVlan(void *unused, ULONG_PTR vlan, NET_BUFFER_LIST *nblChain)
        {
            DispatchInputForVlan(nblChain, (USHORT)vlan);
        }

    The usage above will invoke your callback 3 times: first with the chain
    A->B and VLAN=1; then with the chain C->D and VLAN=2; and finally with the
    chain E and VLAN=1.

    Note that NdisClassifyNblChainByValue found 3 batches of NBLs, but the 1st
    and 3rd batch have the same VLAN, so those batches could have been
    combined.  Use NdisClassifyNblChainByValueLookahead to "try harder" to find
    batches.  It's a drop-in replacement, so the example code is the same as
    above.

    NdisClassifyNblChainByValueLookahead will invoke your flush callback 2
    times: first with the chain A->B->E and VLAN=1; then with the chain C->D
    and VLAN=2.  This improvement in batch size comes at a cost of complexity:
    the algorithm does more work up front, in the hopes that you'll save cycles
    later by having bigger batches.

Many buckets (no callback function):

    If it's inconvenient to process NBLs within a callback function, you can
    alternatively use NdisPartialClassifyNblChainByValue.  This routine removes
    the first bucket's NBLs from the NBL chain, then stops.  You'd typically
    execute it in a loop until the NBL chain is empty:

        void ReceivePackets(NET_BUFFER_LIST *incomingNbls)
        {
            do
            {
                NBL_QUEUE queue;
                NdisInitializeNblQueue(&queue);

                ULONG_PTR vlan;
                NdisPartialClassifyNblChainByValue(
                    &incomingNbls, GetVlan, NULL, &queue, &vlan);

                // All the NBLs in `queue` now have the same VLAN.
                DispatchInputForVlan(queue.First, (USHORT)vlan);

            } while (incomingNbls != NULL);
        }

    This code above will loop 3 times: first with the chain A->B and VLAN=1;
    then with the chain C->D and VLAN=2; and finally with the chain E and
    VLAN=1.

Specific classifiers:

    For your convenience, this header provides several pre-created classifiers
    that can classify NBLs based on commonly-used criteria:

        NdisClassifyNblChainByCancelId for NET_BUFFER_LIST::CancelId
        NdisClassifyNblChainBySourceHandle for NET_BUFFER_LIST::SourceHandle
        NdisClassifyNblChainByPoolHandle for NET_BUFFER_LIST::NdisPoolHandle

    For example, suppose you have this chain of NBLs:

        A[CancelId=4] => B[CancelId=3] => C[CancelId=2] => D[CancelId=3] => NULL

    and you need to remove all NBLs whose CancelId is 3.  You can invoke
    NdisClassifyNblChainByCancelId like this:

        void CancelSendNetBufferLists(PVOID cancelId)
        {
            NBL_QUEUE keep;
            NBL_QUEUE cancel;

            NdisInitializeNblQueue(&keep);
            NdisInitializeNblQueue(&cancel);
            NdisClassifyNblChainByCancelId(My->NblChain, cancelId, &keep, &cancel);

            My->NblChain = keep.First;

            NdisSetStatusInNblChain(cancel.First, NDIS_STATUS_CANCELLED);
            NdisSendNetBufferListsComplete(cancel.First);
        }

    After this routine executes, `keep` contains

        A[CancelId=4] => C[CancelId=2] => NULL

    and `cancel` contains

        B[CancelId=3] => D[CancelId=3] => NULL

Table of Contents:

        NdisClassifyNblChain2
        NdisClassifyNblChain2WithCount
        NdisClassifyNblChainByIndex
        NdisClassifyNblChainByIndexWithCount
        NdisClassifyNblChainByValue
        NdisClassifyNblChainByValueWithCount
        NdisClassifyNblChainByValueLookahead
        NdisClassifyNblChainByValueLookaheadWithCount
        NdisPartialClassifyNblChainByValue
        NdisPartialClassifyNblChainByValueWithCount

        NdisClassifyNblChainByCancelId
        NdisClassifyNblChainBySourceHandle
        NdisClassifyNblChainByPoolHandle

Environment:

    Kernel mode

--*/

#pragma once

#pragma region System Family (kernel drivers) with Desktop Family for compat
#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_DESKTOP)

#include <ndis/ndl/nblqueue.h>

#ifndef NDIS_CLASSIFY_NBL_LOOKHEAD_DEPTH
#   define NDIS_CLASSIFY_NBL_LOOKHEAD_DEPTH 4
#endif

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK)
SIZE_T
NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK(
    _In_ PVOID ClassificationContext,
    _In_ NET_BUFFER_LIST *Nbl);
/*++

Routine Description:

    You implement a callback with this signature to inform
    NdisClassifyNblChainByIndex how to classify each NBL in the chain.

Arguments:

    ClassificationContext - Any optional context you'd like to pass to your callback

    Nbl - The NBL to inspect

Return Value:

    When used with an NdisClassifyXxx routine:
        0 if the NBL should go into Queue0
        1 if the NBL should go into Queue1
        No other return value is allowed.

    When used with an NdisMultiClassifyXxx routine:
        The index of the Queue that receives the NBL

        For example, if NumberOfQueues==5, then this callback may return any
        number in the range [0, 5).

--*/

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK)
ULONG_PTR
NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK(
    _In_ PVOID ClassificationContext,
    _In_ NET_BUFFER_LIST *Nbl);
/*++

Routine Description:

    You implement a callback with this signature to inform
    NdisClassifyNblChainByValue how to classify each NBL in the chain.

Arguments:

    ClassificationContext - Any optional context you'd like to pass to your callback

    Nbl - The NBL to inspect

Return Value:

    Return any opaque ULONG_PTR (i.e., an integer or pointer value)

    Two NBLs are considered to be similar if and only if their ULONG_PTR values
    are the same.

--*/

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(NDIS_NBL_FLUSH_CALLBACK)
VOID
NDIS_NBL_FLUSH_CALLBACK(
    _In_ PVOID FlushContext,
    _In_ ULONG_PTR ClassificationResult,
    _In_ NBL_QUEUE *Queue);
/*++

Routine Description:

    You implement a callback with this signature to receive batches of similar
    NBLs from NdisInvokeCallbackOnHomogenousNblsXxx

Arguments:

    FlushContext - Any optional context you'd like to pass to your callback

    ClassificationResult - The output of your ClassificationRoutine on each of
        the NBLs in NblChain

    Queue - A queue of similar NBLs, as defined by your ClassificationRoutine

--*/

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
_Function_class_(NDIS_NBL_FLUSH_CALLBACK_WITH_COUNT)
VOID
NDIS_NBL_FLUSH_WITH_COUNT_CALLBACK(
    _In_ PVOID FlushContext,
    _In_ ULONG_PTR ClassificationResult,
    _In_ NBL_COUNTED_QUEUE *Queue);
/*++

Routine Description:

    You implement a callback with this signature to receive batches of similar
    NBLs from NdisInvokeCallbackOnHomogenousNblsXxx

Arguments:

    FlushContext - Any optional context you'd like to pass to your callback

    ClassificationResult - The output of your ClassificationRoutine on each of
        the NBLs in NblChain

    Queue - A queue of similar NBLs, as defined by your ClassificationRoutine

--*/

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChain2(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Inout_ NBL_QUEUE *Queue0,
    _Inout_ NBL_QUEUE *Queue1)
/*++

Routine Description:

    Separates an NBL chain into 2 queues, based on a classification function
    that you provide

Arguments:

    NblChain - A chain of NBLs to sift through

    ClassificationCallback - A function (that you implement) to classify NBLs

    ClassificationContext - Any optional context you'd like to pass to your callback

    Queue0 - Receives all NBLs for which the classifier returns 0

    Queue1 - Receives all NBLs for which the classifier returns 1

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *First = Nbl;
    NET_BUFFER_LIST *Previous;
    SIZE_T CurrentIndex;
    SIZE_T ThisIndex;

    NDIS_ASSERT_VALID_NBL_QUEUE(Queue0);
    NDIS_ASSERT_VALID_NBL_QUEUE(Queue1);

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
    NDIS_ASSERT(ThisIndex == 0 || ThisIndex == 1);

    CurrentIndex = ThisIndex;
    Previous = Nbl;
    Nbl = Nbl->Next;

    while (Nbl != NULL)
    {
        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
        NDIS_ASSERT(ThisIndex == 0 || ThisIndex == 1);

        if (ThisIndex != CurrentIndex)
        {
            NBL_QUEUE *const Q = (CurrentIndex == 0) ? Queue0 : Queue1;

            Previous->Next = NULL;
            NdisAppendNblChainToNblQueueFast(Q, First, Previous);

            CurrentIndex = ThisIndex;
            First = Nbl;
        }

        Previous = Nbl;
        Nbl = Nbl->Next;
    }

    NBL_QUEUE *Q = (CurrentIndex == 0) ? Queue0 : Queue1;
    NdisAppendNblChainToNblQueueFast(Q, First, Previous);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChain2WithCount(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK  *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Inout_ NBL_COUNTED_QUEUE *Queue0,
    _Inout_ NBL_COUNTED_QUEUE *Queue1)
/*++

Routine Description:

    Similar to NdisClassifyNblChain, except results are provided in
    NBL_COUNTED_QUEUEs.  Refer to the documentation for NdisClassifyNblChain.

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *First = Nbl;
    NET_BUFFER_LIST *Previous;
    SIZE_T CurrentIndex;
    SIZE_T ThisIndex;
    SIZE_T Count = 1;

    NDIS_ASSERT_VALID_NBL_COUNTED_QUEUE(Queue0);
    NDIS_ASSERT_VALID_NBL_COUNTED_QUEUE(Queue1);

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
    NDIS_ASSERT(ThisIndex == 0 || ThisIndex == 1);

    CurrentIndex = ThisIndex;
    Previous = Nbl;
    Nbl = Nbl->Next;

    while (Nbl != NULL)
    {
        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
        NDIS_ASSERT(ThisIndex == 0 || ThisIndex == 1);

        if (ThisIndex != CurrentIndex)
        {
            NBL_COUNTED_QUEUE *const Q = (CurrentIndex == 0) ? Queue0 : Queue1;

            Previous->Next = NULL;
            NdisAppendNblChainToNblCountedQueueFast(Q, First, Previous, Count);

            CurrentIndex = ThisIndex;
            First = Nbl;
            Count = 1;
        }
        else
        {
            Count++;
        }

        Previous = Nbl;
        Nbl = Nbl->Next;
    }

    NBL_COUNTED_QUEUE *Q = (CurrentIndex == 0) ? Queue0 : Queue1;
    NdisAppendNblChainToNblCountedQueueFast(Q, First, Previous, Count);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByIndex(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Inout_updates_(NumberOfQueues) NBL_QUEUE *Queues,
    _In_ SIZE_T NumberOfQueues)
/*++

Routine Description:

    Separates an NBL chain into N queues, based on a classification function
    that you provide

Arguments:

    NblChain - A chain of NBLs to sift through

    ClassificationCallback - A function (that you implement) to classify NBLs

    ClassificationContext - Any optional context you'd like to pass to your callback

    Queues - An array of initialized NBL_QUEUEs

    NumberOfQueues - The number of elements in the array

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *First = Nbl;
    NET_BUFFER_LIST *Previous;
    SIZE_T CurrentIndex;
    SIZE_T ThisIndex;

#if DBG
    for (SIZE_T i = 0; i < NumberOfQueues; i++)
    {
        NDIS_ASSERT_VALID_NBL_QUEUE(&Queues[i]);
    }
#else
    UNREFERENCED_PARAMETER(NumberOfQueues);
#endif

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
    NDIS_ASSERT(ThisIndex < NumberOfQueues);

    CurrentIndex = ThisIndex;
    Previous = Nbl;
    Nbl = Nbl->Next;

    while (Nbl != NULL)
    {
        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
        NDIS_ASSERT(ThisIndex < NumberOfQueues);

        if (ThisIndex != CurrentIndex)
        {
            Previous->Next = NULL;
            NdisAppendNblChainToNblQueueFast(&Queues[CurrentIndex], First, Previous);

            CurrentIndex = ThisIndex;
            First = Nbl;
        }

        Previous = Nbl;
        Nbl = Nbl->Next;
    }

    NdisAppendNblChainToNblQueueFast(&Queues[CurrentIndex], First, Previous);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByIndexWithCount(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Inout_updates_(NumberOfQueues) NBL_COUNTED_QUEUE *Queues,
    _In_ SIZE_T NumberOfQueues)
/*++

Routine Description:

    Similar to NdisClassifyNblChainByIndex, except results are provided in
    NBL_COUNTED_QUEUEs.  Refer to the documentation for
    NdisClassifyNblChainByIndex.

--*/
{
    NET_BUFFER_LIST *Nbl = NblChain;
    NET_BUFFER_LIST *First = Nbl;
    NET_BUFFER_LIST *Previous;
    SIZE_T CurrentIndex;
    SIZE_T ThisIndex;
    SIZE_T Count = 1;

#if DBG
    ULONG i;
    for (i = 0; i < NumberOfQueues; i++)
    {
        NDIS_ASSERT_VALID_NBL_COUNTED_QUEUE(&Queues[i]);
    }
#else
    UNREFERENCED_PARAMETER(NumberOfQueues);
#endif

    PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
    NDIS_ASSERT(ThisIndex < NumberOfQueues);

    CurrentIndex = ThisIndex;
    Previous = Nbl;
    Nbl = Nbl->Next;

    while (Nbl != NULL)
    {
        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ThisIndex = ClassificationCallback(ClassificationContext, Nbl);
        NDIS_ASSERT(ThisIndex < NumberOfQueues);

        if (ThisIndex != CurrentIndex)
        {
            Previous->Next = NULL;
            NdisAppendNblChainToNblCountedQueueFast(&Queues[CurrentIndex], First, Previous, Count);

            CurrentIndex = ThisIndex;
            First = Nbl;
            Count = 1;
        }
        else
        {
            Count++;
        }

        Previous = Nbl;
        Nbl = Nbl->Next;
    }

    NdisAppendNblChainToNblCountedQueueFast(&Queues[ThisIndex], First, Previous, Count);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByValue(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _In_ NDIS_NBL_FLUSH_CALLBACK *FlushCallback,
    _In_opt_ PVOID FlushContext)
/*++

Routine Description:

    This routine calls your callback with batches of similar NBLs

    Similarity is defined by a classification callback function that you
    provide.  Two NBLs are considered to be similar if and only if your
    classification callback returns the same integer value for each.

Arguments

    NblChain - An NBL chain that contains the input.  The chain will be
        unlinked as part of the operation of this routine.

    ClassificationCallback - Callback that returns an integer (or pointer) that
        indicates whether two NBLs should be batched together

    ClassificationContext - Any optional context you'd like to pass to your callback

    FlushCallback - Callback that is called with each batch of homogenous NBLs

    FlushContext - Any optional context you'd like to pass to your callback

--*/
{
    NBL_QUEUE Queue;
    NdisInitializeNblQueue(&Queue);

    NET_BUFFER_LIST *FirstNbl = NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ULONG_PTR TargetClassification = ClassificationCallback(ClassificationContext, Nbl);

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            NdisAppendNblChainToNblQueueFast(&Queue, FirstNbl, PreviousNbl);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification, &Queue);
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);
        if (NextClassification != TargetClassification)
        {
            PreviousNbl->Next = NULL;
            NdisAppendNblChainToNblQueueFast(&Queue, FirstNbl, PreviousNbl);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification, &Queue);

            NdisInitializeNblQueue(&Queue);
            FirstNbl = Nbl;
            TargetClassification = NextClassification;
        }

        PreviousNbl = Nbl;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByValueWithCount(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _In_ NDIS_NBL_FLUSH_WITH_COUNT_CALLBACK *FlushCallback,
    _In_opt_ PVOID FlushContext)
/*++

Routine Description:

    Similar to NdisClassifyNblChainByValue, except results are provided in an
    NBL_COUNTED_QUEUE.  Refer to the documentation for
    NdisClassifyNblChainByValue.

--*/
{
    NBL_COUNTED_QUEUE Queue;
    NdisInitializeNblCountedQueue(&Queue);

    NET_BUFFER_LIST *FirstNbl = NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ULONG_PTR TargetClassification = ClassificationCallback(ClassificationContext, Nbl);
    SIZE_T Count = 1;

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            NdisAppendNblChainToNblCountedQueueFast(&Queue, FirstNbl, PreviousNbl, Count);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification, &Queue);
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);
        if (NextClassification == TargetClassification)
        {
            Count += 1;
        }
        else
        {
            PreviousNbl->Next = NULL;
            NdisAppendNblChainToNblCountedQueueFast(&Queue, FirstNbl, PreviousNbl, Count);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification, &Queue);

            NdisInitializeNblCountedQueue(&Queue);
            FirstNbl = Nbl;
            TargetClassification = NextClassification;
            Count = 1;
        }

        PreviousNbl = Nbl;
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByValueLookahead(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _In_ NDIS_NBL_FLUSH_CALLBACK *FlushCallback,
    _In_opt_ PVOID FlushContext)
/*++

Routine Description:

    This routine calls your callback with batches of similar NBLs

    Similarity is defined by a classification callback function that you
    provide.  Two NBLs are considered to be similar if and only if your
    classification callback returns the same integer value for each.

    This routine is a drop-in replacement to NdisClassifyNblChainByValue.  The
    difference is that this function tries harder to accumulate larger batches
    of NBLs, at the expense of more time spent classifying them.

Arguments

    NblChain - An NBL chain that contains the input.  The chain will be
        unlinked as part of the operation of this routine.

    ClassificationCallback - Callback that returns an integer (or pointer) that
        indicates whether two NBLs should be batched together

    ClassificationContext - Any optional context you'd like to pass to your callback

    FlushCallback - Callback that is called with each batch of homogenous NBLs

    FlushContext - Any optional context you'd like to pass to your callback

--*/
{
    NBL_QUEUE Queue[NDIS_CLASSIFY_NBL_LOOKHEAD_DEPTH];
    ULONG_PTR TargetClassification[ARRAYSIZE(Queue)];
    BOOLEAN Valid[ARRAYSIZE(Queue)] = { TRUE };

    NET_BUFFER_LIST *FirstNbl = NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;

    SIZE_T PreviousIndex = 0;
    NdisInitializeNblQueue(&Queue[PreviousIndex]);
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    TargetClassification[PreviousIndex] = ClassificationCallback(ClassificationContext, Nbl);

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            NdisAppendNblChainToNblQueueFast(&Queue[PreviousIndex], FirstNbl, PreviousNbl);
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);

        if (TargetClassification[PreviousIndex] != NextClassification)
        {
            PreviousNbl->Next = NULL;
            NdisAppendNblChainToNblQueueFast(&Queue[PreviousIndex], FirstNbl, PreviousNbl);

            for (SIZE_T i = 0; i < ARRAYSIZE(Queue); i++)
            {
                if (Valid[i] == FALSE)
                {
                    FirstNbl = Nbl;
                    PreviousIndex = i;
                    Valid[i] = TRUE;
                    TargetClassification[i] = NextClassification;
                    NdisInitializeNblQueue(&Queue[i]);
                    goto Found;
                }

                if (TargetClassification[i] == NextClassification)
                {
                    FirstNbl = Nbl;
                    PreviousIndex = i;
                    goto Found;
                }
            }

            const SIZE_T EvictionIndex = (PreviousIndex + 1) % ARRAYSIZE(Queue);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification[EvictionIndex], &Queue[EvictionIndex]);

            FirstNbl = Nbl;
            PreviousIndex = EvictionIndex;
            TargetClassification[EvictionIndex] = NextClassification;
            NdisInitializeNblQueue(&Queue[EvictionIndex]);
        }

    Found:

        PreviousNbl = Nbl;
    }

    for (SIZE_T i = 0; i < ARRAYSIZE(Queue); i++)
    {
        if (Valid[i] == FALSE)
        {
            break;
        }

#pragma warning(suppress:6387) // 'FlushContext' could be NULL
        FlushCallback(FlushContext, TargetClassification[i], &Queue[i]);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByValueLookaheadWithCount(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _In_ NDIS_NBL_FLUSH_WITH_COUNT_CALLBACK *FlushCallback,
    _In_opt_ PVOID FlushContext)
/*++

Routine Description:

    Similar to NdisClassifyNblChainByValueLookahead, except results are
    provided in an NBL_COUNTED_QUEUE.  Refer to the documentation for
    NdisClassifyNblChainByValueLookahead.

--*/
{
    NBL_COUNTED_QUEUE Queue[NDIS_CLASSIFY_NBL_LOOKHEAD_DEPTH];
    ULONG_PTR TargetClassification[ARRAYSIZE(Queue)];
    SIZE_T Count[ARRAYSIZE(Queue)] = { 1 };

    NET_BUFFER_LIST *FirstNbl = NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;

    SIZE_T PreviousIndex = 0;
    NdisInitializeNblCountedQueue(&Queue[PreviousIndex]);
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    TargetClassification[PreviousIndex] = ClassificationCallback(ClassificationContext, Nbl);

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            NdisAppendNblChainToNblCountedQueueFast(
                &Queue[PreviousIndex], FirstNbl, PreviousNbl, Count[PreviousIndex]);
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);

        if (TargetClassification[PreviousIndex] == NextClassification)
        {
            Count[PreviousIndex] += 1;
        }
        else
        {
            PreviousNbl->Next = NULL;
            NdisAppendNblChainToNblCountedQueueFast(
                  &Queue[PreviousIndex], FirstNbl, PreviousNbl, Count[PreviousIndex]);

            for (SIZE_T i = 0; i < ARRAYSIZE(Queue); i++)
            {
                if (Count[i] == 0)
                {
                    FirstNbl = Nbl;
                    PreviousIndex = i;
                    Count[i] = 1;
                    TargetClassification[i] = NextClassification;
                    NdisInitializeNblCountedQueue(&Queue[i]);
                    goto Found;
                }

                if (TargetClassification[i] == NextClassification)
                {
                    FirstNbl = Nbl;
                    PreviousIndex = i;
                    Count[i] = 1;
                    goto Found;
                }
            }

            const SIZE_T EvictionIndex = (PreviousIndex + 1) % ARRAYSIZE(Queue);
#pragma warning(suppress:6387) // 'FlushContext' could be NULL
            FlushCallback(FlushContext, TargetClassification[EvictionIndex], &Queue[EvictionIndex]);

            FirstNbl = Nbl;
            PreviousIndex = EvictionIndex;
            Count[EvictionIndex] = 1;
            TargetClassification[EvictionIndex] = NextClassification;
            NdisInitializeNblCountedQueue(&Queue[EvictionIndex]);
        }

    Found:

        PreviousNbl = Nbl;
    }

    for (SIZE_T i = 0; i < ARRAYSIZE(Queue); i++)
    {
        if (Count[i] == 0)
        {
            break;
        }

#pragma warning(suppress:6387) // 'FlushContext' could be NULL
        FlushCallback(FlushContext, TargetClassification[i], &Queue[i]);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisPartialClassifyNblChainByValue(
    _Inout_ NET_BUFFER_LIST **NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Out_ NBL_QUEUE *HomogenousQueue,
    _Out_opt_ ULONG_PTR *ClassificationResult)
/*++

Routine Description:

    This routine removes similar NBLs from the front of the chain

    Similarity is defined by a classification callback function that you
    provide.  Two NBLs are considered to be similar if and only if your
    classification callback returns the same integer value for each.

    You'd typically call NdisPartialClassifyNblChainByValue routine repeatedly
    on an input chain, until there's no more NBLs remaining.

Arguments

    NblChain - Address of an NBL chain that contains the input.  On return,
        contains the head of the remaining NBLs

    ClassificationCallback - Callback that returns an integer (or pointer) that
        indicates whether two NBLs should be batched together

    ClassificationContext - Any optional context you'd like to pass to your callback

    HomogenousQueue - On return, contains one or more similar NBLs from the
        input chain

    ClassificationResult - On return, contains the return value of your
        ClassificationCallback for each of the NBLs in HomogenousQueue

--*/
{
    NdisInitializeNblQueue(HomogenousQueue);

    NET_BUFFER_LIST *FirstNbl = *NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ULONG_PTR TargetClassification = ClassificationCallback(ClassificationContext, Nbl);

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            *NblChain = NULL;
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);
        if (NextClassification != TargetClassification)
        {
            PreviousNbl->Next = NULL;
            *NblChain = Nbl;
            break;
        }

        PreviousNbl = Nbl;
    }

    NdisAppendNblChainToNblQueueFast(HomogenousQueue, FirstNbl, PreviousNbl);

    if (ARGUMENT_PRESENT(ClassificationResult))
    {
        *ClassificationResult = TargetClassification;
    }

    NDIS_ASSERT_VALID_NBL_QUEUE(HomogenousQueue);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisPartialClassifyNblChainByValueWithCount(
    _Inout_ NET_BUFFER_LIST **NblChain,
    _In_ NDIS_NBL_CLASSIFICATION_VALUE_CALLBACK *ClassificationCallback,
    _In_opt_ PVOID ClassificationContext,
    _Out_ NBL_COUNTED_QUEUE *HomogenousQueue,
    _Out_opt_ ULONG_PTR *ClassificationResult)
/*++

Routine Description:

    Similar to NdisPartialClassifyNblChainByValue, except results are provided
    in an NBL_COUNTED_QUEUE.  Refer to the documentation for
    NdisPartialClassifyNblChainByValue.

--*/
{
    NdisInitializeNblCountedQueue(HomogenousQueue);

    NET_BUFFER_LIST *FirstNbl = *NblChain;
    NET_BUFFER_LIST *PreviousNbl = FirstNbl;
    NET_BUFFER_LIST *Nbl = FirstNbl;
#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
    ULONG_PTR TargetClassification = ClassificationCallback(ClassificationContext, Nbl);
    SIZE_T Count = 1;

    while (TRUE)
    {
        Nbl = Nbl->Next;
        if (Nbl == NULL)
        {
            *NblChain = NULL;
            break;
        }

        PreFetchCacheLine(PF_TEMPORAL_LEVEL_1, Nbl->Next);

#pragma warning(suppress:6387) // 'ClassificationContext' could be NULL
        ULONG_PTR NextClassification = ClassificationCallback(ClassificationContext, Nbl);
        if (NextClassification != TargetClassification)
        {
            PreviousNbl->Next = NULL;
            *NblChain = Nbl;
            break;
        }

        PreviousNbl = Nbl;
    }

    NdisAppendNblChainToNblCountedQueueFast(HomogenousQueue, FirstNbl, PreviousNbl, Count);

    if (ARGUMENT_PRESENT(ClassificationResult))
    {
        *ClassificationResult = TargetClassification;
    }

    NDIS_ASSERT_VALID_NBL_COUNTED_QUEUE(HomogenousQueue);
}

inline NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK NdisNblClassifierForCancelId;

_Use_decl_annotations_
inline
SIZE_T
NdisNblClassifierForCancelId(PVOID ClassificationContext, NET_BUFFER_LIST *Nbl)
{
    return Nbl->NetBufferListInfo[NetBufferListCancelId] == ClassificationContext ? 1 : 0;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByCancelId(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ PVOID CancelId,
    _Inout_ NBL_QUEUE *KeepQueue,
    _Inout_ NBL_QUEUE *CancelQueue)
/*++

Routine Description:

    Separates out any NBL with a given CancelId

Arguments:

    NblChain - The NBL chain to look through

    CancelId - The CancelId to search for

    KeepQueue - Receives any NBLs that did not match the CancelId

    CancelQueue - Receives any NBLs that matched the CancelId

--*/
{
    NdisClassifyNblChain2(
        NblChain,
        NdisNblClassifierForCancelId,
        CancelId,
        KeepQueue,
        CancelQueue);
}

inline NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK NdisNblClassifierForSourceHandle;

_Use_decl_annotations_
inline
SIZE_T
NdisNblClassifierForSourceHandle(PVOID ClassificationContext, NET_BUFFER_LIST *Nbl)
{
    return Nbl->SourceHandle == ClassificationContext ? 1 : 0;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainBySourceHandle(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_HANDLE MySourceHandle,
    _Inout_ NBL_QUEUE *TheirQueue,
    _Inout_ NBL_QUEUE *MyQueue)
/*++

Routine Description:

    Separates an NBL chain by each NBL's SourceHandle

Arguments:

    NblChain - The NBL chain to look through

    MySourceHandle - The SourceHandle to search for

    TheirQueue - Receives any NBLs that did not match the SourceHandle

    MyQueue - Receives any NBLs that matched the SourceHandle

--*/
{
    NdisClassifyNblChain2(
        NblChain,
        NdisNblClassifierForSourceHandle,
        MySourceHandle,
        TheirQueue,
        MyQueue);
}

inline NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK NdisNblClassifierForPoolHandle;

_Use_decl_annotations_
inline
SIZE_T
NdisNblClassifierForPoolHandle(PVOID ClassificationContext, NET_BUFFER_LIST *Nbl)
{
    return Nbl->NdisPoolHandle == ClassificationContext ? 1 : 0;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
inline
void
NdisClassifyNblChainByPoolHandle(
    _In_ NET_BUFFER_LIST *NblChain,
    _In_ NDIS_HANDLE MyPoolHandle,
    _Inout_ NBL_QUEUE *TheirQueue,
    _Inout_ NBL_QUEUE *MyQueue)
/*++

Routine Description:

    Separates an NBL chain by each NBL's NdisPoolHandle

Arguments:

    NblChain - The NBL chain to look through

    MyPoolHandle - The NdisPoolHandle to search for

    TheirQueue - Receives any NBLs that did not match the NdisPoolHandle

    MyQueue - Receives any NBLs that matched the NdisPoolHandle

--*/
{
    NdisClassifyNblChain2(
        NblChain,
        NdisNblClassifierForPoolHandle,
        MyPoolHandle,
        TheirQueue,
        MyQueue);
}

#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_DESKTOP)
#pragma endregion
