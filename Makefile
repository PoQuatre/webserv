# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/05/12 18:29:33 by mle-flem          #+#    #+#              #
#    Updated: 2026/06/09 20:41:57 by uanglade         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #



# **************************************************************************** #
#                                    Config                                    #
# **************************************************************************** #

NAME		= webserv
TEST_NAME	= webserv_test
AUTHORS		= uanglade, nlaporte & mle-flem
TARGET		= $(if $(filter test,$(MAKECMDGOALS)),$(TEST_NAME),$(NAME))

CXX				= c++
OLD_CXXFLAGS	:= $(CXXFLAGS)
CXXFLAGS		= -Wall -Wextra -Werror -std=c++98 -DNDEBUG
DFLAGS			= -MMD -MP -MF $(@:.o=.d)
RM				= rm -f

ifneq ($(filter strict,$(MAKECMDGOALS)),)
CXXFLAGS	+= -Wpedantic -Wshadow -Wformat=2 -Wformat-security -Wundef \
				-Wcast-qual -Wcast-align -Wwrite-strings -Wpointer-arith \
				-Wconversion -Wsign-conversion -Wdouble-promotion \
				-Wnon-virtual-dtor -Woverloaded-virtual -Wold-style-cast \
				-fno-common
endif

ifneq ($(filter test debug debug-san asan ubsan,$(MAKECMDGOALS)),)
CXXFLAGS	+= -g3 -fno-omit-frame-pointer
CXXFLAGS	:= $(filter-out -DNDEBUG,$(CXXFLAGS))
endif

ifneq ($(filter asan,$(MAKECMDGOALS)),)
ifeq ($(filter debug-san ubsan,$(MAKECMDGOALS)),)
CXXFLAGS	+= -fsanitize=address
endif
endif

ifneq ($(filter ubsan,$(MAKECMDGOALS)),)
ifeq ($(filter debug-san asan,$(MAKECMDGOALS)),)
CXXFLAGS	+= -fsanitize=undefined
endif
endif

ifneq ($(or $(filter test debug-san,$(MAKECMDGOALS)),$(and $(filter asan,$(MAKECMDGOALS)),$(filter ubsan,$(MAKECMDGOALS)))),)
CXXFLAGS	+= -fsanitize=address,undefined
endif

ifneq ($(filter test debug-san asan ubsan,$(MAKECMDGOALS)),)
CXXFLAGS	+= -fno-sanitize-recover=all
endif

ifneq ($(filter test,$(MAKECMDGOALS)),)
CXXFLAGS	+= -DTESTING
endif

CXXFLAGS		:= $(strip $(CXXFLAGS) $(OLD_CXXFLAGS))

CLANG_FORMAT	:= $(shell command -v clang-format)
CLANG_TIDY		:= $(shell command -v clang-tidy)

SRC_DIR		= src
TEST_DIR	= test
BUILD_DIR	= build
INC_DIR		= include

INCS =	$(SRC_DIR)/ \
		$(INC_DIR)/

TEST_INCS =	$(INCS) \
			$(CRITERION_DIR)/include/



# **************************************************************************** #
#                                   Sources                                    #
# **************************************************************************** #

##begin: LIB_SRCS
LIB_SRCS =	Connection.cpp \
			HttpParser.cpp \
			Server.cpp \
			Ssl.cpp \
			cli.cpp \
			config-parser.cpp \
			dispatcher.cpp \
			http.cpp
##end: LIB_SRCS

SRCS =	$(LIB_SRCS) \
		main.cpp

##begin: TEST_SRCS
TEST_SRCS =	config_parsing.cpp \
			http_parsing.cpp
##end: TEST_SRCS

##begin: HDRS
HDRS =	include/Connection.hpp \
		include/HttpParser.hpp \
		include/Server.hpp \
		include/Ssl.hpp \
		include/cipher_suites.hpp \
		include/cli.hpp \
		include/config-parser-def.hpp \
		include/config-parser.hpp \
		include/dispatcher.hpp \
		include/http.hpp \
		include/logger.hpp \
		include/ssl_types.hpp \
		include/webserv.hpp
##end: HDRS

OBJS = $(addprefix $(BUILD_DIR)/$(SRC_DIR)/,$(SRCS:%.cpp=%.o))
DEPS = $(addprefix $(BUILD_DIR)/$(SRC_DIR)/,$(SRCS:%.cpp=%.d))
LIB_OBJS = $(addprefix $(BUILD_DIR)/$(SRC_DIR)/,$(LIB_SRCS:%.cpp=%.o))
TEST_OBJS = $(addprefix $(BUILD_DIR)/$(TEST_DIR)/,$(TEST_SRCS:%.cpp=%.o))
TEST_DEPS = $(addprefix $(BUILD_DIR)/$(TEST_DIR)/,$(TEST_SRCS:%.cpp=%.d))

