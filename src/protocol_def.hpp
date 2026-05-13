#pragma once

#include "modules/payload_module.hpp"
#include "modules/checksum_module.hpp"

using protocol_modules = nb::type_list<nb::payload_module, nb::checksum_module>;
constexpr int protocol_data_width = 512;
