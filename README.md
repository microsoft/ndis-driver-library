# The NDIS Driver Library (NDL)

[![version](https://img.shields.io/badge/version-1.2.0-blue)](https://github.com/microsoft/ndis-driver-library/releases)
[![vcpkg](https://img.shields.io/vcpkg/v/ndis-driver-library)](https://github.com/microsoft/vcpkg/blob/master/ports/ndis-driver-library/vcpkg.json)

The NDIS Driver Library (NDL) is a header-only library that makes it easier to write NDIS drivers.

Features:

 * **Easy to integrate**: just drop a header into your C or C++ code and you're ready to go
 * **Wide support**: you can use this library in a driver that targets any version of Windows since Windows Vista
 * **Fast**: all these routines are designed for speed, and compete well with their hand-written equivalents
 * **Battle-hardened**: parts of the OS &mdash; including NDIS.SYS itself &mdash; use these headers
 * **Written by the experts**: these headers were written and tested by the OS Networking team at Microsoft

## Example usage

If your LWF injects packets on the send path, you must pluck them back out of the send-complete path.
But cutting apart NBL chains is a tiresome chore &mdash; why not let the library do it for you?

```
#include <ndis.h>
#include <ndis/ndl/nblqueue.h>
#include <ndis/ndl/nblclassify.h>

NDIS_NBL_CLASSIFICATION_INDEX_CALLBACK IsMyNbl;

VOID FilterSendNetBufferListsComplete(NDIS_HANDLE FilterContext, NET_BUFFER_LIST *NblChain, ...)
{
    MY_FILTER *Filter = (MY_FILTER*)FilterContext;

    NBL_QUEUE Mine;
    NBL_QUEUE Theirs;

    NdisInitializeNblQueue(&Mine);
    NdisInitializeNblQueue(&Theirs);

    //
    // Demux the NBL chain into 2 queues, by SourceHandle
    //
    NdisClassifyNblChain2(NblChain, IsMyNbl, Filter, &Theirs, &Mine);

    if (!NdisIsNblQueueEmpty(&Theirs)) {
        NdisFSendNetBufferListsComplete(Filter->NdisHandle, NdisGetNblChainFromNblQueue(&Theirs), ...);
    }

    if (!NdisIsNblQueueEmpty(&Mine)) {
        MyReturnNbls(Filter, NdisGetNblChainFromNblQueue(&Mine));
    }
}

ULONG_PTR IsMyNbl(PVOID Filter, NET_BUFFER_LIST *Nbl)
{
    return (Nbl->SourceHandle == Filter);
}
```

Compare that to what a typical LWF might do, when writing the same algorithm by hand:
```
#include <ndis.h>

VOID FilterSendNetBufferListsComplete(NDIS_HANDLE FilterContext, NET_BUFFER_LIST *NblChain, ...)
{
    MY_FILTER *Filter = (MY_FILTER*)FilterContext;

    NET_BUFFER_LIST *Current = NblChain;
    NET_BUFFER_LIST *TheirFirst = NULL;
    NET_BUFFER_LIST *TheirLast = NULL;
    NET_BUFFER_LIST *MyFirst = NULL;
    NET_BUFFER_LIST *MyLast = NULL;

    while (Current) {
        NET_BUFFER_LIST *Next = Current->Next;

        if (Current->SourceHandle == Filter) {
            //
            // Append to My chain
            //
            if (MyFirst == NULL) {
                MyFirst = Current;
            } else {
                MyLast->Next = Current;
            }
            MyLast = Current;
        } else {
            //
            // Append to Their chain
            //
            if (TheirFirst == NULL) {
                TheirFirst = Current;
            } else {
                TheirLast->Next = Current;
            }
            TheirLast = Current;
        }

        Current = Next;
    }

    if (TheirFirst != NULL) {
        NdisFSendNetBufferListsComplete(pFilter->FilterHandle, TheirFirst, SendCompleteFlags);
    }

    if (MyFirst != NULL) {
        MyReturnNbls(Filter, MyFirst);
    }
}
```
I think you'll agree that the first version is easier to read, and definitely easier to spot any logic bugs in.
Happily, NdisClassifyNblChain2 is even faster than the hand-written code below it, since it uses fewer branches, prefetches list entries, and is careful to avoid redundant writes to memory.
So you don't have to feel guilty about a performance tradeoff here: the simpler code is also the faster code.

## Quick start

All you have to do is place the `src/include` directory into your compiler's include path.
This is a header-only distribution, so there's nothing to compile.

You can consume this library in whatever way is most convenient for you:
* add `ndis-driver-library` to your [vcpkg.json](https://github.com/microsoft/vcpkg) (yes, vcpkg works for drivers too!)
* make this repository a git submodule of your own driver's git repository
* [download](https://github.com/microsoft/ndis-driver-library/releases) the files in a zip file and plop them directly into your code

From there, you only need to #include the header file(s) you want to use.

## Header overview

### `#include <ndis/ndl/nblchain.h>`

[nblchain.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/ndl/nblchain.h) has a few basic helper routines for dealing with a linked list of NBLs.
For example:

 * `NdisNumNblsInNblChain` counts the NBLs in an NBL chain
 * `NdisNumDataBytesInNblChain` counts the number of bytes of packet payload in the NBL chain
 * `NdisSetStatusInNblChain` sets an `NDIS_STATUS` code in each NBL in the chain

You get the idea.
It's really simple stuff &mdash; you could have easily written it yourself.
But now you don't have to.

### `#include <ndis/ndl/nblqueue.h>`

[nblqueue.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/ndl/nblqueue.h) introduces the `NBL_QUEUE`, which is a fancy version of an NBL chain.
While an ordinary NBL chain is just a singly-linked list, the `NBL_QUEUE` tracks the end of the list too, so it supports fast O(1) append.

If you're starting out with an NBL chain, you can convert it into a queue:

```
void ExampleNblQueue(NET_BUFFER_LIST *NblChain)
{
    NBL_QUEUE NblQueue;
    NdisInitializeNblQueue(&NblQueue);

    //
    // Convert the NBL queue into an NBL chain.
    //
    // This operation is O(n), but any future append operations on the queue
    // will be fast O(1).
    //
    NdisAppendNblChainToNblQueue(&NblQueue, NblChain);

    . . . use the NblQueue . . .
}
```

From there, you can efficiently append two NBL queues with `NdisAppendNblQueueToNblQueueFast` or append 1 loose NBL to the queue with `NdisAppendSingleNblToNblQueue`.

The header also offers a variation on the above in `NBL_COUNTED_QUEUE`.
As with an ordinary `NBL_QUEUE`, the `NBL_COUNTED_QUEUE` supports fast O(1) append operations, but it also keeps a running count of the number of NBLs in the queue.
Every time you append, push, or pop from an `NBL_COUNTED_QUEUE`, its count is automatically updated appropriately.
This is useful for the receive path, which needs to carry along a NumberOfNetBufferLists parameter.

## `#include <ndis/ndl/nblclassify.h>`

[nblclassify.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/ndl/nblclassify.h) has routines for demuxing NBL chains.
This means taking a chain of NBLs, and splitting it into 2 or more queues, based on any criterion that you provide.
We used this in our example LWF to pull our own NBLs out of the send-complete path.

Rather than providing one highly-generic classification routine, this library has a family of very similar routines.
This allows each routine to be micro-optimized for performance.
So you only pay for what you actually need.

The simplest classification routine is `NdisClassifyNblChain2`, which splits 1 NBL chain into 2 NBL queues.
You provide a function that takes an NBL and returns 0 or 1 to select which queue it should go into.

If you want to bucket NBLs into *several* queues, use `NdisClassifyNblChainByIndex`.
Your classification function returns a number from 0 to N-1 which selects which queue the NBL should go into.

But sometimes, you can't exactly predict in advance how many queues there will be.
For example, suppose you're classifying packets by destination IP address.
You don't know how many distinct IP addresses appear in the NBL chain &mdash; you just know that if 2 NBLs have different destination IP addresses, they should go into different queues.
This is where `NdisClassifyNblChainByValue` shines: it accumulates similar NBLs, allowing you to process them in batches.

If batching is *really* important to your driver, you can use `NdisClassifyNblChainByValueLookahead`.
This is a drop-in replacement for `NdisClassifyNblChainByValue`, but it tries harder to group similar NBLs together.
For example, if you have a pathological NBL chain of [A, B, A, B, A, B]; then `NdisClassifyNblChainByValue` will not recover any batching; each NBL will be processed separately.
But `NdisClassifyNblChainByValueLookahead` looks ahead enough to rearrange the NBL chain into [A, A, A, B, B, B], so it can process the whole thing in 2 big batches.

Sometimes it's inconvenient to process NBL queues in a callback function.
As an alternative, `NdisPartialClassifyNblChainByValue` allows you to keep your processing logic inline.
It accumulates as many similar NBLs as it can, then returns back to you to let you process them.
Typical usage might look like this:

```
VOID ExamplePartialClassify(NET_BUFFER_LIST *NblChain)
{
    while (NblChain != NULL) {
        NBL_QUEUE Nbls;
        ULONG_PTR IPv4Address;
        NdisPartialClassifyNblChainByValue(&NblChain, GetIPv4Address, NULL, &IPv4Address);

        //
        // Every NBL in the queue has the same IP address, so we can process
        // them all in one batch.
        //

        if (FirewallAllowDestinationIpAddress(IPv4Address)) {
            IndicateNbls(&Nbls);
        } else {
            DropNbls(&Nbls);
        }
    }
}
```

By way of comparison, here's how the same logic would work with `NdisClassifyNblChainByValue`:

```
NDIS_NBL_FLUSH_CALLBACK ProcessNbls;

VOID ExampleCallbackClassify(NET_BUFFER_LIST *NblChain)
{
    NdisClassifyNblChainByValue(&NblChain, GetIPv4Address, NULL, ProcessNbls, NULL);
}

VOID
ProcessNbls(PVOID FlushContext, ULONG_PTR IPv4Address, NBL_QUEUE *Queue)
{
    if (FirewallAllowDestinationIpAddress(IPv4Address)) {
        IndicateNbls(Queue);
    } else {
        DropNbls(Queue);
    }
}
```

`NdisPartialClassifyNblChainByValue` keeps everything in the same function, which is convenient and efficient if you have a lot of local state.
`NdisClassifyNblChainByValue` might be a tiny bit more efficient if the classification routine is expensive, since it avoids redundant classifications.
Use whichever one fits your code the best.

## `#include <ndis/ndl/mdl.h>`

[mdl.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/ndl/mdl.h) has routines for operating on MDL chains.

For example, you can count the number of physical pages that are touched by an MDL chain with `MdlChainGetPageCount`.

You can conveniently zero out a whole MDL chain using `MdlChainZeroBuffers`, or you can zero just a part of an MDL chain with `MdlChainZeroBuffersAtOffset`.

If you would like to save some typing, you can use the `MDL_POINTER` type to refer to a specific byte offset into an MDL chain (think `NET_BUFFER::DataOffset`).
Likewise, you can use `MDL_SPAN` to refer to a specific byte range within an MDL chain (think `NET_BUFFER::DataOffset` combined with `NET_BUFFER::DataLength`).
All the convenience routines in this header also accept MDL pointers and spans.
For example, `MdlSpanZeroBuffers` is the span version of `MdlChainZeroBuffersAtOffset`.

Zeroing buffers is fun, but the real nifty feature is the ability to copy data in or out of a flat buffer, or even copy between two MDL chains.
For example, you can copy 50 bytes from an MDL chain, starting at offset 10:

```
MDL* MdlChain = . . .;
SIZE_T Offset = 10;
UCHAR MyBuffer[50];
MdlCopyMdlChainAtOffsetToFlatBuffer(MyBuffer, MdlChain, Offset, sizeof(MyBuffer));
```

This works efficiently, regardless of how gnarly the MDL chain is.

If you want to do something fancy &mdash; like calculate a checksum &mdash; you might not find a built-in routine to do it.
First, please consider requesting one by filing an [issue](https://github.com/microsoft/ndis-driver-library/issues/new/choose); if it'd be useful to you, it might be useful to others.
But you don't have to wait for us to implement it; you can quickly build your own routines using the low-level MDL iterator routines.

The low-level routine `MdlChainIterateBuffers` invokes a callback for each non-empty buffer in an MDL chain.
And once you have that callback, you can reuse it in `MdlSpanIterateBuffers` to iterate over some subset of the MDL chain.

For example, here's how the buffer zeroing is actually implemented:

```
MDL_BUFFER_OPERATOR ZeroOperator;

_Use_decl_annotations_
NTSTATUS ZeroOperator(PVOID, MDL_SPAN const* Span)
{
    UCHAR* Buffer = MmGetSystemAddressForMdlSafe(Span->Start.Mdl, LowPagePriority);
    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Buffer + Span->Start.Offset, Span->Length);
    return STATUS_SUCCESS;
}

NTSTATUS MdlChainZeroBuffers(MDL* MdlChain)
{
    return MdlChainIterateBuffers(MdlChain, ZeroOperator NULL);
}
```

We implemented the `ZeroOperator` routine that just does its thing on one contiguous buffer at a time, and the iterator figures out where to call it.
The exact same operator can be reused in `MdlSpanIterateBuffers`, so you can zero out subsets of MDL chains without having do to all the offset arithemtic yourself.

## `#include <ndis/ndl/oidrequest.h>`

[oidrequest.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/ndl/oidrequest.h) has routines for operating on OID requests.

If you've ever written a lightweight filter (LWF) driver or intermediate (IM) driver, you've probably noticed that you need to write out some amount of boilerplate to clone OID requests as they pass through your driver.
For example, the ndislwf sample driver has [this switch statement](https://github.com/microsoft/Windows-driver-samples/blob/ea6fd60b5b94b5f556a2317e5fa4b3659d64c9f9/network/ndis/filter/filter.c#L972), and your LWF probably has it too.
At a higher level, everyone needs to have similar mechanics for cloning a request, injecting your own requests, waiting for completion, getting the completion status back out, etc.

Now you can lean on us to write that boilerplate for you. Here's an example of a minimal OID path for a LWF driver:

```
NDIS_STATUS MyFilterOidRequest(
    NDIS_HANDLE filterModuleContext,
    NDIS_OID_REQUEST *oid)
{
    MY_FILTER *filter = (MY_FILTER*)filterModuleContext;
    return NdisFPassthroughOidRequest(filter->ndisHandle, oid, MY_POOLTAG);
}

void MyFilterOidRequestComplete(
    NDIS_HANDLE filterModuleContext,
    NDIS_OID_REQUEST *oid,
    NDIS_STATUS completionStatus)
{
    MY_FILTER *filter = (MY_FILTER*)filterModuleContext;
    NdisFDispatchOidRequestComplete(filter->ndisHandle, oid, completionStatus);
}
```

That's all you need. No switch-case, no copying around BytesWritten or whatever.

When you want to inject your own OID requests, it's as easy as calling `NdisFIssueOidRequestAndWait`.
Our library code will sort out how to wait for completion, so you don't have to worry about asynchronous callbacks.

Although if you *want* asynchronous callbacks, we have you covered: `NdisFIssueOidRequestWithCallback` lets you register a callback for each OID you send, so your driver can be nicely modular.

## `#include <ndis/compat/fileio.h>`

[fileio.h](https://github.com/microsoft/ndis-driver-library/blob/main/src/include/ndis/compat/fileio.h) has a fallback implementation of deprecated NDIS routines.

Historically, NDIS has been a complete driver framework, even including file I/O.
NDIS is deprecating its non-networking features &mdash; we should all use WDF instead.
But as NDIS removes support for old platform features, that means you have to update your driver.
While we would prefer that you update your driver to use the latest WDF routines (e.g. that will help your driver support driver isolation correctly), we also understand that you might not have time to update the driver right now.
As a fallback, you can include this library's compat header to get back some of the routines that were removed from NDIS.

This header adds back in the following NDIS routines:
* `NdisCompatOpenFile`
* `NdisCompatCloseFile`
* `NdisCompatMapFile`
* `NdisCompatUnmapFile`

These work exactly like the classic NDIS-exported routines, but now you can compile them into your driver directly, so you won't be affected as NDIS removes its own support for them.

## Versioning

Current version: 1.2.0

This project uses semantic versioning (semver.org).

We may make any breaking changes to these header files, for example: renaming routines, removing routines, adding/removing parameters, or changing the size of structures.
We commit to two limitations:

 * If make any breaking change, we'll change the major version number.
 * We will not make a breaking change that causes silent bad behavior.  For example, if v1.0 permits a NULL parameter, we wouldn't change v2.0 to crash if given NULL &mdash; we would rename the routine to draw your attention to the change in behavior.

Because of this, we recommend that you take a dependency on a specific version of these header files, and not just build against the rolling HEAD of the repository.

## Contributions

We gladly accept issues here on GitHub.
[File an issue](https://github.com/microsoft/ndis-driver-library/issues/new/choose) if you spot a bug, need clarification on documentation, or have an idea for a new feature that would be a good fit for this library.

For legal reasons, we can only accept pull requests from Microsoft employees.

If you would like to modify your own fork of the NDL, you may wish to automate the generation of the code: refer to the [template directory](https://github.com/microsoft/ndis-driver-library/tree/main/src/template) for information on how to do that.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Security

**Please do not report security vulnerabilities through public GitHub issues.**

Instead, please report them to the Microsoft Security Response Center (MSRC) at [https://msrc.microsoft.com/create-report](https://msrc.microsoft.com/create-report).

If you prefer to submit without logging in, send email to [secure@microsoft.com](mailto:secure@microsoft.com).
If possible, encrypt your message with our PGP key; please download it from the the [Microsoft Security Response Center PGP Key page](https://www.microsoft.com/en-us/msrc/pgp-key-msrc).

You should receive a response within 24 hours.
If for some reason you do not, please follow up via email to ensure we received your original message.
Additional information can be found at [microsoft.com/msrc](https://www.microsoft.com/msrc).

## Legal

**Trademarks** This project may contain trademarks or logos for projects, products, or services.
Authorized use of Microsoft trademarks or logos is subject to and must follow Microsoft's Trademark & Brand Guidelines.
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.

