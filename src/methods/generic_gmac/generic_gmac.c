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


#include "../../fastd.h"
#include "../common.h"


static const fastd_block128_t ZERO_BLOCK = {};

struct fastd_method_session_state {
	fastd_method_common_t common;

	const fastd_cipher_t *cipher;
	const fastd_cipher_context_t *cipher_ctx;
	fastd_cipher_state_t *cipher_state;

	const fastd_cipher_t *gmac_cipher;
	const fastd_cipher_context_t *gmac_cipher_ctx;
	fastd_cipher_state_t *gmac_cipher_state;

	const fastd_mac_t *ghash;
	const fastd_mac_context_t *ghash_ctx;
	fastd_mac_state_t *ghash_state;
};


static bool cipher_get(fastd_context_t *ctx, const char *name, const fastd_cipher_t **cipher, const fastd_cipher_context_t **cctx, const fastd_cipher_t **gmac_cipher, const fastd_cipher_context_t **gmac_cctx) {
	if (!fastd_mac_available("ghash"))
		return false;

	size_t len = strlen(name);

	if (len < 5)
		return false;

	if (strcmp(name+len-5, "-gmac"))
		return false;

	char cipher_name[len];
	memcpy(cipher_name, name, len-4);
	strncpy(cipher_name+len-4, "ctr", 4);

	char *gmac_cipher_name = strchr(cipher_name, '+');

	if (!gmac_cipher_name)
		return false;

	*gmac_cipher_name = 0;
	gmac_cipher_name++;

	if (ctx) {
		*cipher = fastd_cipher_get_by_name(ctx, cipher_name, cctx);
		*gmac_cipher = fastd_cipher_get_by_name(ctx, gmac_cipher_name, gmac_cctx);
		return *cipher && *gmac_cipher;
	}
	else {
		return fastd_cipher_available(cipher_name) && fastd_cipher_available(gmac_cipher_name);
	}
}


static bool method_provides(const char *name) {
	return cipher_get(NULL, name, NULL, NULL, NULL, NULL);
}

static size_t method_max_packet_size(fastd_context_t *ctx) {
	return (fastd_max_packet_size(ctx) + COMMON_HEADBYTES + sizeof(fastd_block128_t));
}


static size_t method_key_length(fastd_context_t *ctx, const char *name) {
	const fastd_cipher_t *cipher = NULL;
	const fastd_cipher_context_t *cctx;

	const fastd_cipher_t *gmac_cipher = NULL;
	const fastd_cipher_context_t *gmac_cctx;

	if (!cipher_get(ctx, name, &cipher, &cctx, &gmac_cipher, &gmac_cctx))
		exit_bug(ctx, "generic-gmac: can't get cipher key length");

	return cipher->key_length + gmac_cipher->key_length;
}

static fastd_method_session_state_t* method_session_init(fastd_context_t *ctx, const char *name, const uint8_t *secret, bool initiator) {
	fastd_method_session_state_t *session = malloc(sizeof(fastd_method_session_state_t));

	fastd_method_common_init(ctx, &session->common, initiator);

	if (!cipher_get(ctx, name, &session->cipher, &session->cipher_ctx, &session->gmac_cipher, &session->gmac_cipher_ctx))
		exit_bug(ctx, "generic-gmac: can't instanciate cipher");

	session->cipher_state = session->cipher->init_state(ctx, session->cipher_ctx, secret);
	if (session->cipher->iv_length && session->cipher->iv_length <= COMMON_NONCEBYTES)
		exit_bug(ctx, "generic-gmac: iv_length to small");

	session->gmac_cipher_state = session->gmac_cipher->init_state(ctx, session->gmac_cipher_ctx, secret + session->cipher->key_length);
	if (session->gmac_cipher->iv_length <= COMMON_NONCEBYTES)
		exit_bug(ctx, "generic-gmac: GMAC cipher iv_length to small");

	fastd_block128_t H;

	uint8_t zeroiv[session->gmac_cipher->iv_length];
	memset(zeroiv, 0, session->gmac_cipher->iv_length);

	session->gmac_cipher->crypt(ctx, session->gmac_cipher_state, &H, &ZERO_BLOCK, sizeof(fastd_block128_t), zeroiv);

	session->ghash = fastd_mac_get_by_name(ctx, "ghash", &session->ghash_ctx);
	if (!session->ghash)
		exit_bug(ctx, "generic-gmac: can't instanciate ghash mac");

	session->ghash_state = session->ghash->init_state(ctx, session->ghash_ctx, H.b);

	return session;
}

