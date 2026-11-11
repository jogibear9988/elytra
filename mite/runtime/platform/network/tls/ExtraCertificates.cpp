/** Read once before user JS; malformed configured trust fails explicitly, never as empty trust. */
#include "platform/network/tls/ExtraCertificates.h"
#include "modules/ModulePaths.h"
#include "platform/UV.h"
#include <AK/String.h>
#include <fstream>
#include <memory>
#include <openssl/err.h>
#include <openssl/pem.h>

namespace Mite {
namespace {
    TLSExtraCertificates read_extra_certificates()
    {
        TLSExtraCertificates result;
        // libuv requires a real destination even for the first environment read.
        // Read once into the bounded path buffer, including its terminating NUL.
        std::string filename(32769, '\0');
        size_t size = filename.size();
        auto status = uv_os_getenv("NODE_EXTRA_CA_CERTS", filename.data(), &size);
        if (status == UV_ENOENT || (!status && !size))
            return result;
        if (status || size > 32768) {
            result.error = "NODE_EXTRA_CA_CERTS cannot be read or exceeds the path budget";
            return result;
        }

        filename.resize(size);
        if (filename.empty())
            return result;
        if (String::from_utf8({ filename.data(), filename.size() }).is_error()) {
            result.error = "NODE_EXTRA_CA_CERTS path is not UTF-8";
            return result;
        }

        auto path = ModulePaths::path_from_utf8(filename);
        std::error_code path_error;
        if (!std::filesystem::is_regular_file(path, path_error)) {
            result.error = "Configured NODE_EXTRA_CA_CERTS is not an available regular file";
            return result;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            result.error = "Configured NODE_EXTRA_CA_CERTS file is unavailable";
            return result;
        }

        constexpr size_t limit = 4 * 1024 * 1024;
        std::string contents(limit + 1, '\0');
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        auto length = input.gcount();
        if (input.bad() || static_cast<size_t>(length) > limit) {
            result.error = "Configured NODE_EXTRA_CA_CERTS file is unreadable or exceeds 4 MiB";
            return result;
        }

        contents.resize(static_cast<size_t>(length));
        std::unique_ptr<BIO, decltype(&BIO_free)> source(
            BIO_new_mem_buf(contents.data(), static_cast<int>(contents.size())), BIO_free);
        if (!source) {
            result.error = "Cannot allocate extra certificate parser";
            return result;
        }

        ERR_clear_error();
        while (auto* parsed = PEM_read_bio_X509_AUX(source.get(), nullptr, nullptr, nullptr)) {
            std::unique_ptr<X509, decltype(&X509_free)> certificate(parsed, X509_free);
            if (result.certificates.size() >= 1024) {
                result.error = "Configured NODE_EXTRA_CA_CERTS exceeds 1024 certificates";
                break;
            }

            std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new(BIO_s_mem()), BIO_free);
            if (!output || PEM_write_bio_X509(output.get(), certificate.get()) != 1) {
                result.error = "Cannot encode extra certificate";
                break;
            }

            char* bytes = nullptr;
            auto count = BIO_get_mem_data(output.get(), &bytes);
            if (count <= 0 || !bytes) {
                result.error = "Cannot encode extra certificate";
                break;
            }

            result.certificates.emplace_back(bytes, static_cast<size_t>(count));
        }

        auto error = ERR_peek_last_error();
        if (result.error.empty()
            && (result.certificates.empty()
                || (error
                    && !(ERR_GET_LIB(error) == ERR_LIB_PEM
                        && ERR_GET_REASON(error) == PEM_R_NO_START_LINE))))
            result.error = "Configured NODE_EXTRA_CA_CERTS contains invalid or no PEM certificates";
        ERR_clear_error();
        if (!result.error.empty())
            result.certificates.clear();
        return result;
    }
}

TLSExtraCertificates const& tls_extra_certificates()
{
    static auto const snapshot = read_extra_certificates();
    return snapshot;
}

void capture_tls_extra_certificates()
{
    (void)tls_extra_certificates();
}

bool load_tls_extra_certificates(SSL_CTX* context)
{
    auto const& snapshot = tls_extra_certificates();
    if (!snapshot.error.empty())
        return false;
    for (auto const& pem : snapshot.certificates) {
        std::unique_ptr<BIO, decltype(&BIO_free)> input(
            BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
        std::unique_ptr<X509, decltype(&X509_free)> certificate(
            input ? PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr) : nullptr, X509_free);
        if (!certificate)
            return false;
        if (X509_STORE_add_cert(SSL_CTX_get_cert_store(context), certificate.get()) != 1) {
            auto error = ERR_peek_last_error();
            if (!error || ERR_GET_LIB(error) != ERR_LIB_X509
                || ERR_GET_REASON(error) != X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                ERR_clear_error();
                return false;
            }
        }

        ERR_clear_error();
    }

    return true;
}
}
