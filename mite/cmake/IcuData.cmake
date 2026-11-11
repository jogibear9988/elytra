# Mite intentionally uses ICU's Node-compatible mappings for five legacy codecs. Upstream's browser
# trim removes every converter because LibWeb uses WHATWG tables. Select LibUnicode's existing
# full-data copy path for both runtime graphs instead. This ordinary variable shadows the discovered
# build tool; the installed ICU tool and upstream sources remain untouched. Cross-compilation
# already uses that path.
set(ICU_ICUPKG_EXECUTABLE "")
