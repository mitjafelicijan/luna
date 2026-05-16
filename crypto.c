#include "luna.h"

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/coding.h>

static int l_crypto_md5(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	unsigned char hash[WC_MD5_DIGEST_SIZE];
	if (wc_Md5Hash((const byte*)data, (word32)len, hash) != 0) {
		return luaL_error(L, "MD5 hash failed");
	}
	lua_pushlstring(L, (const char*)hash, WC_MD5_DIGEST_SIZE);
	return 1;
}

static int l_crypto_sha1(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	unsigned char hash[WC_SHA_DIGEST_SIZE];
	if (wc_ShaHash((const byte*)data, (word32)len, hash) != 0) {
		return luaL_error(L, "SHA1 hash failed");
	}
	lua_pushlstring(L, (const char*)hash, WC_SHA_DIGEST_SIZE);
	return 1;
}

static int l_crypto_sha256(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	unsigned char hash[WC_SHA256_DIGEST_SIZE];
	if (wc_Sha256Hash((const byte*)data, (word32)len, hash) != 0) {
		return luaL_error(L, "SHA256 hash failed");
	}
	lua_pushlstring(L, (const char*)hash, WC_SHA256_DIGEST_SIZE);
	return 1;
}

static int l_crypto_sha512(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	unsigned char hash[WC_SHA512_DIGEST_SIZE];
	if (wc_Sha512Hash((const byte*)data, (word32)len, hash) != 0) {
		return luaL_error(L, "SHA512 hash failed");
	}
	lua_pushlstring(L, (const char*)hash, WC_SHA512_DIGEST_SIZE);
	return 1;
}

static int l_crypto_hmac(lua_State *L) {
	const char *algo = luaL_checkstring(L, 1);
	size_t key_len, data_len;
	const char *key = luaL_checklstring(L, 2, &key_len);
	const char *data = luaL_checklstring(L, 3, &data_len);

	int type;
	int hash_size;

	if (strcmp(algo, "md5") == 0) {
		type = WC_HASH_TYPE_MD5;
		hash_size = WC_MD5_DIGEST_SIZE;
	} else if (strcmp(algo, "sha1") == 0) {
		type = WC_HASH_TYPE_SHA;
		hash_size = WC_SHA_DIGEST_SIZE;
	} else if (strcmp(algo, "sha256") == 0) {
		type = WC_HASH_TYPE_SHA256;
		hash_size = WC_SHA256_DIGEST_SIZE;
	} else if (strcmp(algo, "sha512") == 0) {
		type = WC_HASH_TYPE_SHA512;
		hash_size = WC_SHA512_DIGEST_SIZE;
	} else {
		return luaL_error(L, "Unsupported HMAC algorithm: %s", algo);
	}

	Hmac hmac;
	unsigned char out[WC_MAX_DIGEST_SIZE];

	if (wc_HmacSetKey(&hmac, type, (const byte*)key, (word32)key_len) != 0) {
		return luaL_error(L, "HMAC set key failed");
	}
	if (wc_HmacUpdate(&hmac, (const byte*)data, (word32)data_len) != 0) {
		return luaL_error(L, "HMAC update failed");
	}
	if (wc_HmacFinal(&hmac, out) != 0) {
		return luaL_error(L, "HMAC final failed");
	}

	lua_pushlstring(L, (const char*)out, hash_size);
	return 1;
}

static int l_crypto_random(lua_State *L) {
	int n = luaL_checkinteger(L, 1);
	if (n <= 0) {
		return 0;
	}

	WC_RNG rng;
	unsigned char *buf = malloc(n);
	if (!buf) {
		return luaL_error(L, "Memory allocation failed");
	}

	if (wc_InitRng(&rng) != 0) {
		free(buf);
		return luaL_error(L, "RNG init failed");
	}

	if (wc_RNG_GenerateBlock(&rng, buf, (word32)n) != 0) {
		wc_FreeRng(&rng);
		free(buf);
		return luaL_error(L, "RNG generation failed");
	}

	wc_FreeRng(&rng);
	lua_pushlstring(L, (const char*)buf, n);
	free(buf);
	return 1;
}

static int l_crypto_base64_encode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	word32 out_len = (word32)(len * 2 + 10); // Sufficient space

	unsigned char *out = malloc(out_len);
	if (!out) {
		return luaL_error(L, "Memory allocation failed");
	}

	if (Base64_Encode_NoNl((const byte*)data, (word32)len, out, &out_len) != 0) {
		free(out);
		return luaL_error(L, "Base64 encode failed");
	}

	lua_pushlstring(L, (const char*)out, out_len);
	free(out);
	return 1;
}

static int l_crypto_base64_decode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	word32 out_len = (word32)len; // Sufficient space

	unsigned char *out = malloc(out_len);
	if (!out) {
		return luaL_error(L, "Memory allocation failed");
	}

	int ret = Base64_Decode((const byte*)data, (word32)len, out, &out_len);
	if (ret != 0) {
		free(out);
		return luaL_error(L, "Base64 decode failed: %d", ret);
	}

	lua_pushlstring(L, (const char*)out, out_len);
	free(out);
	return 1;
}

static int l_crypto_hex_encode(lua_State *L) {
	size_t len;
	const char *data = luaL_checklstring(L, 1, &len);
	char *hex = malloc(len * 2 + 1);
	if (!hex) {
		return luaL_error(L, "Memory allocation failed");
	}

	for (size_t i = 0; i < len; i++) {
		sprintf(hex + i * 2, "%02x", (unsigned char)data[i]);
	}
	lua_pushlstring(L, hex, len * 2);
	free(hex);
	return 1;
}

static int l_crypto_hex_decode(lua_State *L) {
	size_t len;
	const char *hex = luaL_checklstring(L, 1, &len);
	if (len % 2 != 0) {
		return luaL_error(L, "Invalid hex string length");
	}

	size_t out_len = len / 2;
	unsigned char *out = malloc(out_len);
	if (!out) {
		return luaL_error(L, "Memory allocation failed");
	}

	for (size_t i = 0; i < out_len; i++) {
		unsigned int val;
		sscanf(hex + i * 2, "%02x", &val);
		out[i] = (unsigned char)val;
	}

	lua_pushlstring(L, (const char*)out, out_len);
	free(out);
	return 1;
}

static const struct luaL_Reg crypto_lib[] = {
	{"md5", l_crypto_md5},
	{"sha1", l_crypto_sha1},
	{"sha256", l_crypto_sha256},
	{"sha512", l_crypto_sha512},
	{"hmac", l_crypto_hmac},
	{"random", l_crypto_random},
	{"base64_encode", l_crypto_base64_encode},
	{"base64_decode", l_crypto_base64_decode},
	{"hex_encode", l_crypto_hex_encode},
	{"hex_decode", l_crypto_hex_decode},
	{NULL, NULL}
};

int luaopen_crypto(lua_State *L) {
	luaL_newlib(L, crypto_lib);
	return 1;
}
