//
//  TPCircularBuffer.c
//  Circular/Ring buffer implementation
//
//  https://github.com/michaeltyson/TPCircularBuffer
//
//  Created by Michael Tyson on 10/12/2011.
//
//  Copyright (C) 2012-2013 A Tasty Pixel
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//
#if defined(__APPLE__) // Mach VM backend; the mmap backend for other platforms follows it
#include "TPCircularBuffer.h"
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

#define reportResult(result,operation) (_reportResult((result),(operation),strrchr(__FILE__, '/')+1,__LINE__))
static inline bool _reportResult(kern_return_t result, const char *operation, const char* file, int line) {
    if ( result != ERR_SUCCESS ) {
        printf("%s:%d: %s: %s\n", file, line, operation, mach_error_string(result)); 
        return false;
    }
    return true;
}

bool _TPCircularBufferInit(TPCircularBuffer *buffer, uint32_t length, size_t structSize) {
    
    assert(length > 0);
    
    if ( structSize != sizeof(TPCircularBuffer) ) {
        fprintf(stderr, "TPCircularBuffer: Header version mismatch. Check for old versions of TPCircularBuffer in your project\n");
        abort();
    }
    
    // Keep trying until we get our buffer, needed to handle race conditions
    int retries = 3;
    while ( true ) {

        buffer->length = (uint32_t)round_page(length);    // We need whole page sizes

        // Temporarily allocate twice the length, so we have the contiguous address space to
        // support a second instance of the buffer directly after
        vm_address_t bufferAddress;
        kern_return_t result = vm_allocate(mach_task_self(),
                                           &bufferAddress,
                                           buffer->length * 2,
                                           VM_FLAGS_ANYWHERE); // allocate anywhere it'll fit
        if ( result != ERR_SUCCESS ) {
            if ( retries-- == 0 ) {
                reportResult(result, "Buffer allocation");
                return false;
            }
            // Try again if we fail
            continue;
        }
        
        // Now replace the second half of the allocation with a virtual copy of the first half. Deallocate the second half...
        result = vm_deallocate(mach_task_self(),
                               bufferAddress + buffer->length,
                               buffer->length);
        if ( result != ERR_SUCCESS ) {
            if ( retries-- == 0 ) {
                reportResult(result, "Buffer deallocation");
                return false;
            }
            // If this fails somehow, deallocate the whole region and try again
            vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
            continue;
        }
        
        // Re-map the buffer to the address space immediately after the buffer
        vm_address_t virtualAddress = bufferAddress + buffer->length;
        vm_prot_t cur_prot, max_prot;
        result = vm_remap(mach_task_self(),
                          &virtualAddress,   // mirror target
                          buffer->length,    // size of mirror
                          0,                 // auto alignment
                          0,                 // force remapping to virtualAddress
                          mach_task_self(),  // same task
                          bufferAddress,     // mirror source
                          0,                 // MAP READ-WRITE, NOT COPY
                          &cur_prot,         // unused protection struct
                          &max_prot,         // unused protection struct
                          VM_INHERIT_DEFAULT);
        if ( result != ERR_SUCCESS ) {
            if ( retries-- == 0 ) {
                reportResult(result, "Remap buffer memory");
                return false;
            }
            // If this remap failed, we hit a race condition, so deallocate and try again
            vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
            continue;
        }
        
        if ( virtualAddress != bufferAddress+buffer->length ) {
            // If the memory is not contiguous, clean up both allocated buffers and try again
            if ( retries-- == 0 ) {
                printf("Couldn't map buffer memory to end of buffer\n");
                return false;
            }

            vm_deallocate(mach_task_self(), virtualAddress, buffer->length);
            vm_deallocate(mach_task_self(), bufferAddress, buffer->length);
            continue;
        }
        
        buffer->buffer = (void*)bufferAddress;
        buffer->fillCount = 0;
        buffer->head = buffer->tail = 0;
        buffer->atomic = true;
        
        return true;
    }
    return false;
}

void TPCircularBufferCleanup(TPCircularBuffer *buffer) {
    vm_deallocate(mach_task_self(), (vm_address_t)buffer->buffer, buffer->length * 2);
    memset(buffer, 0, sizeof(TPCircularBuffer));
}

#else // mmap backend: map one block of shared memory twice, back to back

#include "TPCircularBuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#if defined(__ANDROID__) && __ANDROID_API__ >= 26
#include <android/sharedmem.h> // link with libandroid
#elif defined(__linux__)
#include <sys/syscall.h>
#endif

static size_t roundToPageSize(size_t length) {
    long pageSize = sysconf(_SC_PAGESIZE);
    if ( pageSize <= 0 ) pageSize = 4096;
    return (length + (size_t)pageSize - 1) & ~((size_t)pageSize - 1);
}

// Returns a file descriptor for length bytes of shared memory, or -1 on failure.
static int createSharedMemory(size_t length) {
#if defined(__ANDROID__) && __ANDROID_API__ >= 26
    return ASharedMemory_create("TPCircularBuffer", length);
#elif defined(__linux__) && defined(SYS_memfd_create)
    int fd = (int)syscall(SYS_memfd_create, "TPCircularBuffer", 0);
    if ( fd >= 0 && ftruncate(fd, (off_t)length) != 0 ) {
        close(fd);
        return -1;
    }
    return fd;
#else
    (void)length;
    return -1;
#endif
}

bool _TPCircularBufferInit(TPCircularBuffer *buffer, uint32_t length, size_t structSize) {

    assert(length > 0);

    if ( structSize != sizeof(TPCircularBuffer) ) {
        fprintf(stderr, "TPCircularBuffer: Header version mismatch. Check for old versions of TPCircularBuffer in your project\n");
        abort();
    }

    buffer->length = (uint32_t)roundToPageSize(length);    // We need whole page sizes

    int fd = createSharedMemory(buffer->length);
    if ( fd < 0 ) {
        printf("TPCircularBuffer: couldn't create shared memory\n");
        return false;
    }

    // Reserve twice the length of contiguous address space, then map the same
    // memory into both halves so the buffer is mirrored directly after itself
    char *address = mmap(NULL, (size_t)buffer->length * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    bool mapped = address != MAP_FAILED
        && mmap(address, buffer->length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0) != MAP_FAILED
        && mmap(address + buffer->length, buffer->length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0) != MAP_FAILED;
    close(fd); // the mappings keep the memory alive
    if ( !mapped ) {
        if ( address != MAP_FAILED ) munmap(address, (size_t)buffer->length * 2);
        printf("TPCircularBuffer: couldn't map buffer memory\n");
        return false;
    }

    buffer->buffer = address;
    buffer->fillCount = 0;
    buffer->head = buffer->tail = 0;
    buffer->atomic = true;

    return true;
}

void TPCircularBufferCleanup(TPCircularBuffer *buffer) {
    if ( buffer->buffer ) munmap(buffer->buffer, (size_t)buffer->length * 2);
    memset(buffer, 0, sizeof(TPCircularBuffer));
}

#endif

void TPCircularBufferClear(TPCircularBuffer *buffer) {
    uint32_t fillCount;
    if ( TPCircularBufferTail(buffer, &fillCount) ) {
        TPCircularBufferConsume(buffer, fillCount);
    }
}

void  TPCircularBufferSetAtomic(TPCircularBuffer *buffer, bool atomic) {
    buffer->atomic = atomic;
}
