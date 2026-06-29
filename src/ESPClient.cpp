//
// Created by bobi on 6. 05. 26.
//

#include "ESPClient.h"
#include "SharedState.h"
#include "Network/Connection.h"
#include "Utils/SafeQueue.h"

#include <future>
#include <thread>

namespace Test
{

    ESPClient::ESPClient(std::string ip, SharedState &state)
        : logger("CONFIG"), m_sharedState(state), ip_(std::move(ip))
    {
        worker_ = std::thread(&ESPClient::workerLoop, this);
    }

    ESPClient::~ESPClient()
    {
        {
            std::lock_guard lock(jobsMutex_);
            running_ = false;
        }
        jobsCv_.notify_all();
        if (worker_.joinable())
            worker_.join();
    }

    void ESPClient::enqueue(Job job)
    {
        {
            std::lock_guard lock(jobsMutex_);
            jobs_.push_back(std::move(job));
        }
        jobsCv_.notify_one();
    }

    void ESPClient::workerLoop()
    {
        while (true)
        {
            Job job;
            {
                std::unique_lock lock(jobsMutex_);
                jobsCv_.wait(lock, [&]
                             { return !running_ || !jobs_.empty(); });

                // Shutting down: fulfill any waiting callers with nullopt (so a
                // sendRequestOpt blocked on its future doesn't hang on a dead
                // promise), then exit. Pending async jobs are simply dropped.
                if (!running_)
                {
                    while (!jobs_.empty())
                    {
                        Job pending = std::move(jobs_.front());
                        jobs_.pop_front();
                        lock.unlock();
                        if (pending.onResponse)
                            pending.onResponse(std::nullopt);
                        lock.lock();
                    }
                    return;
                }

                job = std::move(jobs_.front());
                jobs_.pop_front();
            }

            // One attempt, then retry-with-backoff on timeout. Covers the device's
            // "not ready right after the previous connection closed" window.
            std::optional<std::string> resp;
            for (int attempt = 0; attempt <= job.retries && running_; ++attempt)
            {
                if (attempt > 0)
                    std::this_thread::sleep_for(job.backoff);
                resp = doRequest(job.request, job.timeout);
                if (resp)
                    break;
            }

            if (job.onResponse)
                job.onResponse(std::move(resp));
        }
    }

    std::optional<std::string> ESPClient::doRequest(const std::string &request, std::chrono::milliseconds timeout)
    {
        Network::Connection conn(ip_);
        if (!conn.open())
            return std::nullopt;

        // Only the worker reaches here, so requests are already serialized; drop
        // any leftover reply from a previously timed-out request so each reply
        // stays matched to the request that asked for it.
        m_sharedState.getQueue().clear(ip_);

        if (!conn.send(request))
            return std::nullopt;

        // Do NOT close the request socket yet. The firmware reads + dispatches
        // inside `while (tc.isConnected())`; closing now drops the connection
        // before the request is read, so no response is ever sent. Hold it open
        // until the reply arrives on the listener (or we time out); `conn`'s
        // destructor closes it on scope exit, covering every return path.
        return m_sharedState.getQueue().pop_for(ip_, timeout);
    }

    std::optional<std::string> ESPClient::sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout)
    {
        // Enqueue a job whose callback fulfills a promise, then block on its
        // future until the worker hands back the reply. Keeps the call site
        // synchronous while still funneling all traffic through the one worker.
        auto prom = std::make_shared<std::promise<std::optional<std::string>>>();
        auto fut = prom->get_future();

        enqueue({request, timeout,
                 [prom](std::optional<std::string> reply)
                 { prom->set_value(std::move(reply)); }});

        return fut.get();
    }

    void ESPClient::sendRequestAsync(const std::string &req, std::chrono::milliseconds timeout, ResponseHandler onResponse,
                                     int retries, std::chrono::milliseconds backoff)
    {
        enqueue({req, timeout, std::move(onResponse), retries, backoff});
    }

    // void ESPClient::run(const std::string &request, bool jsonres, std::chrono::milliseconds timeout)
    // {
    //     auto result = sendRequestOpt(request, timeout);

    //     // run() is only used for mutations (reboot, highlight): drop cached reads.
    //     // m_sharedState.clearCacheForIp(ip_);

    //     if (!result.has_value())
    //     {
    //         logger.error("No response");
    //         return;
    //     }
    //     using json = nlohmann::json;
    //     json data = json::parse(result.value());
    //     if (data.contains("err"))
    //     {
    //         logger.error("{}", data["err"].get<std::string>());
    //     }
    //     else
    //     {
    //         logger.println("{}", data.dump(2));
    //     }
    //     // m_sharedState.addResponse(result.value());
    // };

    // std::string ESPClient::runStr(const std::string &request, bool jsonres, std::chrono::milliseconds timeout, bool useCache)
    // {
    //     if (useCache)
    //     {
    //         // if (auto cached = m_sharedState.getCached(ip_, request))
    //         {
    //             logger.debug("Cache hit: {}", request);
    //             return *cached;
    //         }
    //     }

    //     auto result = sendRequestOpt(request, timeout);

    //     // A non-cached request is a mutation (set*/toggle/reboot/wifi): drop this
    //     // device's cached reads so the next read reflects the new state. This keeps
    //     // the read cache (otherwise manual-invalidation only) correct after writes.
    //     if (!useCache)
    //         m_sharedState.clearCacheForIp(ip_);

    //     if (!result)
    //     {
    //         logger.error("No response");
    //         return "";
    //     }

    //     // Only cache successful, non-error responses so a transient failure
    //     // doesn't get pinned in the cache (which is manual-invalidation only).
    //     if (useCache && !result->empty() && result->find("\"err\"") == std::string::npos)
    //     {
    //         m_sharedState.putCache(ip_, request, *result);
    //     }

    //     return result.value();
    // };
}