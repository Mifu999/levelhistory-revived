#pragma once

#include <functional>
#include <string>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

// Small wrapper around Geode's web API.
//
// v2.0.0: rewritten for Geode 5.x. WebRequest::get() now returns a WebFuture
// (arc-based) instead of a WebTask, and the old EventListener<WebTask> API is
// gone. geode::async::TaskHolder replaces it: it spawns the future on the arc
// runtime, invokes the callback on the main thread, and aborts the request
// automatically if this object is destroyed first.
class GeodeNetwork {
public:
    enum HttpMethod {
        MGet
    };
protected:
    std::function<void(GeodeNetwork *)> _onOk = nullptr;
    std::function<void(GeodeNetwork *)> _onError = nullptr;

    std::string _data;
    std::string _url;
    int _httpCode = 0;

    HttpMethod _method = MGet;

    geode::async::TaskHolder<geode::utils::web::WebResponse> _holder;
public:
    GeodeNetwork();

    void setOkCallback(std::function<void(GeodeNetwork *)> ok);
    void setErrorCallback(std::function<void(GeodeNetwork *)> error);

    void setURL(std::string url);
    void setMethod(HttpMethod method);

    std::string &getResponse();
    int getHttpCode();

    // Polite, identifiable User-Agent sent with every request so the
    // GDHistory maintainers can tell mod traffic apart (and whitelist it).
    static std::string userAgent();

    void send();
};
