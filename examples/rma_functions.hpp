#pragma once

# include <string>

namespace rmafunctions {

    struct RmaFunctionConfig {
        std::string client_ip_address;
        unsigned int client_port;
        unsigned int rma_buffer_size;
    };
    
}