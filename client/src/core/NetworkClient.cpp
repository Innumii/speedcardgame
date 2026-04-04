//This class is used for creation/handling of the Client connection to the server
#include "core/NetworkClient.hpp"
#include "utils/EnvUtil.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <fstream>
#include <array>
#include <cstdlib>

#include <openssl/x509v3.h>

namespace {
    bool fileExists(const std::string& path) {
        std::ifstream file(path);
        return file.good();
    }

    bool loadTlsTrustStore(SSL_CTX* sslCtx) {
        const char* explicitCaPath = std::getenv("GAME_SERVER_CA_CERT_PATH");
        if (explicitCaPath != nullptr && explicitCaPath[0] != '\0') {
            if (!fileExists(explicitCaPath)) {
                std::cerr << "[NetworkClient] GAME_SERVER_CA_CERT_PATH not found: " << explicitCaPath << "\n";
                return false;
            }

            if (SSL_CTX_load_verify_locations(sslCtx, explicitCaPath, nullptr) != 1) {
                std::cerr << "[NetworkClient] Failed to load GAME_SERVER_CA_CERT_PATH: " << explicitCaPath << "\n";
                return false;
            }

            return true;
        }

        static const std::array<const char*, 4> candidatePaths = {
            "./certs/ca.crt",
            "../certs/ca.crt",
            "client/certs/ca.crt",
            "/certs/ca.crt",
        };

        for (const char* candidatePath : candidatePaths) {
            if (!fileExists(candidatePath)) {
                continue;
            }

            if (SSL_CTX_load_verify_locations(sslCtx, candidatePath, nullptr) != 1) {
                std::cerr << "[NetworkClient] Failed to load CA certificate bundle: " << candidatePath << "\n";
                return false;
            }

            return true;
        }

        if (SSL_CTX_set_default_verify_paths(sslCtx) != 1) {
            std::cerr << "[NetworkClient] Failed to load system certificate trust store\n";
            return false;
        }

        return true;
    }

    bool configureHostnameVerification(SSL* ssl, const std::string& host) {
        if (SSL_set_tlsext_host_name(ssl, host.c_str()) != 1) {
            std::cerr << "[NetworkClient] Failed to configure TLS SNI host\n";
            return false;
        }

        X509_VERIFY_PARAM* verifyParam = SSL_get0_param(ssl);
        X509_VERIFY_PARAM_set_hostflags(verifyParam, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

        if (X509_VERIFY_PARAM_set1_host(verifyParam, host.c_str(), 0) == 1) {
            return true;
        }

        if (X509_VERIFY_PARAM_set1_ip_asc(verifyParam, host.c_str()) == 1) {
            return true;
        }

        std::cerr << "[NetworkClient] Failed to configure TLS hostname/IP verification\n";
        return false;
    }
}

NetworkClient::NetworkClient(SocketMode m) : socketFd(-1), connected(false), mode(m) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
    }
#endif
    initOpenSSL();
}

NetworkClient::~NetworkClient() {
    disconnect();
    cleanupSSL();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkClient::initOpenSSL() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    sslCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_load_verify_locations(sslCtx, "./certs/ca.crt", nullptr);
    if (!sslCtx) {
        std::cerr << "[NetworkClient] Failed to create SSL_CTX\n";
        return false;
    }

    if (SSL_CTX_set_min_proto_version(sslCtx, TLS1_2_VERSION) != 1) {
        std::cerr << "[NetworkClient] Failed to set minimum TLS version\n";
        return false;
    }

    if (!loadTlsTrustStore(sslCtx)) {
        return false;
    }

    // Always verify the server certificate and hostname.
    SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER, nullptr);
    if (!SSL_CTX_load_verify_locations(sslCtx, "./certs/ca.crt", nullptr)) {
        std::cerr << "[NetworkClient] Failed to load CA certificate\n";
        // You can choose to return false if you want strict verification
    }

    return true;
}

void NetworkClient::cleanupSSL() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (sslCtx) {
        SSL_CTX_free(sslCtx);
        sslCtx = nullptr;
    }
}

