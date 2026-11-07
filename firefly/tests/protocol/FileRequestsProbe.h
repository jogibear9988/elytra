/** Native request ownership acceptance; the caller supplies a disposable regular file. */
#pragma once
#include <AK/Error.h>
#include <AK/StringView.h>

ErrorOr<void> check_file_requests(StringView);