static bool method_session_is_valid(fastd_context_t *ctx, fastd_method_session_state_t *session) {
	return (session && fastd_method_session_common_is_valid(ctx, &session->common));
}

static bool method_session_is_initiator(fastd_context_t *ctx UNUSED, fastd_method_session_state_t *session) {
	return fastd_method_session_common_is_initiator(&session->common);
}

static bool method_session_want_refresh(fastd_context_t *ctx, fastd_method_session_state_t *session) {
	return fastd_method_session_common_want_refresh(ctx, &session->common);
}

static void method_session_superseded(fastd_context_t *ctx, fastd_method_session_state_t *session) {
	fastd_method_session_common_superseded(ctx, &session->common);
}

static void method_session_free(fastd_context_t *ctx, fastd_method_session_state_t *session) {
	if (session) {
		session->cipher->free_state(ctx, session->cipher_state);
		session->gmac_cipher->free_state(ctx, session->gmac_cipher_state);
		session->ghash->free_state(ctx, session->ghash_state);

		free(session);
	}
}

static inline void put_size(fastd_block128_t *out, size_t len) {
	memset(out, 0, sizeof(fastd_block128_t));
	out->b[3] = len >> 29;
	out->b[4] = len >> 21;
	out->b[5] = len >> 13;
	out->b[6] = len >> 5;
	out->b[7] = len << 3;
}

static bool method_encrypt(fastd_context_t *ctx, fastd_peer_t *peer UNUSED, fastd_method_session_state_t *session, fastd_buffer_t *out, fastd_buffer_t in) {
	size_t tail_len = alignto(in.len, sizeof(fastd_block128_t))-in.len;
	*out = fastd_buffer_alloc(ctx, sizeof(fastd_block128_t)+in.len, alignto(COMMON_HEADBYTES, 16), sizeof(fastd_block128_t)+tail_len);

	if (tail_len)
		memset(in.data+in.len, 0, tail_len);

	int n_blocks = block_count(in.len, sizeof(fastd_block128_t));

	fastd_block128_t *inblocks = in.data;
	fastd_block128_t *outblocks = out->data;
	fastd_block128_t sig;

	uint8_t gmac_nonce[session->gmac_cipher->iv_length];
	memset(gmac_nonce, 0, session->gmac_cipher->iv_length);
	memcpy(gmac_nonce, session->common.send_nonce, COMMON_NONCEBYTES);
	gmac_nonce[session->gmac_cipher->iv_length-1] = 1;

	bool ok = session->gmac_cipher->crypt(ctx, session->gmac_cipher_state, outblocks, &ZERO_BLOCK, sizeof(fastd_block128_t), gmac_nonce);

	if (ok) {
		uint8_t nonce[session->cipher->iv_length];
		if (session->cipher->iv_length) {
			memset(nonce, 0, session->cipher->iv_length);
			memcpy(nonce, session->common.send_nonce, COMMON_NONCEBYTES);
			nonce[session->cipher->iv_length-1] = 1;
		}

		ok = session->cipher->crypt(ctx, session->cipher_state, outblocks+1, inblocks, n_blocks*sizeof(fastd_block128_t), nonce);
	}

	if (ok) {
		if (tail_len)
			memset(out->data+out->len, 0, tail_len);

		put_size(&outblocks[n_blocks+1], in.len);

		ok = session->ghash->hash(ctx, session->ghash_state, &sig, outblocks+1, n_blocks+1);
	}

	if (!ok) {
		fastd_buffer_free(*out);
		return false;
	}

	xor_a(&outblocks[0], &sig);

	fastd_buffer_free(in);

	fastd_buffer_pull_head(ctx, out, COMMON_HEADBYTES);

	memcpy(out->data, session->common.send_nonce, COMMON_NONCEBYTES);
	fastd_method_increment_nonce(&session->common);

	((uint8_t*)out->data)[COMMON_NONCEBYTES] = 0; /* flags */

	return true;
}