bool NetworkClient::connectTo(const std::string& ip, int port) {
    if (connected) return false;

    // 1️⃣ Resolve host (supports both raw IP and DNS hostnames)
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const std::string portStr = std::to_string(port);
    addrinfo* results = nullptr;
    const int gaiResult = getaddrinfo(ip.c_str(), portStr.c_str(), &hints, &results);
    if (gaiResult != 0 || !results) {
#ifdef _WIN32
        std::cerr << "[NetworkClient] getaddrinfo() failed: " << gaiResult << "\n";
#else
        std::cerr << "[NetworkClient] getaddrinfo() failed: " << gai_strerror(gaiResult) << "\n";
#endif
        return false;
    }

    // 2️⃣ Create socket + connect (try each resolved address)
    bool connectStarted = false;
    for (addrinfo* node = results; node != nullptr; node = node->ai_next) {
        socketFd = socket(node->ai_family, node->ai_socktype, node->ai_protocol);
        if (socketFd < 0) {
            continue;
        }

        if (connect(socketFd, node->ai_addr, static_cast<int>(node->ai_addrlen)) == 0) {
            connectStarted = true;
            break;
        }

#ifdef _WIN32
        int err = WSAGetLastError();
        if (mode == SocketMode::NonBlocking && err == WSAEWOULDBLOCK) {
            connectStarted = true;
            break;
        }
#else
        if (mode == SocketMode::NonBlocking && errno == EINPROGRESS) {
            connectStarted = true;
            break;
        }
#endif

        CLOSE_SOCKET(socketFd);
        socketFd = -1;
    }
    freeaddrinfo(results);

    if (!connectStarted || socketFd < 0) {
        std::cerr << "[NetworkClient] connect() failed for host: " << ip << ":" << port << "\n";
        return false;
    }

    // 3️⃣ Set socket mode
#ifdef _WIN32
    u_long m = (mode == SocketMode::NonBlocking ? 1 : 0);
    ioctlsocket(socketFd, FIONBIO, &m);
#else
    int flags = fcntl(socketFd, F_GETFL, 0);
    if (mode == SocketMode::NonBlocking)
        fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    else
        fcntl(socketFd, F_SETFL, flags & ~O_NONBLOCK);
#endif

    // 4️⃣ Create SSL object
    ssl = SSL_new(sslCtx);
    if (!ssl) {
        std::cerr << "[NetworkClient] SSL_new failed\n";
        CLOSE_SOCKET(socketFd);
        socketFd = -1;
        return false;
    }
    SSL_set_fd(ssl, socketFd);

    const std::string tlsServerName = EnvUtil::getEnvOrDefault("GAME_SERVER_TLS_SERVER_NAME", ip.c_str());
    if (!configureHostnameVerification(ssl, tlsServerName)) {
        SSL_free(ssl);
        ssl = nullptr;
        CLOSE_SOCKET(socketFd);
        socketFd = -1;
        return false;
    }

    // 5️⃣ Configure verification
    SSL_CTX_set_verify(sslCtx, SSL_VERIFY_NONE, nullptr);

    // 6️⃣ Perform TLS handshake
    int ret = 0;
    while ((ret = SSL_connect(ssl)) != 1) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // retry
            continue;
        }
        std::cerr << "[NetworkClient] TLS handshake failed\n";
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        ssl = nullptr;
        CLOSE_SOCKET(socketFd);
        socketFd = -1;
        return false;
    }

    std::cout << "[NetworkClient] TLS handshake successful\n";
    connected = true;
    return true;
}

void NetworkClient::disconnect() {
    if (!connected) return;

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }

    CLOSE_SOCKET(socketFd);
    socketFd = -1;
    connected = false;
}

bool NetworkClient::isConnected() const {
    return connected;
}

bool NetworkClient::sendString(const std::string& msg) {
    return send(msg.data(), msg.size());
}

bool NetworkClient::send(const void* data, size_t size) {
    if (!connected || !ssl) return false;

    const char* buffer = static_cast<const char*>(data);
    size_t totalSent = 0;

    while (totalSent < size) {
        int sent = SSL_write(ssl, buffer + totalSent, size - totalSent);
        if (sent <= 0) {
            int err = SSL_get_error(ssl, sent);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;

            std::cerr << "[NetworkClient] SSL_write failed\n";
            disconnect();
            return false;
        }
        totalSent += sent;
    }

    return true;
}

int NetworkClient::receive(void* buffer, size_t size) {
    if (!connected || !ssl) return -1;

    int received = SSL_read(ssl, buffer, static_cast<int>(size));
    if (received <= 0) {
        int err = SSL_get_error(ssl, received);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;

        disconnect();
        return -1;
    }
    return received;
}

//GAME ACTIONS (send data as JSON)

bool NetworkClient::sendPlayCard(int cardId, int lane, std::optional<int> targetLane, std::optional<int> targetIndex) {
    if (!connected) return false;

    std::ostringstream ss;

    if (targetLane.has_value() && targetIndex.has_value()) {
        // Include both target lane and whether it's opponent
        ss << "PLAY " << cardId << " " << lane
           << " " << *targetLane << " " << *targetIndex << "\n";
    } else {
        // Creature summon: no target
        ss << "PLAY " << cardId << " " << lane << "\n";
    }

    return sendString(ss.str());
}

bool NetworkClient::sendDiscardCard(int cardId) {
    if (!connected) return false;
    std::ostringstream ss;
    ss << "DISCARD " << cardId << "\n";
    return sendString(ss.str());
}

