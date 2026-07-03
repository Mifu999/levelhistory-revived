#include "GeodeNetwork.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

void GeodeNetwork::setOkCallback(std::function<void(GeodeNetwork *)> ok) {
    _onOk = ok;
}
void GeodeNetwork::setErrorCallback(std::function<void(GeodeNetwork *)> error) {
    _onError = error;
}

void GeodeNetwork::setURL(std::string url) {
    _url = url;
}
void GeodeNetwork::setMethod(HttpMethod method) {
    _method = method;
}

std::string &GeodeNetwork::getResponse() {
    return _data;
}

int GeodeNetwork::getHttpCode() {
    return _httpCode;
}

std::string GeodeNetwork::userAgent() {
    return fmt::format(
        "LevelHistoryRevived/{} (Geode mod; {})",
        Mod::get()->getVersion().toVString(false),
        Mod::get()->getID()
    );
}

void GeodeNetwork::send() {
    geode::utils::web::WebRequest req = geode::utils::web::WebRequest();

    int64_t timeout = Mod::get()->getSettingValue<int64_t>("request-timeout");
    if (timeout < 5) timeout = 5;

    req.timeout(std::chrono::seconds(timeout));
    req.userAgent(GeodeNetwork::userAgent());

    if (_method == MGet) {
        // The callback runs on the main thread once the request completes.
        // Destroying this GeodeNetwork (e.g. `delete network;` inside the
        // callback) is safe: the TaskHolder only aborts still-pending tasks.
        _holder.spawn(req.get(_url), [this](geode::utils::web::WebResponse res) {
            this->_httpCode = res.code();
            this->_data = res.string().unwrapOr("");

            if (res.ok()) {
                if (this->_onOk != nullptr) {
                    this->_onOk(this);
                }

                return;
            }

            if (this->_data.empty()) {
                this->_data = fmt::format("Error: HTTP {}", res.code());
            }

            if (this->_onError != nullptr) {
                this->_onError(this);
            }
        });
    }
}

GeodeNetwork::GeodeNetwork() {}
