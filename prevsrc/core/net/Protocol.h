#pragma once
#include <optional>
#include <string>

namespace core::net
{

    // Protocol 2 compatibility shim.
    //
    // The rest of the client builds protocol 1 requests ({"request": ...}) and
    // parses protocol 1 reply shapes. For a protocol 2 device, ESPClient runs
    // every request through toV2() before sending and every reply through
    // fromV2() after receiving, so callers don't care which protocol the device
    // speaks. See firmware/docs/api.md ("Migrating from protocol 1").
    struct Protocol
    {
        // Rewrite a protocol 1 request as protocol 2. Returns it unchanged if
        // it isn't JSON with a "request" key.
        static std::string toV2(const std::string &v1Request);

        // Rewrite a protocol 2 reply ({ok, data} / {ok:false, err}) into the
        // shape protocol 1 gave for `v1Request`. Errors become {"err": msg}.
        static std::optional<std::string> fromV2(const std::string &v1Request,
                                                 const std::optional<std::string> &v2Reply);
    };

}