LINT_SRCS		= $(addprefix $(SRC_DIR)/,$(SRCS)) $(HDRS) $(addprefix $(TEST_DIR)/,$(TEST_SRCS))
LINT_STAMPS		= $(addprefix $(BUILD_DIR)/lint/,$(addsuffix .ok,$(LINT_SRCS)))



# **************************************************************************** #
#                            External Dependencies                             #
# **************************************************************************** #

EXT_DIR = _deps

# TODO: update version on next stable release
CRITERION_VERSION		= c9869401dceb3f90e4a370928268d41f6ecbdc8f

# TODO: remove on next stable release
CRITERION_URL			= https://github.com/Snaipe/Criterion/archive/$(CRITERION_VERSION).tar.gz
CRITERION_HASH			= 9dfddc00fdbd15472d62478fc3b11c2fd4fdcfcb7c428ef3b0da5bccb4f9c84a

# TODO: uncomment on next stable release
# CRITERION_URL.DEFAULT		= https://github.com/Snaipe/Criterion/releases/download/v$(CRITERION_VERSION)/criterion-$(CRITERION_VERSION).tar.xz
# CRITERION_URL.x86_64		= https://github.com/Snaipe/Criterion/releases/download/v$(CRITERION_VERSION)/criterion-$(CRITERION_VERSION)-linux-x86_64.tar.xz
# CRITERION_URL				= $(if $(CRITERION_URL.$(ARCH)),$(CRITERION_URL.$(ARCH)),$(CRITERION_URL.DEFAULT))
# CRITERION_HASH.DEFAULT	= 8ec64e482a70b3bfc1836ace0988b3316e6c3cfeac883fb5a674dcea5083ea16
# CRITERION_HASH.x86_64		= f1b3dd5186783dcdd63433c1facd3b4d6af5244a151057370b53bdda80f16121
# CRITERION_HASH			= $(if $(CRITERION_HASH.$(ARCH)),$(CRITERION_HASH.$(ARCH)),$(CRITERION_HASH.DEFAULT))

CRITERION_DIR	= $(EXT_DIR)/criterion
# TODO: change tarball extension on next stable release
CRITERION_SRC	= $(EXT_DIR)/criterion-$(CRITERION_VERSION).tar.gz
CRITERION_NAME	= $(CRITERION_DIR)/libcriterion.so



# **************************************************************************** #
#                              Makefile variables                              #
# **************************************************************************** #

SHELL	:= bash
ARCH	:= $(shell uname -m)

shell_escape = $(subst ','\'',$(1))

MK_WIDTH	:= 80
MK_FILE		:= $(firstword $(MAKEFILE_LIST))
MK_ROOT		:= $(shell git rev-parse --show-toplevel 2>/dev/null || echo '$(CURDIR)')
MK_CXXFLAGS	:= $(BUILD_DIR)/cxxflags.txt

FOUND_CXXFLAGS	:= $(shell cat $(MK_CXXFLAGS) 2>/dev/null || echo '$(call shell_escape,$(CXXFLAGS))')
MK_REBUILD		:= $(strip $(filter clean fclean re,$(MAKECMDGOALS)) \
					$(shell [ 'x$(call shell_escape,$(FOUND_CXXFLAGS))' = 'x$(call shell_escape,$(CXXFLAGS))' ] || echo yes))

NOPRETTY	?= $(if $(CI),1,$(if $(MAKE_TERMOUT),,1))
MAKEFLAGS	+= $(if $(NOPRETTY),,--silent --no-print-directory)

