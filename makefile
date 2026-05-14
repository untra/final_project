# Build targets:
#   make / make all   – native OpenGL/GLUT binary (./final), original behavior
#   make wasm         – Emscripten/WebGL build (build/web/final.html)
#   make textures     – regenerate assets/textures.{c,h} from assets/bmp/*.bmp
#   make lint         – cppcheck over project sources
#   make format       – clang-format -i over project sources
#   make format-check – clang-format dry-run (CI)
#   make clean        – remove native artifacts
#   make clean-wasm   – remove wasm artifacts + generated textures
#   make clean-all    – both

EXE      = final
BUILD_WEB = build/web

# Sources that belong to the native CSCIx229 utility archive.
CSCI_SRCS = fatal.c loadtexbmp.c loadteximg.c print.c project.c errcheck.c object.c
CSCI_OBJS = $(CSCI_SRCS:.c=.o)

# Sources the wasm build compiles directly (no static archives — emcc
# handles the link). loadtexbmp.c and object.c are dropped because the
# wasm build has neither a filesystem nor any callers of LoadOBJ.
WASM_SRCS = final.c bowling.c fatal.c loadteximg.c print.c project.c errcheck.c assets/textures.c

# Sources we lint and format (excludes generated and vendored code).
LINT_SRCS   = final.c bowling.c loadteximg.c loadtexbmp.c fatal.c print.c project.c errcheck.c object.c
FORMAT_SRCS = $(LINT_SRCS) bowling.h CSCIx229.h

# Platform branch — same idiom as the original 2015 makefile, extended.
ifeq "$(OS)" "Windows_NT"
CFLG   = -O3 -Wall
LIBS   = -lglut32cu -lglu32 -lopengl32
CLEAN  = del *.exe *.o *.a
else
ifeq "$(shell uname)" "Darwin"
CFLG   = -O3 -Wall -Wno-deprecated-declarations
LIBS   = -framework GLUT -framework OpenGL
else
CFLG   = -O3 -Wall
LIBS   = -lglut -lGLU -lGL -lm
endif
CLEAN  = rm -f $(EXE) *.o *.a assets/*.o
endif

all: $(EXE)

# ------------- native -------------

# Per-source dependencies. final.o + the texture archive both depend on
# the generated header so a fresh checkout will trigger texture generation.
final.o:        final.c CSCIx229.h bowling.h assets/textures.h
fatal.o:        fatal.c CSCIx229.h
loadtexbmp.o:   loadtexbmp.c CSCIx229.h
loadteximg.o:   loadteximg.c CSCIx229.h stb_image.h
print.o:        print.c CSCIx229.h
project.o:      project.c CSCIx229.h
errcheck.o:     errcheck.c CSCIx229.h
object.o:       object.c CSCIx229.h
bowling.o:      bowling.c bowling.h

CSCIx229.a: $(CSCI_OBJS)
	ar -rcs $@ $^

bowling.a: bowling.o
	ar -rcs $@ $^

assets/textures.o: assets/textures.c assets/textures.h
	gcc -c $(CFLG) -o $@ $<

.c.o:
	gcc -c $(CFLG) $<

$(EXE): final.o CSCIx229.a bowling.a assets/textures.o
	gcc -O3 -o $@ $^ $(LIBS)

# ------------- textures -------------

# The generator script is the canonical source of truth for the rule;
# Make sees the script + every BMP as inputs. textures.c and textures.h
# are produced as a pair.
assets/textures.c assets/textures.h: scripts/embed-textures.sh $(wildcard assets/bmp/*.bmp)
	bash scripts/embed-textures.sh

textures: assets/textures.c assets/textures.h

# ------------- wasm -------------

EMCC      ?= emcc
EMCC_FLAGS = -O3 \
             -I. \
             -s LEGACY_GL_EMULATION=1 \
             -s ALLOW_MEMORY_GROWTH=1 \
             -lglut

# Output filename is index.html so GitHub Pages (and any static bucket)
# serves it at the directory root with no extra config.
wasm: $(BUILD_WEB)/index.html

$(BUILD_WEB)/index.html: $(WASM_SRCS) bowling.h CSCIx229.h wasm_compat.h assets/textures.h \
                         web/shell.html web/style.css web/CNAME web/.nojekyll
	mkdir -p $(BUILD_WEB)
	$(EMCC) $(EMCC_FLAGS) $(WASM_SRCS) --shell-file web/shell.html -o $@
	cp web/style.css $(BUILD_WEB)/style.css
	cp web/CNAME     $(BUILD_WEB)/CNAME
	cp web/.nojekyll $(BUILD_WEB)/.nojekyll

# ------------- lint / format -------------

lint:
	cppcheck --enable=warning,performance,portability \
	         --suppress=missingIncludeSystem \
	         --suppress='*:stb_image.h' \
	         --suppress='*:assets/textures.c' \
	         --error-exitcode=1 \
	         $(LINT_SRCS)

format:
	clang-format -i $(FORMAT_SRCS)

format-check:
	clang-format --dry-run --Werror $(FORMAT_SRCS)

# ------------- clean -------------

clean:
	$(CLEAN)

clean-wasm:
	rm -rf $(BUILD_WEB)
	rm -f assets/textures.c assets/textures.h
	rm -rf assets/png

clean-all: clean clean-wasm

.PHONY: all wasm textures lint format format-check clean clean-wasm clean-all
