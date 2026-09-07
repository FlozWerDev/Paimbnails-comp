#pragma once

#include "../ThumbRequests.hpp"

#include <Geode/Geode.hpp>
#include <string>
#include <vector>

namespace paimon::thumbreq {

class RequestFeed {
public:
    using ListCallback = geode::CopyableFunction<void(bool ok, std::vector<Request> const& requests)>;

    static RequestFeed& get() {
        static RequestFeed instance;
        return instance;
    }

    // `status` empty means every request, newest first.
    void fetch(std::string const& status, ListCallback callback);

    std::vector<Request> const& cached() const { return m_cached; }

private:
    RequestFeed() = default;

    std::vector<Request> m_cached;
};

} // namespace paimon::thumbreq
