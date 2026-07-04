TARGET ?= kobo
VERSION := $(shell sed -n '1p' VERSION)
BUILD_DIR ?= build
CACHE_DIR ?= .cache
DIST_DIR ?= dist
KOREADER_BASE_REVISION ?= e26612880d468c45e6bea6805ae422840032dd66
SOURCE_DATE_EPOCH ?= 1704067200

export BUILD_DIR CACHE_DIR DIST_DIR KOREADER_BASE KOREADER_BASE_REVISION SOURCE_DATE_EPOCH VERSION

.PHONY: fetch build test package clean target-info

fetch:
	./scripts/fetch-sources.sh

target-info:
	./scripts/target-info.py --target "$(TARGET)" --koreader-base "$$(./scripts/ensure-koreader-base.sh)"

build: fetch
	./scripts/build-mosh.sh "$(TARGET)"

test:
	python3 -m unittest discover -s tests -p '*_test.py'

package: build
	./scripts/package.sh "$(TARGET)"

clean:
	rm -rf "$(BUILD_DIR)" "$(DIST_DIR)"
