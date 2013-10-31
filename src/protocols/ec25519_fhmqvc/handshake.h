/*
  Copyright (c) 2012-2013, Matthias Schiffer <mschiffer@universe-factory.net>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef _FASTD_PROTOCOL_EC25519_FHMQVC_HANDSHAKE_H_
#define _FASTD_PROTOCOL_EC25519_FHMQVC_HANDSHAKE_H_

#include "ec25519_fhmqvc.h"


typedef struct handshake_key {
	uint64_t serial;
	struct timespec preferred_till;
	struct timespec valid_till;

	/* keypair used as initiator */
	keypair_t key1;

	/* keypair used as responder */
	keypair_t key2;
} handshake_key_t;

struct fastd_protocol_state {
	handshake_key_t prev_handshake_key;
	handshake_key_t handshake_key;
};


static inline bool is_handshake_key_valid(fastd_context_t *ctx, const handshake_key_t *handshake_key) {
	return timespec_after(&handshake_key->valid_till, &ctx->now);
}

static inline bool is_handshake_key_preferred(fastd_context_t *ctx, const handshake_key_t *handshake_key) {
	return timespec_after(&handshake_key->preferred_till, &ctx->now);
}

#endif /* _FASTD_PROTOCOL_EC25519_FHMQVC_HANDSHAKE_H_ */