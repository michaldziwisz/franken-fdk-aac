# fdkaac-franken - top-level Makefile (tylko weryfikacja funkcjonalna).
#
# Budowa binarek: patrz README.md (cross-mingw z WSL). Ten Makefile NIE buduje
# enkodera - sluzy do formalnego `make check` na juz zbudowanych .exe, bo
# upstream fdk-aac/nu774 nie dostarczaja zadnego testu (make check tam = no-op).

.PHONY: check test clean-test

check test:
	@bash tests/check.sh

clean-test:
	@rm -f _check_in.wav _check_dab_in.wav _check_out*.m4a _check_out*.aac _check_out*.txt _check_out*.dabp _check_out*.bin
	@rm -rf _check_dabtmp
