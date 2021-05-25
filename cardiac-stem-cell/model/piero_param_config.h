#pragma once
#include <stdint.h>
namespace ns3{
void piero_set_rl_server_ip(const char *ip);
void piero_set_rl_server_port(uint16_t port);
void piero_set_epsilon(float v);
void piero_set_random_stream(int64_t v);
}
