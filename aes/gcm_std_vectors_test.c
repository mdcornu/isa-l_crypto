/**********************************************************************
  Copyright(c) 2011-2016 Intel Corporation All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>		// for memcmp
#include <malloc.h>		// for memalign() and _aligned_malloc() & aligned_free()
#include <aes_gcm.h>
#include "gcm_vectors.h"
#include "types.h"
#include "gcm_test_alloc.h"

#ifndef TEST_SEED
# define TEST_SEED 0x1234
#endif

int check_data(uint8_t * test, uint8_t * expected, uint64_t len, char *data_name)
{
	int mismatch;
	int OK = 0;

	mismatch = memcmp(test, expected, len);
	if (mismatch) {
		OK = 1;
		printf("  expected results don't match %s \t\t", data_name);
		{
			uint64_t a;
			for (a = 0; a < len; a++) {
				if (test[a] != expected[a]) {
					printf(" '%x' != '%x' at %lx of %lx\n",
					       test[a], expected[a], a, len);
					break;
				}
			}
		}
	}
	return OK;
}

int test_gcm128_std_vectors(gcm_vector const *vector)
{
	struct gcm_key_data gkey;
	struct gcm_context_data gctx;
	int OK = 0;
	// Temporary array for the calculated vectors
	uint8_t *ct_test = NULL;
	uint8_t *pt_test = NULL;
	uint8_t *IV_c = NULL;
	uint8_t *T_test = NULL;
	uint8_t *T2_test = NULL;

	// Allocate required memory
	void **alloc_tab[] = {
		(void **)&pt_test, (void **)&ct_test, (void **)&IV_c, (void **)&T_test,
		(void **)&T2_test
	};
	const size_t length_tab[] = {
		vector->Plen, vector->Plen, vector->IVlen, vector->Tlen, vector->Tlen
	};

	if (vector_allocate(alloc_tab, length_tab, NULL, DIM(alloc_tab)) != 0) {
		vector_free(alloc_tab, NULL, DIM(alloc_tab));
		return 1;
	}

	memcpy(IV_c, vector->IV, vector->IVlen);

	// This is only required once for a given key
	aes_gcm_pre_128(vector->K, &gkey);

	////
	// ISA-l Encrypt
	////
	aes_gcm_enc_128(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->C, vector->Plen, "ISA-L encrypted cypher text (C)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L tag (T)");

	// test of in-place encrypt
	memcpy(pt_test, vector->P, vector->Plen);
	aes_gcm_enc_128(&gkey, &gctx, pt_test, pt_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->C, vector->Plen,
			 "ISA-L encrypted cypher text(in-place)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L encrypted tag T(in-place)");
	memset(ct_test, 0, vector->Plen);
	memset(T_test, 0, vector->Tlen);

	////
	// ISA-l Decrypt
	////
	aes_gcm_dec_128(&gkey, &gctx, pt_test, vector->C, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->P, vector->Plen, "ISA-L decrypted plain text (P)");
	// GCM decryption outputs a 16 byte tag value that must be verified against the expected tag value
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T)");

	// test in in-place decrypt
	memcpy(ct_test, vector->C, vector->Plen);
	aes_gcm_dec_128(&gkey, &gctx, ct_test, ct_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->P, vector->Plen, "ISA-L plain text (P) - in-place");
	OK |=
	    check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T) - in-place");
	// ISA-L enc -> ISA-L dec
	aes_gcm_enc_128(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	memset(pt_test, 0, vector->Plen);
	aes_gcm_dec_128(&gkey, &gctx, pt_test, ct_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T2_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->P, vector->Plen,
		       "ISA-L self decrypted plain text (P)");
	OK |= check_data(T_test, T2_test, vector->Tlen, "ISA-L self decrypted tag (T)");

	vector_free(alloc_tab, NULL, DIM(alloc_tab));
	return OK;
}

int test_gcm256_std_vectors(gcm_vector const *vector)
{
	struct gcm_key_data gkey;
	struct gcm_context_data gctx;
	int OK = 0;
	// Temporary array for the calculated vectors
	uint8_t *ct_test = NULL;
	uint8_t *pt_test = NULL;
	uint8_t *IV_c = NULL;
	uint8_t *T_test = NULL;
	uint8_t *T2_test = NULL;

	// Allocate required memory
	void **alloc_tab[] = {
		(void **)&pt_test, (void **)&ct_test, (void **)&IV_c, (void **)&T_test,
		(void **)&T2_test
	};
	const size_t length_tab[] = {
		vector->Plen, vector->Plen, vector->IVlen, vector->Tlen, vector->Tlen
	};

	if (vector_allocate(alloc_tab, length_tab, NULL, DIM(alloc_tab)) != 0) {
		vector_free(alloc_tab, NULL, DIM(alloc_tab));
		return 1;
	}

	memcpy(IV_c, vector->IV, vector->IVlen);

	// This is only required once for a given key
	aes_gcm_pre_256(vector->K, &gkey);

	////
	// ISA-l Encrypt
	////
	memset(ct_test, 0, vector->Plen);
	aes_gcm_enc_256(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->C, vector->Plen, "ISA-L encrypted cypher text (C)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L tag (T)");

	// test of in-place encrypt
	memcpy(pt_test, vector->P, vector->Plen);
	aes_gcm_enc_256(&gkey, &gctx, pt_test, pt_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->C, vector->Plen,
		       "ISA-L encrypted cypher text(in-place)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L encrypted tag T(in-place)");
	memset(ct_test, 0, vector->Plen);
	memset(T_test, 0, vector->Tlen);

	////
	// ISA-l Decrypt
	////
	aes_gcm_dec_256(&gkey, &gctx, pt_test, vector->C, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->P, vector->Plen, "ISA-L decrypted plain text (P)");
	// GCM decryption outputs a 16 byte tag value that must be verified against the expected tag value
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T)");

	// test in in-place decrypt
	memcpy(ct_test, vector->C, vector->Plen);
	aes_gcm_dec_256(&gkey, &gctx, ct_test, ct_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->P, vector->Plen, "ISA-L plain text (P) - in-place");
	OK |=
	    check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T) - in-place");
	// ISA-L enc -> ISA-L dec
	aes_gcm_enc_256(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	memset(pt_test, 0, vector->Plen);
	aes_gcm_dec_256(&gkey, &gctx, pt_test, ct_test, vector->Plen, IV_c,
			vector->A, vector->Alen, T2_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->P, vector->Plen,
		       "ISA-L self decrypted plain text (P)");
	OK |= check_data(T_test, T2_test, vector->Tlen, "ISA-L self decrypted tag (T)");

	vector_free(alloc_tab, NULL, DIM(alloc_tab));
	return OK;
}

void aes_gcm_stream_enc_128(const struct gcm_key_data *key_data,
			    struct gcm_context_data *context,
			    uint8_t * out,
			    uint8_t const *in,
			    uint64_t len,
			    uint8_t * iv,
			    uint8_t const *aad,
			    uint64_t aad_len, uint8_t * auth_tag, uint64_t auth_tag_len)
{
	aes_gcm_init_128(key_data, context, iv, aad, aad_len);
	uint8_t test_sequence[] = { 1, 12, 22, 0, 1, 12, 16 };	//sum(test_sequence) > max_Plen in vectors
	uint32_t i;
	uint32_t offset = 0, dist;

	for (i = 0; i < sizeof(test_sequence); i++) {
		dist = test_sequence[i];
		if (offset + dist > len)
			break;
		aes_gcm_enc_128_update(key_data, context, out + offset, in + offset, dist);
		offset += dist;
	}

	aes_gcm_enc_128_update(key_data, context, out + offset, in + offset, len - offset);
	aes_gcm_enc_128_finalize(key_data, context, auth_tag, auth_tag_len);
}

void aes_gcm_stream_dec_128(const struct gcm_key_data *key_data,
			    struct gcm_context_data *context,
			    uint8_t * out,
			    uint8_t const *in,
			    uint64_t len,
			    uint8_t * iv,
			    uint8_t const *aad,
			    uint64_t aad_len, uint8_t * auth_tag, uint64_t auth_tag_len)
{
	aes_gcm_init_128(key_data, context, iv, aad, aad_len);
	uint8_t test_sequence[] = { 1, 12, 22, 0, 1, 12, 16 };	//sum(test_sequence) > max_Plen in vectors
	uint32_t i;
	uint32_t offset = 0, dist;

	for (i = 0; i < sizeof(test_sequence); i++) {
		dist = test_sequence[i];
		if (offset + dist > len)
			break;
		aes_gcm_dec_128_update(key_data, context, out + offset, in + offset, dist);
		offset += dist;
	}
	aes_gcm_dec_128_update(key_data, context, out + offset, in + offset, len - offset);
	aes_gcm_dec_128_finalize(key_data, context, auth_tag, auth_tag_len);

}

#if !defined(NT_LD) && !defined(NT_ST) && !defined(NT_LDST)
int test_gcm128_std_stream_vectors(gcm_vector const *vector)
{
	struct gcm_key_data gkey;
	struct gcm_context_data gctx;
	int OK = 0;
	// Temporary array for the calculated vectors
	uint8_t *ct_test = NULL;
	uint8_t *pt_test = NULL;
	uint8_t *IV_c = NULL;
	uint8_t *T_test = NULL;
	uint8_t *T2_test = NULL;

	// Allocate required memory
	void **alloc_tab[] = {
		(void **)&pt_test, (void **)&ct_test, (void **)&IV_c, (void **)&T_test,
		(void **)&T2_test
	};
	const size_t length_tab[] = {
		vector->Plen, vector->Plen, vector->IVlen, vector->Tlen, vector->Tlen
	};

	if (vector_allocate(alloc_tab, length_tab, NULL, DIM(alloc_tab)) != 0) {
		vector_free(alloc_tab, NULL, DIM(alloc_tab));
		return 1;
	}

	memcpy(IV_c, vector->IV, vector->IVlen);

	// This is only required once for a given key
	memset(gkey.expanded_keys, 0, sizeof(gkey.expanded_keys));
	aes_gcm_pre_128(vector->K, &gkey);

	////
	// ISA-l Encrypt
	////

	aes_gcm_stream_enc_128(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->C, vector->Plen, "ISA-L encrypted cypher text (C)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L tag (T)");

	// test of in-place encrypt
	memcpy(pt_test, vector->P, vector->Plen);
	aes_gcm_stream_enc_128(&gkey, &gctx, pt_test, pt_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->C, vector->Plen,
			 "ISA-L encrypted cypher text(in-place)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L encrypted tag T(in-place)");
	memset(ct_test, 0, vector->Plen);
	memset(T_test, 0, vector->Tlen);

	////
	// ISA-l Decrypt
	////
	aes_gcm_stream_dec_128(&gkey, &gctx, pt_test, vector->C, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->P, vector->Plen, "ISA-L decrypted plain text (P)");
	// GCM decryption outputs a 16 byte tag value that must be verified against the expected tag value
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T)");

	// test in in-place decrypt
	memcpy(ct_test, vector->C, vector->Plen);
	aes_gcm_stream_dec_128(&gkey, &gctx, ct_test, ct_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->P, vector->Plen, "ISA-L plain text (P) - in-place");
	OK |=
	    check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T) - in-place");
	// ISA-L enc -> ISA-L dec
	aes_gcm_stream_enc_128(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	memset(pt_test, 0, vector->Plen);
	aes_gcm_stream_dec_128(&gkey, &gctx, pt_test, ct_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T2_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->P, vector->Plen,
		       "ISA-L self decrypted plain text (P)");
	OK |= check_data(T_test, T2_test, vector->Tlen, "ISA-L self decrypted tag (T)");

	vector_free(alloc_tab, NULL, DIM(alloc_tab));
	return OK;
}

void aes_gcm_stream_enc_256(const struct gcm_key_data *key_data,
			    struct gcm_context_data *context,
			    uint8_t * out,
			    uint8_t const *in,
			    uint64_t len,
			    uint8_t * iv,
			    uint8_t const *aad,
			    uint64_t aad_len, uint8_t * auth_tag, uint64_t auth_tag_len)
{
	aes_gcm_init_256(key_data, context, iv, aad, aad_len);
	uint8_t test_sequence[] = { 1, 12, 22, 0, 1, 12, 16 };	//sum(test_sequence) > max_Plen in vectors
	uint32_t i;
	uint32_t offset = 0, dist;

	for (i = 0; i < sizeof(test_sequence); i++) {
		dist = test_sequence[i];
		if (offset + dist > len)
			break;
		aes_gcm_enc_256_update(key_data, context, out + offset, in + offset, dist);
		offset += dist;
	}

	aes_gcm_enc_256_update(key_data, context, out + offset, in + offset, len - offset);
	aes_gcm_enc_256_finalize(key_data, context, auth_tag, auth_tag_len);

}

void aes_gcm_stream_dec_256(const struct gcm_key_data *key_data,
			    struct gcm_context_data *context,
			    uint8_t * out,
			    uint8_t const *in,
			    uint64_t len,
			    uint8_t * iv,
			    uint8_t const *aad,
			    uint64_t aad_len, uint8_t * auth_tag, uint64_t auth_tag_len)
{
	aes_gcm_init_256(key_data, context, iv, aad, aad_len);
	uint8_t test_sequence[] = { 1, 12, 22, 0, 1, 12, 16 };	//sum(test_sequence) > max_Plen in vectors
	uint32_t i;
	uint32_t offset = 0, dist;

	for (i = 0; i < sizeof(test_sequence); i++) {
		dist = test_sequence[i];
		if (offset + dist > len)
			break;
		aes_gcm_dec_256_update(key_data, context, out + offset, in + offset, dist);
		offset += dist;
	}

	aes_gcm_dec_256_update(key_data, context, out + offset, in + offset, len - offset);
	aes_gcm_dec_256_finalize(key_data, context, auth_tag, auth_tag_len);

}

int test_gcm256_std_stream_vectors(gcm_vector const *vector)
{
	struct gcm_key_data gkey;
	struct gcm_context_data gctx;
	int OK = 0;
	// Temporary array for the calculated vectors
	uint8_t *ct_test = NULL;
	uint8_t *pt_test = NULL;
	uint8_t *IV_c = NULL;
	uint8_t *T_test = NULL;
	uint8_t *T2_test = NULL;

	// Allocate required memory
	void **alloc_tab[] = {
		(void **)&pt_test, (void **)&ct_test, (void **)&IV_c, (void **)&T_test,
		(void **)&T2_test
	};
	const size_t length_tab[] = {
		vector->Plen, vector->Plen, vector->IVlen, vector->Tlen, vector->Tlen
	};

	if (vector_allocate(alloc_tab, length_tab, NULL, DIM(alloc_tab)) != 0) {
		vector_free(alloc_tab, NULL, DIM(alloc_tab));
		return 1;
	}

	memcpy(IV_c, vector->IV, vector->IVlen);

	// This is only required once for a given key
	aes_gcm_pre_256(vector->K, &gkey);

	////
	// ISA-l Encrypt
	////
	memset(ct_test, 0, vector->Plen);
	aes_gcm_stream_enc_256(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->C, vector->Plen, "ISA-L encrypted cypher text (C)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L tag (T)");

	// test of in-place encrypt
	memcpy(pt_test, vector->P, vector->Plen);
	aes_gcm_stream_enc_256(&gkey, &gctx, pt_test, pt_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->C, vector->Plen,
		       "ISA-L encrypted cypher text(in-place)");
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L encrypted tag T(in-place)");
	memset(ct_test, 0, vector->Plen);
	memset(T_test, 0, vector->Tlen);

	////
	// ISA-l Decrypt
	////
	aes_gcm_stream_dec_256(&gkey, &gctx, pt_test, vector->C, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(pt_test, vector->P, vector->Plen, "ISA-L decrypted plain text (P)");
	// GCM decryption outputs a 16 byte tag value that must be verified against the expected tag value
	OK |= check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T)");

	// test in in-place decrypt
	memcpy(ct_test, vector->C, vector->Plen);
	aes_gcm_stream_dec_256(&gkey, &gctx, ct_test, ct_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T_test, vector->Tlen);
	OK |= check_data(ct_test, vector->P, vector->Plen, "ISA-L plain text (P) - in-place");
	OK |=
	    check_data(T_test, vector->T, vector->Tlen, "ISA-L decrypted tag (T) - in-place");
	// ISA-L enc -> ISA-L dec
	aes_gcm_stream_enc_256(&gkey, &gctx, ct_test, vector->P, vector->Plen,
			       IV_c, vector->A, vector->Alen, T_test, vector->Tlen);
	memset(pt_test, 0, vector->Plen);
	aes_gcm_stream_dec_256(&gkey, &gctx, pt_test, ct_test, vector->Plen, IV_c,
			       vector->A, vector->Alen, T2_test, vector->Tlen);
	OK |=
	    check_data(pt_test, vector->P, vector->Plen,
		       "ISA-L self decrypted plain text (P)");
	OK |= check_data(T_test, T2_test, vector->Tlen, "ISA-L self decrypted tag (T)");

	vector_free(alloc_tab, NULL, DIM(alloc_tab));
	return OK;
}
#endif

int test_gcm_std_vectors(void)
{
	int const vectors_cnt = sizeof(gcm_vectors) / sizeof(gcm_vectors[0]);
	int vect;
	int OK = 0;

	printf("AES-GCM standard test vectors new api:\n");
	for (vect = 0; (vect < vectors_cnt); vect++) {
#ifdef DEBUG
		printf("Standard vector new api %d/%d"
		       "  Keylen:%d IVlen:%d PTLen:%d AADlen:%d Tlen:%d\n",
		       vect, vectors_cnt - 1, (int)gcm_vectors[vect].Klen,
		       (int)gcm_vectors[vect].IVlen, (int)gcm_vectors[vect].Plen,
		       (int)gcm_vectors[vect].Alen, (int)gcm_vectors[vect].Tlen);
#else
		printf(".");
#endif
		if (BITS_128 == gcm_vectors[vect].Klen)
			OK |= test_gcm128_std_vectors(&gcm_vectors[vect]);
		else
			OK |= test_gcm256_std_vectors(&gcm_vectors[vect]);
		if (0 != OK)
			return OK;
	}
	printf("\n");
	return OK;
}

#if !defined(NT_LD) && !defined(NT_ST) && !defined(NT_LDST)
/**
 * Stream API test with standard vectors
 */
