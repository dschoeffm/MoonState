#ifndef DTLS_HPP
#define DTLS_HPP

#include <common.hpp>
#include <cstdint>

namespace DTLS {

namespace Record {};
namespace Handshake {};

}; // namespace DTLS

/*
 * YCM definition is a workaround for a libclang bug
 * When compiling, YCM should never be set.
 * Set YCM in a libclang based IDE in order to avoid errors
 */
#ifndef YCM
#include "dtls_handshake.hpp"
#include "dtls_record.hpp"
#endif

#endif /* DTLS_HPP */
