#pragma once


namespace rmafunctions {

    const int IPV4_ADDRESS_STRING_LENGTH = 16;

    struct RmaFunctionConfig {
        char client_ip_address[IPV4_ADDRESS_STRING_LENGTH];
        unsigned int client_port;
        int64_t rma_memory_in_bytes;
    };
    
}