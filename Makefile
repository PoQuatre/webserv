# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/05/12 18:29:33 by mle-flem          #+#    #+#              #
#    Updated: 2026/05/14 14:26:06 by poquatre         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #



# **************************************************************************** #
#                                    Config                                    #
# **************************************************************************** #

NAME	= webserv
AUTHORS	= uanglade, nlaporte & mle-flem

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

ifneq ($(filter debug debug-san asan ubsan,$(MAKECMDGOALS)),)
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

ifneq ($(or $(filter debug-san,$(MAKECMDGOALS)),$(and $(filter asan,$(MAKECMDGOALS)),$(filter ubsan,$(MAKECMDGOALS)))),)
CXXFLAGS	+= -fsanitize=address,undefined
endif

ifneq ($(filter debug-san asan ubsan,$(MAKECMDGOALS)),)
CXXFLAGS	+= -fno-sanitize-recover=all
endif

CXXFLAGS	:= $(strip $(CXXFLAGS) $(OLD_CXXFLAGS))

CLANG_FORMAT	:= $(shell command -v clang-format)
CLANG_TIDY		:= $(shell command -v clang-tidy)

SRC_DIR		= src
BUILD_DIR	= build
INC_DIR		= include

INCS =	$(SRC_DIR)/ \
		$(INC_DIR)/



# **************************************************************************** #
#                                   Sources                                    #
# **************************************************************************** #

##begin: SRCS
SRCS =	Logger.cpp \
		Server.cpp \
		config.cpp \
		main.cpp
##end: SRCS

##begin: HDRS
HDRS =	include/Logger.hpp \
		include/Server.hpp \
		include/http.hpp \
		include/webserv.hpp
##end: HDRS

OBJS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.o))
DEPS = $(addprefix $(BUILD_DIR)/,$(SRCS:%.cpp=%.d))



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
TOTAL_STEPS		:= $(shell CAPTURE_STEPS=1 $(MAKE) -n --no-print-directory \
					$(MAKECMDGOALS) 2>/dev/null | grep -E -c '^#progress$$')
endif

STEPS_WIDTH		:= $(shell printf '$(TOTAL_STEPS)' | wc -m)
BAR_WIDTH		:= $(shell echo $$(($(MK_WIDTH) - 6 - $(STEPS_WIDTH) * 2)))

PROGRESS_FMT	= \033[2F\033[0J$(CLR_RESET)$(1)$(CLR_RESET)\n\n$(CLR_TEAL)[$(CLR_GREEN)%s%s$(CLR_TEAL)] $(CLR_RESET)($(CLR_BLUE)%$(STEPS_WIDTH)d$(CLR_RESET)/$(CLR_BLUE)%$(STEPS_WIDTH)d$(CLR_RESET))\n
endif



# **************************************************************************** #
#                                Makefile logic                                #
# **************************************************************************** #

define update_sources
	update_files() { \
		start="$$1"; \
		indent="$$2"; \
		varname="$$3"; \
		ext="$$4"; \
		files="$$( \
			find "$$start" -type f -name "$$ext" -not -path './_*' -not -path './_*/**' \
			| awk -F/ '{ print NF "\t" $$0 }' \
			| sort -n \
			| cut -f2- \
			| grep -vE '(^|/)_([^/]*$$|[^/]+/)' \
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
	update_files '$(SRC_DIR)' $$'\t\t' 'SRCS' '*.cpp'; \
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



# **************************************************************************** #
#                                   Targets                                    #
# **************************************************************************** #

.PHONY: all
all: .header $(NAME)

.PHONY: strict
strict: .header $(NAME)

.PHONY: debug
debug: .header $(NAME)

.PHONY: debug-san
debug-san: .header $(NAME)

.PHONY: asan
asan: .header $(NAME)

.PHONY: ubsan
ubsan: .header $(NAME)

-include $(DEPS)

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
		'$(CLR_TEAL)AUTHOR: $(CLR_YELLOW)$(AUTHOR)\n' \
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
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(if $(MK_REBUILD),fclean)
	@$(call progress,$(CLR_BLUE)Compiling $(CLR_TEAL)$@)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $(INCS:%=-I%) -o $@ -c $<

.PHONY: clean
clean: .header
	@$(call progress,$(CLR_BLUE)clean $(CLR_TEAL)$(NAME))
	$(RM) -r $(BUILD_DIR)

.PHONY: fclean
fclean: .header clean
	@$(call progress,$(CLR_BLUE)fclean $(CLR_TEAL)$(NAME))
	$(RM) $(NAME)

.PHONY: re
re: fclean all

.PHONY: update-srcs
update-srcs: .header
	@$(call progress,$(CLR_BLUE)Updating sources of $(CLR_TEAL)$(NAME))
	$(call update_sources)

.PHONY: format
format: .header
	@$(call progress,$(CLR_BLUE)Formatting $(CLR_TEAL)$(NAME))
	$(CLANG_FORMAT) -i $(SRCS) $(HDRS)

.PHONY: format-check
format-check: .header
	@$(call progress,$(CLR_BLUE)Checking formatting of $(CLR_TEAL)$(NAME))
	$(CLANG_FORMAT) --dry-run --Werror $(SRCS) $(HDRS)

.PHONY: lint
lint: .header
	@$(call progress,$(CLR_BLUE)Linting $(CLR_TEAL)$(NAME))
	$(CLANG_TIDY) -p . --quiet $(addprefix $(SRC_DIR)/,$(SRCS))

.PHONY: lint-fix
lint-fix: .header
	@$(call progress,$(CLR_BLUE)Linting $(CLR_TEAL)$(NAME))
	$(CLANG_TIDY) -p . --quiet --fix $(addprefix $(SRC_DIR)/,$(SRCS))

.PHONY: .ci-args
.ci-args:
	@echo '$(CXXFLAGS) $(INCS:%=-I%)'
