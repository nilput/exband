The way SSL interacts with exband:
    At connection creation, a handler is called for the handshake
    Read and write functions are different for SSL connections
    If built with SSL, more Configuration options are available, namely:
        SSL block in servers
        "ssl" : {
            "fullchain": "../fullchain.pem"
            "privkey":   "../privkey.pem"
            "ciphers": [...]
        }

