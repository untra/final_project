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

EXE          = final
BUILD_NATIVE = build/native
BUILD_WEB    = build/web

# Sources that belong to the native CSCIx229 utility archive.
CSCI_SRCS = fatal.c loadtexbmp.c loadteximg.c print.c project.c errcheck.c object.c
CSCI_OBJS = $(addprefix $(BUILD_NATIVE)/,$(CSCI_SRCS:.c=.o))

# Sources the wasm build compiles directly (no static archives — emcc
# handles the link). loadtexbmp.c and object.c are dropped because the
# wasm build has neither a filesystem nor any callers of LoadOBJ.
WASM_SRCS = final.c bowling.c fatal.c loadteximg.c print.c project.c errcheck.c wasm_static_geom.c assets/textures.c

# Sources we lint and format (excludes generated and vendored code).
LINT_SRCS   = final.c bowling.c loadteximg.c loadtexbmp.c fatal.c print.c project.c errcheck.c object.c
FORMAT_SRCS = $(LINT_SRCS) bowling.h CSCIx229.h

# Platform branch — same idiom as the original 2015 makefile, extended.
ifeq "$(OS)" "Windows_NT"
CFLG   = -O3 -Wall
LIBS   = -lglut32cu -lglu32 -lopengl32
CLEAN  = del $(EXE).exe & if exist build\native rmdir /S /Q build\native
else
ifeq "$(shell uname)" "Darwin"
CFLG   = -O3 -Wall -Wno-deprecated-declarations
LIBS   = -framework GLUT -framework OpenGL
else
CFLG   = -O3 -Wall
LIBS   = -lglut -lGLU -lGL -lm
endif
# Also scrub legacy root-level *.o / *.a / assets/*.o from the pre-refactor
# layout. Harmless once the tree is clean.
CLEAN  = rm -rf $(BUILD_NATIVE) $(EXE); rm -f *.o *.a assets/*.o
endif

all: $(EXE)

# ------------- native -------------

# All native intermediates live under $(BUILD_NATIVE). The pattern rule
# below handles every CSCI_SRCS / final.c / bowling.c; the generated
# textures.c lives under assets/ and gets its own rule to flatten the
# path. Per-source dependencies (headers etc.) attach to the prefixed
# targets so a header touch invalidates the right object.
$(BUILD_NATIVE):
	mkdir -p $@

$(BUILD_NATIVE)/%.o: %.c | $(BUILD_NATIVE)
	gcc -c $(CFLG) -o $@ $<

$(BUILD_NATIVE)/textures.o: assets/textures.c assets/textures.h | $(BUILD_NATIVE)
	gcc -c $(CFLG) -o $@ $<

$(BUILD_NATIVE)/final.o:      final.c CSCIx229.h bowling.h assets/textures.h
$(BUILD_NATIVE)/fatal.o:      fatal.c CSCIx229.h
$(BUILD_NATIVE)/loadtexbmp.o: loadtexbmp.c CSCIx229.h
$(BUILD_NATIVE)/loadteximg.o: loadteximg.c CSCIx229.h stb_image.h
$(BUILD_NATIVE)/print.o:      print.c CSCIx229.h
$(BUILD_NATIVE)/project.o:    project.c CSCIx229.h
$(BUILD_NATIVE)/errcheck.o:   errcheck.c CSCIx229.h
$(BUILD_NATIVE)/object.o:     object.c CSCIx229.h
$(BUILD_NATIVE)/bowling.o:    bowling.c bowling.h

$(BUILD_NATIVE)/CSCIx229.a: $(CSCI_OBJS)
	ar -rcs $@ $^

$(BUILD_NATIVE)/bowling.a: $(BUILD_NATIVE)/bowling.o
	ar -rcs $@ $^

$(EXE): $(BUILD_NATIVE)/final.o $(BUILD_NATIVE)/CSCIx229.a $(BUILD_NATIVE)/bowling.a $(BUILD_NATIVE)/textures.o
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
             -s USE_WEBGL2=1 \
             -s GL_UNSAFE_OPTS=0 \
             -s ALLOW_MEMORY_GROWTH=1 \
             -lglut

# Output filename is index.html so GitHub Pages (and any static bucket)
# serves it at the directory root with no extra config.
wasm: $(BUILD_WEB)/index.html

$(BUILD_WEB)/index.html: $(WASM_SRCS) bowling.h CSCIx229.h wasm_compat.h wasm_static_geom.h assets/textures.h \
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

# ------------- probe / shot -------------
#
# wasm-probe: rebuild wasm, launch headless Chromium, capture browser console
#             + GL state JSON + canvas PNG to build/web/probe/. The agent-
#             facing iteration loop. See AGENT_RUNBOOK.md.
# native-shot: run ./final, screenshot its window via macOS screencapture -l.
#             Companion to wasm-probe for side-by-side visual diffs.

NPM      ?= npm
NODE     ?= node

tools/node_modules/.installed: tools/package.json
	cd tools && $(NPM) install
	cd tools && npx playwright install chromium
	@touch $@

wasm-probe: wasm tools/node_modules/.installed
	$(NODE) tools/wasm-probe.mjs

native-shot: $(EXE)
	@echo "note: first run requires macOS Accessibility + Screen Recording permission"
	@echo "      for the invoking terminal/process. Click through both prompts on prompt."
	bash tools/native-shot.sh

# ------------- clean -------------

clean:
	$(CLEAN)

clean-wasm:
	rm -rf $(BUILD_WEB)
	rm -f assets/textures.c assets/textures.h
	rm -rf assets/png

clean-probe:
	rm -rf $(BUILD_WEB)/probe build/native/probe

clean-all: clean clean-wasm clean-probe

.PHONY: all wasm textures lint format format-check clean clean-wasm clean-probe clean-all wasm-probe native-shot
