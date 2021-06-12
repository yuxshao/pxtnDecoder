PXTONE_DIR:=pxtone_source
EMCC_DIR:=emscripten_source

CLANG_OPTS:=-std=c++11 -Wno-unused-value -Wno-switch -Wno-parentheses

EMCC_OPTS:=--bind -s EXPORTED_RUNTIME_METHODS="['getValue']"
EMCC_OPTS+=-s DISABLE_EXCEPTION_CATCHING=1 -s NO_EXIT_RUNTIME=1 -s NO_FILESYSTEM=1
EMCC_OPTS+=-s ALLOW_MEMORY_GROWTH=1 -s WASM=1
EMCC_OPTS+=-Oz --memory-init-file 0 --closure 0
EMCC_OPTS+=-s EXPORT_NAME=pxtnDecoder
EMCC_OPTS+=-s MODULARIZE_INSTANCE=1
EMCC_OPTS+=--pre-js $(EMCC_DIR)/pre.js --post-js $(EMCC_DIR)/post.js

EMCC_LINKS:=-I $(PXTONE_DIR)/src-oggvorbis -I $(PXTONE_DIR)

EMCC_SRCS:=-x c $(wildcard $(PXTONE_DIR)/src-oggvorbis/*.c) ./vorbis/lib/.libs/libvorbis.a
EMCC_SRCS+=-x c++ $(EMCC_DIR)/bind.cpp $(wildcard $(PXTONE_DIR)/*.cpp)

all: build/pxtnDecoder.min.js build/emDecoder.wasm

build/emDecoder.wasm: src/emDecoder.js
	cp src/emDecoder.wasm build/emDecoder.wasm

build/pxtnDecoder.min.js: build/pxtnDecoder.js
	uglifyjs build/pxtnDecoder.js -c --comments "/pxtnDecoder/" -o build/pxtnDecoder.min.js 

build/pxtnDecoder.js: $(wildcard src/*) src/emDecoder.js
	browserify src -o build/pxtnDecoder.js -t [ babelify --presets [ @babel/preset-env ] --plugins [ @babel/transform-runtime ] ]

src/emDecoder.js: $(PXTONE_DIR)/* $(EMCC_DIR)/* vorbis/lib/.libs/libvorbis.a
	em++ $(CLANG_OPTS) $(EMCC_OPTS) $(EMCC_LINKS) $(EMCC_SRCS) -o src/emDecoder.js

vorbis/lib/.libs/libvorbis.a:
	cd Ogg
	./autogen.sh
	emconfigure configure
	emmake make
	cd ../
	./autogen.sh
	emconfigure ./configure --with-ogg-includes=$(pwd)/../Ogg/include --with-ogg-libraries=$(pwd)/../Ogg/src/.libs/
	emmake make

clean:
	$(RM) -rf build temp src/emDecoder.js src/emDecoder.wasm
