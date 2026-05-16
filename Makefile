CC      = zig cc -target x86_64-linux-musl
CFLAGS  = -Wall -O2 -Iluajit/src -Ilibev -Icjson -Icurl/include -Iwolfssl -Iwolfssl/build -Isqlite -Ijinjac/libjinjac/include -Ijinjac/libjinjac/src -Iiniparser/src -DCURL_STATICLIB
LIBS    = luajit/src/libluajit.a libev/.libs/libev.a curl/build/lib/libcurl.a wolfssl/build/libwolfssl.a jinjac/build/libjinjac/src/liblibjinjac_static.a iniparser/build/libiniparser.a -lm -ldl -lpthread -lunwind

SRCS    = core.c json.c http.c timer.c util.c log.c assert.c request.c env.c crypto.c sqlite.c fs.c process.c template.c ini.c path.c stash.c sqlite/sqlite3.c cjson/cJSON.c
OBJS    = $(SRCS:.c=.o)

MEX_ASSURE = zig make cmake flex bison
MEX_DESCRIPTION = "Lightweight Lua runtime powered by LuaJIT and libev"

include makext.mk
help: .help

build: .assure luna # Statically compiles luna binary

luna: $(OBJS)
	$(CC) $(CFLAGS) -static -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

requirements: .assure # Compiles all required libraries
	cd luajit/src && \
		make clean && \
		make -j`nproc` CC="zig cc -target x86_64-linux-musl" HOST_CC="gcc" BUILDMODE=static TARGET_LDFLAGS="-lunwind"
	cd libev && \
		make clean && \
		./configure CC="zig cc -target x86_64-linux-musl" --host=x86_64-linux-musl --enable-static --disable-shared && \
		make -j`nproc`
	mkdir -p wolfssl/build && cd wolfssl/build && \
		rm -rf * && \
		CC="zig cc -target x86_64-linux-musl" cmake .. \
			-DCMAKE_BUILD_TYPE=Release \
			-DBUILD_SHARED_LIBS=OFF \
			-DWOLFSSL_CURL=yes \
			-DWOLFSSL_EXAMPLES=no \
			-DWOLFSSL_CRYPT_TESTS=no && \
		make -j`nproc`
	mkdir -p curl/build && cd curl/build && \
		rm -rf * && \
		CC="zig cc -target x86_64-linux-musl" cmake .. \
			-DCMAKE_BUILD_TYPE=Release \
			-DBUILD_SHARED_LIBS=OFF \
			-DBUILD_CURL_EXE=OFF \
			-DBUILD_TESTING=OFF \
			-DCURL_USE_LIBPSL=OFF \
			-DCURL_USE_LIBSSH2=OFF \
			-DCURL_USE_GSSAPI=OFF \
			-DCURL_USE_OPENSSL=OFF \
			-DCURL_USE_MBEDTLS=OFF \
			-DCURL_USE_WOLFSSL=ON \
			-DCURL_USE_GNUTLS=OFF \
			-DCURL_ZLIB=OFF \
			-DCURL_BROTLI=OFF \
			-DCURL_ZSTD=OFF \
			-DHTTP_ONLY=ON \
			-DWOLFSSL_INCLUDE_DIR="$(CURDIR)/wolfssl;$(CURDIR)/wolfssl/build" \
			-DWOLFSSL_LIBRARY=$(CURDIR)/wolfssl/build/libwolfssl.a && \
		make -j`nproc`
	mkdir -p jinjac/build && cd jinjac/build && \
		rm -rf * && \
		CC="zig cc -target x86_64-linux-musl" cmake .. \
			-DCMAKE_BUILD_TYPE=Release \
			-DTRACE=OFF \
			-DCOVERAGE=OFF && \
		make -j`nproc` libjinjac_static
	mkdir -p iniparser/build && cd iniparser/build && \
		rm -rf * && \
		CC="zig cc -target x86_64-linux-musl" cmake .. \
			-DCMAKE_BUILD_TYPE=Release \
			-DBUILD_SHARED_LIBS=OFF \
			-DBUILD_STATIC_LIBS=ON && \
		make -j`nproc`

clean: # Cleans up build artefacts
	rm -f luna *.o

.PHONY: clean requirements
