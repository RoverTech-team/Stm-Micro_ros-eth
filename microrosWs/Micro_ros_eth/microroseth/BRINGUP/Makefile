# BRINGUP root Makefile — single entry point for the 3-step bringup.
# Usage:
#   make all            # build all 5 firmware variants
#   make manifest       # rebuild BUILD_MANIFEST.md (sizes + sha256)
#   make clean          # clean all build dirs
#   make step1          # flash + validate step 1 (echo/net baseline)
#   make step2          # flash + validate step 2 (shared J1-J5)
#   make step3          # flash + validate step 3 (full incl. J6, ATTENDED)
#   make test-net       # network check only (needs Jetson)
# Env overrides: CLI, JETSON, SSHPASS, SERIAL_PORT

SHELL := /bin/bash

.PHONY: all clean manifest step1 step2 step3 test-net

all:
	$(MAKE) -C cm7_echo -j4
	$(MAKE) -C cm7_e2e -j4
	$(MAKE) -C cm7_e2e_full -j4
	$(MAKE) -C cm4_follow -j4
	$(MAKE) -C cm4_full -j4

clean:
	rm -rf cm7_echo/build cm7_e2e/build cm7_e2e_full/build cm4_follow/build cm4_full/build

manifest: all
	./scripts/gen_manifest.sh

step1:
	./scripts/step1.sh

step2:
	./scripts/step2.sh

step3:
	./scripts/step3.sh

test-net:
	./scripts/test_net.sh