int test_gcm_std_strm_vectors(void)
{
	int const vectors_cnt = sizeof(gcm_vectors) / sizeof(gcm_vectors[0]);
	int vect;
	int OK = 0;

	printf("AES-GCM standard test vectors stream api:\n");
	for (vect = 0; (vect < vectors_cnt); vect++) {
#ifdef DEBUG
		printf("Standard vector stream api %d/%d"
		       "  Keylen:%d IVlen:%d PTLen:%d AADlen:%d Tlen:%d\n",
		       vect, vectors_cnt - 1, (int)gcm_vectors[vect].Klen,
		       (int)gcm_vectors[vect].IVlen, (int)gcm_vectors[vect].Plen,
		       (int)gcm_vectors[vect].Alen, (int)gcm_vectors[vect].Tlen);
#else
		printf(".");
#endif
		if (BITS_128 == gcm_vectors[vect].Klen)
			OK |= test_gcm128_std_stream_vectors(&gcm_vectors[vect]);
		else
			OK |= test_gcm256_std_stream_vectors(&gcm_vectors[vect]);
		if (0 != OK)
			return OK;
	}
	printf("\n");
	return OK;
}
#endif
int main(int argc, char **argv)
{
	int errors = 0;
	int seed;

	if (argc == 1)
		seed = TEST_SEED;
	else
		seed = atoi(argv[1]);

	srand(seed);
	printf("SEED: %d\n", seed);

	errors += test_gcm_std_vectors();
#if !defined(NT_LD) && !defined(NT_ST) && !defined(NT_LDST)
	errors += test_gcm_std_strm_vectors();
#endif

	if (0 == errors)
		printf("...Pass\n");
	else
		printf("...Fail\n");

	return errors;
}
