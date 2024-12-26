#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl.h"

void initSSLServer(SSL*& ssl, SSL_CTX*& ctx) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = SSL_CTX_new(TLS_server_method());
    ssl = SSL_new(ctx);
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Load server certificate and key
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void cleanupSSLServer(SSL*& ssl, SSL_CTX*& ctx) {
    SSL_CTX_free(ctx);
    EVP_cleanup();
}

void initSSLClient(SSL*& ssl, SSL_CTX*& ctx) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = SSL_CTX_new(TLS_client_method());
    ssl = SSL_new(ctx);
    if (!ctx) {
        std::cerr << "Unable to create SSL context" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void cleanupSSLClient(SSL*& ssl, SSL_CTX*& ctx) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    EVP_cleanup();
}