CLR_BLUE	= \033[0;34m
CLR_TEAL	= \033[0;36m
CLR_GREEN	= \033[0;32m
CLR_YELLOW	= \033[0;33m
CLR_GREY	= \033[0;2m
CLR_RESET	= \033[0m

ifeq ($(shell git rev-parse HEAD >/dev/null 2>&1; echo $$?),0)
DATE := $(shell git log -1 --date=format:"%Y/%m/%d %T" --format="%ad")
HASH := $(shell git rev-parse --short HEAD)
endif

ifeq ($(CAPTURE_STEPS),)
ifeq ($(MAKELEVEL),0)
CURRENT_STEP	:= 0
NEXT_STEP		:= $(shell expr $(CURRENT_STEP) + 1)
TOTAL_STEPS		:= $(shell CAPTURE_STEPS=1 CXXFLAGS='$(call shell_escape,$(OLD_CXXFLAGS))' \
					$(MAKE) -n --no-print-directory $(MAKECMDGOALS) | grep -E -c '^#progress$$')
endif

STEPS_WIDTH		:= $(shell printf '$(TOTAL_STEPS)' | wc -m)
BAR_WIDTH		:= $(shell echo $$(($(MK_WIDTH) - 6 - $(STEPS_WIDTH) * 2)))

PROGRESS_FMT	= \033[2F\033[0J$(CLR_RESET)$(1)$(CLR_RESET)\n\n$(CLR_TEAL)[$(CLR_GREEN)%s%s$(CLR_TEAL)] $(CLR_RESET)($(CLR_BLUE)%$(STEPS_WIDTH)d$(CLR_RESET)/$(CLR_BLUE)%$(STEPS_WIDTH)d$(CLR_RESET))\n
endif



# **************************************************************************** #
#                                Makefile logic                                #
# **************************************************************************** #

define update_sources
	export LANG=C LANGUAGE=C LC_ALL=C; \
	update_files() { \
		start="$$1"; \
		indent="$$2"; \
		varname="$$3"; \
		ext="$$4"; \
		exclude="$$5"; \
		files="$$( \
			find "$$start" -type f -name "$$ext" -not -path './_*' -not -path './_*/**' \
			| awk -F/ '{ print NF "\t" $$0 }' \
			| sort -n \
			| cut -f2- \
			| grep -vE '(^|/)_([^/]*$$|[^/]+/)' \
			| { [ -n "$$exclude" ] && grep -vE "(^|/)($$exclude)$$" || cat; } \
			| sed -e "s/^$${start}\//$${indent}/" -e '$$!s/$$/ \\\\/' -e "1s/^$${indent}/$${varname} =\t/" \
			)"; \
		awk -v files="$${files//$$'\n'/\\\n}" -v name="$$varname" ' \
			BEGIN { inblock = 0 } \
			$$0 == "##begin: " name { print; print files; inblock = 1; next } \
			$$0 == "##end: " name   { print; inblock = 0; next } \
			!inblock { print } \
		' '$(MK_FILE)' > '$(MK_FILE).tmp' && \
			mv '$(MK_FILE).tmp' '$(MK_FILE)'; \
	}; \
	update_files '$(SRC_DIR)' $$'\t\t\t' 'LIB_SRCS' '*.cpp' 'main\.cpp'; \
	update_files '$(TEST_DIR)' $$'\t\t\t' 'TEST_SRCS' '*.cpp'; \
	update_files '.' $$'\t\t' 'HDRS' '*.hpp'
endef

ifeq ($(CAPTURE_STEPS),)
ifeq ($(NOPRETTY),)

define progress
	$(eval CURRENT_STEP=$(NEXT_STEP))
	$(eval NEXT_STEP=$(shell expr $(NEXT_STEP) + 1))
	width=$$(( $(CURRENT_STEP) * $(BAR_WIDTH) / $(TOTAL_STEPS) )); \
	done_str=$$(printf '%*s' "$$width" ''); \
	done_str=$${done_str// /█}; \
	todo_str=$$(printf '%*s' "$$(( $(BAR_WIDTH) - $$width ))" ''); \
	todo_str=$${todo_str// /▒}; \
	stdbuf -o1M printf '$(PROGRESS_FMT)' "$$done_str" "$$todo_str" \
		'$(CURRENT_STEP)' '$(TOTAL_STEPS)'
endef

endif
else

define progress
	#progress
endef

endif

ifeq ($(NOPRETTY),)

define success_quiet
	tmp=$$(mktemp); \
	{ $(1); } >"$$tmp" 2>&1; \
	ec=$$?; \
	[ "$$ec" -ne 0 ] && cat "$$tmp"; \
	$(RM) "$$tmp"; \
	exit "$$ec"
endef

else

define success_quiet
	$(1)
endef

endif



# **************************************************************************** #
#                                   Targets                                    #
# **************************************************************************** #

.PHONY: all
all: .header $(TARGET)

.PHONY: strict
strict: .header $(TARGET)

.PHONY: debug
debug: .header $(TARGET)

.PHONY: debug-san
debug-san: .header $(TARGET)

.PHONY: asan
asan: .header $(TARGET)

.PHONY: ubsan
ubsan: .header $(TARGET)

.PHONY: test
test: .header $(TARGET)
	@$(call progress,$(CLR_BLUE)Running $(CLR_TEAL)$(TARGET))
	./$(TARGET) $(if $(TEST_VERBOSE),--verbose) --default-timeout 2

-include $(DEPS) $(TEST_DEPS)

.PHONY: .header
.header:
ifeq ($(MAKELEVEL),0)
ifeq ($(NOPRETTY),)
	@stdbuf -o1M printf '%b' '$(CLR_GREEN)' \
		' ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄ \n' \
		'█                                                                              █\n' \
		'█                            █████                                             █\n' \
		'█                           ▒▒███                                              █\n' \
		'█   █████ ███ █████  ██████  ▒███████   █████   ██████  ████████  █████ █████  █\n' \
		'█  ▒▒███ ▒███▒▒███  ███▒▒███ ▒███▒▒███ ███▒▒   ███▒▒███▒▒███▒▒███▒▒███ ▒▒███   █\n' \
		'█   ▒███ ▒███ ▒███ ▒███████  ▒███ ▒███▒▒█████ ▒███████  ▒███ ▒▒▒  ▒███  ▒███   █\n' \
		'█   ▒▒███████████  ▒███▒▒▒   ▒███ ▒███ ▒▒▒▒███▒███▒▒▒   ▒███      ▒▒███ ███    █\n' \
		'█    ▒▒████▒████   ▒▒██████  ████████  ██████ ▒▒██████  █████      ▒▒█████     █\n' \
		'█     ▒▒▒▒ ▒▒▒▒     ▒▒▒▒▒▒  ▒▒▒▒▒▒▒▒  ▒▒▒▒▒▒   ▒▒▒▒▒▒  ▒▒▒▒▒        ▒▒▒▒▒      █\n' \
		'█                                                                              █\n' \
		'█                                                                              █\n' \
		' ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀ \n' \
		'$(CLR_RESET)\n' \
		'$(CLR_TEAL)NAME: $(CLR_YELLOW)$(NAME)$(if $(HASH),@$(HASH))\n' \
		'$(CLR_TEAL)AUTHORS: $(CLR_YELLOW)$(AUTHORS)\n' \
		'$(if $(DATE),$(CLR_TEAL)DATE: $(CLR_YELLOW)$(DATE)\n)' \
		'$(CLR_TEAL)CXX: $(CLR_YELLOW)$(CXX)\n' \
		'$(CLR_TEAL)CXXFLAGS: $(CLR_YELLOW)$(CXXFLAGS)\n' \
		'$(if $(MAKEDEBUG),$(CLR_TEAL)TOTAL_STEPS: $(CLR_YELLOW)$(TOTAL_STEPS)\n)' \
		'$(CLR_RESET)\n' \
		"$(if $(filter 0,$(TOTAL_STEPS)),$(CLR_BLUE)Nothing to be done for \
			'$(CLR_TEAL)$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)$(CLR_BLUE)'.$(CLR_RESET)\n,\n\n)"
endif
endif

$(NAME): $(OBJS)
	@$(call progress,$(CLR_BLUE)Linking $(CLR_TEAL)$@)
	@mkdir -p $(dir $(MK_CXXFLAGS))
	@echo '$(call shell_escape,$(CXXFLAGS))' > $(MK_CXXFLAGS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) -lssl -lcrypto

$(TEST_NAME): $(LIB_OBJS) $(TEST_OBJS) $(CRITERION_NAME)
	@$(call progress,$(CLR_BLUE)Linking $(CLR_TEAL)$@)
	@mkdir -p $(dir $(MK_CXXFLAGS))
	@echo '$(call shell_escape,$(CXXFLAGS))' > $(MK_CXXFLAGS)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS) -Wl,-rpath='$(dir $(CRITERION_NAME))' -L'$(dir $(CRITERION_NAME))' -lcriterion

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp $(if $(MK_REBUILD),.fclean)
	@$(call progress,$(CLR_BLUE)Compiling $(CLR_TEAL)$@)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCS:%=-I%) -o $@ -c $<

$(BUILD_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp $(CRITERION_NAME) $(if $(MK_REBUILD),.fclean)
	@$(call progress,$(CLR_BLUE)Compiling $(CLR_TEAL)$@)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -std=c++11 $(DFLAGS) $(TEST_INCS:%=-I%) -o $@ -c $<

$(BUILD_DIR)/lint/%.ok: %
	@mkdir -p $(dir $@)
	@$(call progress,$(CLR_BLUE)Linting $(CLR_TEAL)$<)
	$(call success_quiet,$(CLANG_TIDY) -p . --quiet $(if $(filter lint-fix,$(MAKECMDGOALS)),--fix) $< --warnings-as-errors='*')
	@touch $@

$(CRITERION_SRC):
	@$(call progress,$(CLR_BLUE)Downloading $(CLR_TEAL)$(notdir $@))
	@mkdir -p '$(dir $@)'
	curl -sSLo '$@' $(CRITERION_URL)
	@$(call progress,$(CLR_BLUE)Checking $(CLR_TEAL)$(notdir $@))
	echo '$(CRITERION_HASH) *$@' | sha256sum -c - >/dev/null


# TODO: remove the force src build on next stable release
ifeq ($(ARCH) force src build,x86_64)

$(CRITERION_NAME): $(CRITERION_SRC)
	@$(call progress,$(CLR_BLUE)Extracting $(CLR_TEAL)$(notdir $(CRITERION_SRC)))
	$(RM) -r $(CRITERION_DIR)
	mkdir -p '$(CRITERION_DIR)'
	tar -xf '$(CRITERION_SRC)' -C '$(CRITERION_DIR)' --strip-components=1
	@$(call progress,$(CLR_BLUE)Copying $(CLR_TEAL)$(notdir $@))
	cp '$(CRITERION_DIR)/lib/$(notdir $@)'* '$(dir $@)'

else

$(CRITERION_NAME): $(CRITERION_SRC)
	@$(call progress,$(CLR_BLUE)Extracting $(CLR_TEAL)$(notdir $(CRITERION_SRC)))
	$(RM) -r $(CRITERION_DIR)
	mkdir -p '$(CRITERION_DIR)'
	tar -xf '$(CRITERION_SRC)' -C '$(CRITERION_DIR)' --strip-components=1
	@$(call progress,$(CLR_BLUE)Configuring $(CLR_TEAL)$(notdir $@))
	$(call success_quiet,cd '$(CRITERION_DIR)' && CXXFLAGS= meson setup build)
	@$(call progress,$(CLR_BLUE)Making $(CLR_TEAL)$(notdir $@))
	$(call success_quiet,cd '$(CRITERION_DIR)' && CXXFLAGS= meson compile -C build)
	cp '$(CRITERION_DIR)/build/src/$(notdir $@)'* '$(dir $@)' 2>/dev/null || true

endif

.PHONY: clean
clean: .header
	@$(call progress,$(CLR_BLUE)clean $(CLR_TEAL)$(NAME))
	$(RM) -r $(BUILD_DIR)

.PHONY: fclean
fclean: .header clean
	@$(call progress,$(CLR_BLUE)fclean $(CLR_TEAL)$(NAME))
	$(RM) $(NAME) $(TEST_NAME)

.PHONY: .clean
.clean: .header
	@$(call progress,$(CLR_BLUE)clean $(CLR_TEAL)$(NAME))
	$(RM) -r $(BUILD_DIR)/$(SRC_DIR) $(BUILD_DIR)/$(TEST_DIR)

.PHONY: .fclean
.fclean: .header .clean
	@$(call progress,$(CLR_BLUE)fclean $(CLR_TEAL)$(NAME))
	$(RM) $(NAME) $(TEST_NAME)

.PHONY: re
re: fclean all

.PHONY: update-srcs
update-srcs: .header
	@$(call progress,$(CLR_BLUE)Updating sources of $(CLR_TEAL)$(NAME))
	$(call update_sources)

.PHONY: format
format: .header
	@$(call progress,$(CLR_BLUE)Checking formatting of $(CLR_TEAL)$(NAME))
	$(CLANG_FORMAT) --dry-run --Werror $(addprefix $(SRC_DIR)/,$(SRCS)) $(HDRS) $(addprefix $(TEST_DIR)/,$(TEST_SRCS))

.PHONY: format-fix
format-fix: .header
	@$(call progress,$(CLR_BLUE)Formatting $(CLR_TEAL)$(NAME))
	$(CLANG_FORMAT) -i $(addprefix $(SRC_DIR)/,$(SRCS)) $(HDRS) $(addprefix $(TEST_DIR)/,$(TEST_SRCS))

.PHONY: lint
lint: .header $(LINT_STAMPS)

.PHONY: lint-fix
lint-fix: .header $(LINT_STAMPS)

.PHONY: setup-criterion
setup-criterion: .header $(CRITERION_NAME)

.PHONY: .ci-args
.ci-args:
	@echo '$(CXXFLAGS) $(TEST_INCS:%=-I%)'
