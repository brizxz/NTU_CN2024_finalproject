#include <iostream>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
void sendFileRelay(SSL* ssl);
void receiveFileRelay(SSL* ssl);
void relayFile(SSL* senderSsl, const std::string& recipient, const std::string& fileName,
                        std::unordered_map<std::string, int> connectedClients, std::unordered_map<std::string, SSL*> connectedClientSSLs,
                        pthread_mutex_t &clientsMutex);