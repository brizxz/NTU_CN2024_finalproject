#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <iostream>
void initSSLServer(SSL*& ssl, SSL_CTX*& ctx);
void cleanupSSLServer(SSL*& ssl, SSL_CTX*& ctx);
void initSSLClient(SSL*& ssl, SSL_CTX*& ctx);
void cleanupSSLClient(SSL*& ssl, SSL_CTX*& ctx);


