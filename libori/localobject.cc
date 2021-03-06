/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define __STDC_LIMIT_MACROS

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/sha.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include "tuneables.h"

#include <oriutil/debug.h>
#include <ori/object.h>
#include <ori/localobject.h>

using namespace std;

/*
 * Object
 */
LocalObject::LocalObject(PfTransaction::sp transaction, size_t ix)
    : Object(transaction->infos[ix]), transaction(transaction), ix_tr(ix),
      packfile()
{
}

LocalObject::LocalObject(Packfile::sp packfile, const IndexEntry &entry)
    : Object(entry.info), packfile(packfile), entry(entry)
{
}

LocalObject::~LocalObject()
{
}

// XXX: Eliminate duplicate compression code!
bytestream *LocalObject::getPayloadStream() {
    if (packfile.get()) {
        return packfile->getPayload(entry);
    }
    if (transaction.get()) {
        switch(info.getAlgo()) {
            case ObjectInfo::ZIPALGO_NONE:
                return new strstream(transaction->payloads[ix_tr]);
            case ObjectInfo::ZIPALGO_FASTLZ:
                return new zipstream(new strstream(transaction->payloads[ix_tr]),
                                     DECOMPRESS, info.payload_size);
            case ObjectInfo::ZIPALGO_LZMA:
            case ObjectInfo::ZIPALGO_UNKNOWN:
                NOT_IMPLEMENTED(false);
        }
    }
    return NULL;
}

/*
 * Static methods
 */



/*
 * Private methods
 */

