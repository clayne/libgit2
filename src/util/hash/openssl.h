/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_hash_openssl_h__
#define INCLUDE_hash_openssl_h__

#include "hash/sha.h"

#if defined(GIT_SHA1_OPENSSL_FIPS) || defined(GIT_SHA256_OPENSSL_FIPS)
# include <openssl/evp.h>
#endif

#if defined(GIT_SHA1_OPENSSL) || defined(GIT_SHA256_OPENSSL)
# include <openssl/sha.h>
#endif

#if defined(GIT_SHA1_OPENSSL_DYNAMIC)
typedef struct {
	unsigned int h0, h1, h2, h3, h4;
	unsigned int Nl, Nh;
	unsigned int data[16];
	unsigned int num;
} SHA_CTX;
#endif

#if defined(GIT_SHA256_OPENSSL_DYNAMIC)
typedef struct {
	unsigned int h[8];
	unsigned int Nl, Nh;
	unsigned int data[16];
	unsigned int num, md_len;
} SHA256_CTX;
#endif

#if defined(GIT_SHA1_OPENSSL) || defined(GIT_SHA1_OPENSSL_DYNAMIC)
struct git_hash_sha1_ctx {
	SHA_CTX c;
};
#endif

#ifdef GIT_SHA1_OPENSSL_FIPS
struct git_hash_sha1_ctx {
	EVP_MD_CTX* c;
};
#endif

#if defined(GIT_SHA256_OPENSSL) || defined(GIT_SHA256_OPENSSL_DYNAMIC)
struct git_hash_sha256_ctx {
	SHA256_CTX c;
};
#endif

#ifdef GIT_SHA256_OPENSSL_FIPS
struct git_hash_sha256_ctx {
	EVP_MD_CTX* c;
};
#endif

#endif