static bool method_decrypt(fastd_context_t *ctx, fastd_peer_t *peer, fastd_method_session_state_t *session, fastd_buffer_t *out, fastd_buffer_t in) {
	if (in.len < COMMON_HEADBYTES+sizeof(fastd_block128_t))
		return false;

	if (!method_session_is_valid(ctx, session))
		return false;

	const uint8_t *common_nonce = in.data;

	if (common_nonce[COMMON_NONCEBYTES]) /* flags */
		return false;

	int64_t age;
	if (!fastd_method_is_nonce_valid(ctx, &session->common, common_nonce, &age))
		return false;

	uint8_t gmac_nonce[session->gmac_cipher->iv_length];
	memset(gmac_nonce, 0, session->gmac_cipher->iv_length);
	memcpy(gmac_nonce, common_nonce, COMMON_NONCEBYTES);
	gmac_nonce[session->gmac_cipher->iv_length-1] = 1;

	uint8_t nonce[session->cipher->iv_length];
	if (session->cipher->iv_length) {
		memset(nonce, 0, session->cipher->iv_length);
		memcpy(nonce, common_nonce, COMMON_NONCEBYTES);
		nonce[session->cipher->iv_length-1] = 1;
	}

	fastd_buffer_push_head(ctx, &in, COMMON_HEADBYTES);

	size_t tail_len = alignto(in.len, sizeof(fastd_block128_t))-in.len;
	*out = fastd_buffer_alloc(ctx, in.len, 0, tail_len);

	int n_blocks = block_count(in.len, sizeof(fastd_block128_t));

	fastd_block128_t *inblocks = in.data;
	fastd_block128_t *outblocks = out->data;
	fastd_block128_t sig;

	bool ok = session->gmac_cipher->crypt(ctx, session->gmac_cipher_state, outblocks, inblocks, sizeof(fastd_block128_t), gmac_nonce);

	if (ok)
		ok = session->cipher->crypt(ctx, session->cipher_state, outblocks+1, inblocks+1, (n_blocks-1)*sizeof(fastd_block128_t), nonce);

	if (ok) {
		if (tail_len)
			memset(in.data+in.len, 0, tail_len);

		put_size(&inblocks[n_blocks], in.len-sizeof(fastd_block128_t));

		ok = session->ghash->hash(ctx, session->ghash_state, &sig, inblocks+1, n_blocks);
	}

	if (!ok || memcmp(&sig, &outblocks[0], sizeof(fastd_block128_t)) != 0) {
		fastd_buffer_free(*out);
		return false;
	}

	fastd_buffer_push_head(ctx, out, sizeof(fastd_block128_t));

	if (!fastd_method_reorder_check(ctx, peer, &session->common, common_nonce, age)) {
		fastd_buffer_free(*out);
		*out = fastd_buffer_alloc(ctx, 0, 0, 0);
	}

	fastd_buffer_free(in);

	return true;
}

const fastd_method_t fastd_method_generic_gmac = {
	.provides = method_provides,

	.max_packet_size = method_max_packet_size,
	.min_encrypt_head_space = 0,
	.min_decrypt_head_space = 0,
	.min_encrypt_tail_space = sizeof(fastd_block128_t)-1,
	.min_decrypt_tail_space = 2*sizeof(fastd_block128_t)-1,

	.key_length = method_key_length,
	.session_init = method_session_init,
	.session_is_valid = method_session_is_valid,
	.session_is_initiator = method_session_is_initiator,
	.session_want_refresh = method_session_want_refresh,
	.session_superseded = method_session_superseded,
	.session_free = method_session_free,

	.encrypt = method_encrypt,
	.decrypt = method_decrypt,
};
