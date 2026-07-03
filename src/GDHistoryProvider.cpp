#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <nlohmann/json.hpp>
#include <hjfod.gmd-api/include/GMD.hpp>
#include "GeodeNetwork.hpp"
#include <ctime>

using namespace geode::prelude;

#include "GDHistoryProvider.hpp"

// v2.0.0: null-safe, type-safe JSON accessors. The old PARSE_* macros used
// operator[] + get<T>() directly, which inserted keys into the document and
// could throw nlohmann::json::type_error on unexpected types, crashing GD on
// malformed API responses. These helpers never throw and never mutate.
static std::string jStr(nlohmann::json const &obj, const char *key, std::string const &fallback = "") {
    if (!obj.is_object() || !obj.contains(key)) return fallback;
    auto const &v = obj.at(key);
    if (v.is_string()) return v.get<std::string>();
    return fallback;
}

static int jInt(nlohmann::json const &obj, const char *key, int fallback = 0) {
    if (!obj.is_object() || !obj.contains(key)) return fallback;
    auto const &v = obj.at(key);
    if (v.is_number()) return v.get<int>();
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return fallback;
}

static long long jLong(nlohmann::json const &obj, const char *key, long long fallback = -1) {
    if (!obj.is_object() || !obj.contains(key)) return fallback;
    auto const &v = obj.at(key);
    if (v.is_number()) return v.get<long long>();
    return fallback;
}

static bool jHas(nlohmann::json const &obj, const char *key) {
    return obj.is_object() && obj.contains(key) && !obj.at(key).is_null();
}

// v2.0.0: the /level/{id}/{recid}/download route sits behind Cloudflare.
// When the challenge page is served instead of the .gmd, fail with a clear
// error instead of feeding HTML to gmd-api.
static bool looksLikeCloudflareChallenge(std::string const &body) {
    return body.find("Just a moment...") != std::string::npos
        || body.find("cf-browser-verification") != std::string::npos
        || body.find("challenge-platform") != std::string::npos
        || body.find("cf_chl_") != std::string::npos;
}

// "2015-08-12T20:15:32.849Z" -> "2015-08-12"
static std::string isoDatePart(std::string const &iso) {
    if (iso.size() >= 10) return iso.substr(0, 10);
    return iso;
}

std::string GDHistoryProvider::getName() {
    return "GDHistory";
}
void GDHistoryProvider::downloadLevel(std::function<void(LevelProvider *, GJGameLevel *)> onComplete) {
    auto savedParams = _params;
    
    cleanupLevels(false);

    int cid = getCacheID();

    if (cid != -1) {
        struct Cached *cached = _cachedResults.data() + cid;

        parseResult(cached->_result, onComplete);

        return;
    }

    GeodeNetwork *network = new GeodeNetwork();

    if (_params.count(LPFeatures::QueryID)) {
        network->setURL(fmt::format("{}/api/v1/level/{}/", _baseUrl, std::get<int>(_params[LPFeatures::QueryID])));
        network->setErrorCallback([this, network, onComplete](GeodeNetwork *) {
            std::string response = network->getResponse();

            if (response.find("Server Error") != std::string::npos) {
                this->_currentError = "-7";
            } else if (looksLikeCloudflareChallenge(response)) {
                this->_currentError = "-11";
            } else {
                this->_currentError = "-1";
            }

            onComplete(this, nullptr);

            delete network;

            return;
        });

        network->setOkCallback([this, network, onComplete, savedParams](GeodeNetwork *) {
            std::string response = network->getResponse();

            if (response.find("Server Error") != std::string::npos) {
                this->_currentError = "-7";
                onComplete(this, nullptr);

                delete network;

                return;
            }

            this->_params = savedParams;
            this->parseResult(response, onComplete, network);

            struct Cached cached = {
                _params, response, (uint64_t)time(0)
            };

            this->_cachedResults.push_back(cached);

            return;
        });
        
        network->send();
    } else if (_params.count(LPFeatures::QueryLevelName)) {
        std::string a = std::get<std::string>(_params[LPFeatures::QueryLevelName]);
        int b = 0;
        int c = 0;

        if (_params.count(LPFeatures::LimitLevelArray)) {
            b = std::get<int>(_params[LPFeatures::LimitLevelArray]);
        }
        if (_params.count(LPFeatures::SetLevelArrayPage)) {
            c = std::get<int>(_params[LPFeatures::SetLevelArrayPage]);
        }

        if (b == 0) {
            b = 100;
        }

        network->setURL(fmt::format("{}/api/v1/search/level/advanced/?query={}&limit={}&offset={}", _baseUrl, url_encode(a), b, b * c));
        
        network->setErrorCallback([this, network, onComplete](GeodeNetwork *) {
            std::string error = network->getResponse();

            if (error.find("Server Error") != std::string::npos) {
                this->_currentError = "-7";
            } else if (looksLikeCloudflareChallenge(error)) {
                this->_currentError = "-11";
            } else {
                this->_currentError = "-1";
            }

            onComplete(this, nullptr);

            delete network;

            return;
        });

        network->setOkCallback([this, network, onComplete, savedParams](GeodeNetwork *) {
            std::string response = network->getResponse();

            if (response.find("Server Error") != std::string::npos) {
                this->_currentError = "-7";
                onComplete(this, nullptr);

                delete network;

                return;
            }

            this->_params = savedParams;
            this->parseResult(response, onComplete, network);

            struct Cached cached = {
                _params, response, (uint64_t)time(0)
            };

            this->_cachedResults.push_back(cached);

            return;
        });
        
        network->send();
    } else {
        delete network;
    }

    _params.clear();

    return;
}

GDHistoryProvider::GDHistoryProvider() {
    // v2.0.0: default instance URL is configurable through mod settings.
    std::string url = Mod::get()->getSettingValue<std::string>("instance-url");

    if (url.empty()) {
        url = "https://history.geometrydash.eu";
    }

    // strip trailing slashes so fmt-joined paths stay canonical
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    _baseUrl = url;
}

std::unordered_map<GDHistoryProvider::LPFeatures, bool> GDHistoryProvider::getFeatures() {
    std::unordered_map<enum LPFeatures, bool> features;

    features[LPFeatures::QueryID] = true;
    features[LPFeatures::QueryLevelName] = true;
    features[LPFeatures::LimitLevelArray] = true;
    features[LPFeatures::SetLevelArrayPage] = true;
    features[LPFeatures::ReturnMultipleLevels] = true;

    return features;
}

std::vector<GJGameLevel *> GDHistoryProvider::askMultipleLevels() {
    return _serverResponseParsed;
}
void GDHistoryProvider::cleanupLevels(bool withRelease) {
    if (!withRelease) {
        _serverResponseParsed.clear();

        return;
    }
    
    for (auto level : _serverResponseParsed) {
        if (level != nullptr) {
            level->release();
        }
    }

    _serverResponseParsed.clear();
}

#include <fstream>
#include <filesystem>

void GDHistoryProvider::getLevelData(int id, std::function<void(LevelProvider *, std::string, struct LevelProvider::BasicLevelInformation)> onComplete) {
    int recid = 0;

    log::info("(GDHistoryProvider) preparing to download {}", id);

    if (_params.count(LPFeatures::SpecificRecord)) {
        recid = std::get<int>(_params[LPFeatures::SpecificRecord]);
    }

    if (recid == 0) {
        log::info("(GDHistoryProvider) record for {} not found, getting it", id);

        setParameter(LPFeatures::QueryID, id);

        std::vector<GJGameLevel *> old_vec = this->_serverResponseParsed;
        cleanupLevels(false);

        downloadLevel([this, id, onComplete, old_vec](LevelProvider *, GJGameLevel *level) {
            struct LevelProvider::BasicLevelInformation info;

            this->_params.clear();

            if (!level) {
                this->cleanupLevels(true);
                this->_serverResponseParsed = old_vec;

                onComplete(this, this->getErrorCode(), info);

                return;
            }

            GJGameLevel *propLevel = nullptr;
            auto levels = this->askMultipleLevels();

            for (auto lvl : levels) {
                if (lvl->m_dislikes >= 1) {
                    propLevel = lvl;
                    
                    break;
                }
            }

            if (!propLevel) {
                this->cleanupLevels(true);
                this->_serverResponseParsed = old_vec;

                this->_currentError = "-5";
                onComplete(this, "-5", info);

                return;
            }

            if (propLevel->m_demonVotes <= 0) {
                this->cleanupLevels(true);
                this->_serverResponseParsed = old_vec;

                this->_currentError = "-3";
                onComplete(this, "-3", info);

                return;
            }

            log::info("(GDHistoryProvider) record for {} is {}", id, propLevel->m_demonVotes);

            this->setParameter(LPFeatures::SpecificRecord, propLevel->m_demonVotes);

            this->cleanupLevels(true);
            this->_serverResponseParsed = old_vec;

            this->getLevelData(id, onComplete);
        });

        return;
    }

    log::info("(GDHistoryProvider) downloading level {}", id);

    setParameter(LPFeatures::TmpAskedLevel, id);

    auto savedParams = _params;

    int cid = getCacheID();
    if (cid != -1) {
        log::info("(GDHistoryProvider) trying to use cached version of level {}", id);
    
        struct Cached *cached = _cachedResults.data() + cid;

        parseResult(cached->_result, onComplete);

        return;
    }

    GeodeNetwork *network = new GeodeNetwork();

    network->setURL(fmt::format("{}/level/{}/{}/download", _baseUrl, id, recid));

    network->setErrorCallback([this, onComplete, network](GeodeNetwork *) {
        struct LevelProvider::BasicLevelInformation info = {};

        if (looksLikeCloudflareChallenge(network->getResponse())) {
            this->_currentError = "-11";
            onComplete(this, "-11", info);
        } else {
            this->_currentError = "-1";
            onComplete(this, "-1", info);
        }

        delete network;
    });

    network->setOkCallback([this, onComplete, network, savedParams](GeodeNetwork *) {
        struct LevelProvider::BasicLevelInformation info;

        std::string response = network->getResponse();

        this->_params = savedParams;
        this->parseResult(response, onComplete, network);

        struct Cached cached = {
            _params, response, (uint64_t)time(0)
        };

        this->_cachedResults.push_back(cached);
    });

    network->send();

    _params.clear();
}

std::string GDHistoryProvider::getErrorCodeDescription(std::string err) {
    std::string res = "unknown error";

    std::unordered_map<std::string, std::string> errors = {
        {"-1", "http error."},
        {"-2", "gmd api error. (dumped into console)"},
        {"-3", "invalid record id."},
        {"-4", "level not found."},
        {"-5", "level data cannot be downloaded for this level. Note that this issue will be fixed if level would have downloadable link for it in the future."},
        {"-6", "insufficient rights to download this level."},
        {"-7", "gd history's api is down."},
        {"-8", "api did not return record data."},
        {"-9", "api returned record data with invalid data type."},
        {"-10", "api returned an record array with zero levels in it."},
        {"-11", "the request was blocked by the server's bot protection (Cloudflare). Try again later or open the level page in your browser."},
        {"-12", "api returned invalid JSON."}
    };

    if (errors.count(err)) {
        res = errors[err];
    }

    return res;
}

std::string GDHistoryProvider::getDescription() {
    return std::string("A Geometry Dash preservation project. ") + _baseUrl;
}

// ===== v2.0.0: new GDHistory API capabilities =====

bool GDHistoryProvider::hasDateEstimation() {
    return true;
}

void GDHistoryProvider::getDateEstimation(int id, std::function<void(LevelProvider *, struct LevelProvider::DateEstimation)> onComplete) {
    GeodeNetwork *network = new GeodeNetwork();

    network->setURL(fmt::format("{}/api/v1/date/level/{}/", _baseUrl, id));

    network->setErrorCallback([this, onComplete, network](GeodeNetwork *) {
        struct DateEstimation est;

        onComplete(this, est);

        delete network;
    });

    network->setOkCallback([this, onComplete, network](GeodeNetwork *) {
        struct DateEstimation est;

        nlohmann::json data = nlohmann::json::parse(network->getResponse(), nullptr, false);

        if (!data.is_discarded() && data.is_object()) {
            if (jHas(data, "approx")) est.approx = isoDatePart(jStr(data.at("approx"), "estimation"));
            if (jHas(data, "low"))    est.low    = isoDatePart(jStr(data.at("low"),    "estimation"));
            if (jHas(data, "high"))   est.high   = isoDatePart(jStr(data.at("high"),   "estimation"));

            est.ok = !est.approx.empty() || !est.low.empty() || !est.high.empty();
        }

        onComplete(this, est);

        delete network;
    });

    network->send();
}

bool GDHistoryProvider::hasArchiveStats() {
    return true;
}

void GDHistoryProvider::getArchiveStats(std::function<void(LevelProvider *, struct LevelProvider::ArchiveStats)> onComplete) {
    GeodeNetwork *network = new GeodeNetwork();

    network->setURL(fmt::format("{}/api/v1/counts/", _baseUrl));

    network->setErrorCallback([this, onComplete, network](GeodeNetwork *) {
        struct ArchiveStats stats;

        onComplete(this, stats);

        delete network;
    });

    network->setOkCallback([this, onComplete, network](GeodeNetwork *) {
        struct ArchiveStats stats;

        nlohmann::json data = nlohmann::json::parse(network->getResponse(), nullptr, false);

        if (!data.is_discarded() && data.is_object()) {
            stats.levelCount       = jLong(data, "level_count");
            stats.levelStringCount = jLong(data, "level_string_count");
            stats.songCount        = jLong(data, "song_count");
            stats.userCount        = jLong(data, "gduser_count");
            stats.saveCount        = jLong(data, "save_count");
            stats.requestCount     = jLong(data, "request_count");

            stats.ok = stats.levelCount >= 0;
        }

        onComplete(this, stats);

        delete network;
    });

    network->send();
}

std::string GDHistoryProvider::getWebsiteLevelURL(int id) {
    return fmt::format("{}/level/{}/", _baseUrl, id);
}

void GDHistoryProvider::parseResult(std::string &result, std::function<void(LevelProvider *, GJGameLevel *)> onComplete, GeodeNetwork *network) {
    if (_params.count(LPFeatures::QueryID) && !_params.count(LPFeatures::TmpAskedLevel)) {
        // v2.0.0: non-throwing parse. The old code used the throwing overload,
        // which crashed the game whenever the API returned anything that was
        // not valid JSON (Cloudflare page, proxy error page, ...).
        nlohmann::json data = nlohmann::json::parse(result, nullptr, false);

        if (data.is_discarded() || !data.is_object()) {
            this->_currentError = "-12";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        if (!data.contains("records")) {
            this->_currentError = "-8";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        nlohmann::json records = data.at("records");
        if (!records.is_array()) {
            this->_currentError = "-9";
            onComplete(this, nullptr);
                    
            if (network != nullptr) delete network;

            return;
        }

        int levels = records.size();

        if (levels == 0) {
            this->_currentError = "-10";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        for (int i = 0; i < levels; i++) {
            nlohmann::json leveljson = records[i];

            if (!leveljson.is_object()) continue;

            GJGameLevel *level = GJGameLevel::create();

            level->m_levelName   = jStr(data, "cache_level_name").c_str();
            // v2.0.0 fix: the API field is "description", not
            // "level_description" — descriptions were silently never shown.
            level->m_levelDesc   = jStr(leveljson, "description").c_str();
            level->m_creatorName = jStr(data, "cache_username").c_str();

            // song/sfx id lists: prefer the record's own values (accurate for
            // that specific version), fall back to the level-wide cache.
            level->m_songIDs = jStr(leveljson, "song_ids", jStr(data, "song_ids")).c_str();
            level->m_sfxIDs  = jStr(leveljson, "sfx_ids",  jStr(data, "sfx_ids")).c_str();

            level->m_audioTrack      = jInt(leveljson, "official_song");
            level->m_gameVersion     = jInt(leveljson, "game_version");
            level->m_ratings         = jInt(leveljson, "rating");
            level->m_ratingsSum      = jInt(leveljson, "rating_sum");
            level->m_downloads       = jInt(leveljson, "downloads");
            level->m_likes           = jInt(leveljson, "likes");
            level->m_levelLength     = jInt(leveljson, "length");
            level->m_userID          = jInt(data, "cache_user_id");
            level->m_coins           = jInt(leveljson, "coins");
            level->m_coinsVerified   = jInt(leveljson, "coins_verified");
            level->m_rateStars       = jInt(leveljson, "stars");
            level->m_accountID       = jInt(leveljson, "account_id");
            level->m_levelID         = jInt(data, "online_id");
            level->m_demon           = jInt(leveljson, "demon");
            level->m_autoLevel       = (bool)jInt(leveljson, "auto");
            level->m_isEditable      = (bool)jInt(leveljson, "level_string_available");
            level->m_demonDifficulty = jInt(leveljson, "demon_type");
            level->m_featured        = jInt(leveljson, "feature_score");
            level->m_isEpic          = jInt(leveljson, "epic");

            // NOTE (inherited hack): the GDHistory record id is smuggled in
            // m_demonVotes so getLevelData() can request that exact record.
            level->m_demonVotes = jInt(leveljson, "id");
        
            if (jHas(leveljson, "song") && leveljson.at("song").is_object()) {
                level->m_songID = jInt(leveljson.at("song"), "online_id");
            }

            // v2.0.0: richer per-record date label, shown under the cell:
            // "2015-11-21 (v1) [glm_03]"
            std::string realDate   = isoDatePart(jStr(leveljson, "real_date"));
            int levelVersion       = jInt(leveljson, "level_version");
            std::string recordType = jStr(leveljson, "record_type");

            std::string dateLabel = realDate.empty() ? std::string("unknown date") : realDate;
            if (levelVersion > 0)     dateLabel += fmt::format(" (v{})", levelVersion);
            if (!recordType.empty())  dateLabel += fmt::format(" [{}]", recordType);

            level->m_uploadDate = dateLabel.c_str();

            bool has_leveldata = (bool)jInt(leveljson, "level_string_available");
            if (has_leveldata) {
                // NOTE (inherited hack): m_dislikes==1 flags "this record has
                // downloadable level data" for the UI.
                level->m_dislikes = 1;
                level->m_likes++;
            } else {
                level->m_dislikes = 0;
            }

            this->makeLevelCopyable(level);

            level->retain();
                        
            this->_serverResponseParsed.push_back(level);
        }

        if (this->_serverResponseParsed.empty()) {
            this->_currentError = "-10";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        if (this->_params.count(LPFeatures::LimitLevelArray)) {
            int max = std::get<int>(this->_params[LPFeatures::LimitLevelArray]);

            if (max > 0) {
                std::vector<GJGameLevel *> new_vec;

                for (int i = 0; i < max && i < (int)_serverResponseParsed.size(); i++) {
                    new_vec.push_back(this->_serverResponseParsed.at(i));
                }

                // v2.0.0 fix: release the levels that get trimmed off instead
                // of leaking them.
                for (int i = max; i < (int)_serverResponseParsed.size(); i++) {
                    this->_serverResponseParsed.at(i)->release();
                }

                this->_serverResponseParsed = new_vec;
            }
        }
                    
        onComplete(this, this->_serverResponseParsed.at(0));

        if (network != nullptr) delete network;
    } else if (_params.count(LPFeatures::QueryLevelName) && !_params.count(LPFeatures::TmpAskedLevel)) {

        nlohmann::json data = nlohmann::json::parse(result, nullptr, false);

        if (data.is_discarded() || !data.is_object()) {
            this->_currentError = "-12";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        if (!data.contains("hits")) {
            this->_currentError = "-8";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        nlohmann::json records = data.at("hits");
        if (!records.is_array()) {
            this->_currentError = "-9";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        int levels = records.size();

        if (levels == 0) {
            this->_currentError = "-10";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }

        for (int i = 0; i < levels; i++) {
            nlohmann::json leveljson = records[i];

            if (!leveljson.is_object()) continue;

            GJGameLevel *level = GJGameLevel::create();

            level->m_levelName   = jStr(leveljson, "cache_level_name").c_str();
            level->m_uploadDate  = isoDatePart(jStr(leveljson, "cache_submitted")).c_str();
            level->m_creatorName = jStr(leveljson, "cache_username").c_str();

            level->m_gameVersion     = jInt(leveljson, "cache_game_version", 22);
            level->m_ratings         = jInt(leveljson, "cache_rating");
            level->m_ratingsSum      = jInt(leveljson, "cache_rating_sum");
            level->m_demon           = jInt(leveljson, "cache_demon");
            level->m_autoLevel       = (bool)jInt(leveljson, "cache_auto");
            level->m_demonDifficulty = jInt(leveljson, "cache_demon_type");
            level->m_downloads       = jInt(leveljson, "cache_downloads");
            level->m_likes           = jInt(leveljson, "cache_likes");
            level->m_levelLength     = jInt(leveljson, "cache_length");
            // v2.0.0 fix: was read from the response root (always absent);
            // the field lives on each hit.
            level->m_userID          = jInt(leveljson, "cache_user_id");
            level->m_rateStars       = jInt(leveljson, "cache_stars");
            level->m_levelID         = jInt(leveljson, "online_id");
            level->m_featured        = jInt(leveljson, "cache_featured");
            level->m_isEpic          = jInt(leveljson, "cache_epic");

            // NOTE (inherited hack): GDHistory internal id -> m_demonVotes
            level->m_demonVotes = jInt(leveljson, "id");

            bool has_leveldata = (bool)jInt(leveljson, "cache_level_string_available");
            if (has_leveldata) {
                log::info("(GDHistoryProvider) level \"{}\" by \"{}\" has leveldata", level->m_levelName, level->m_creatorName);
                level->m_dislikes = 1;
                level->m_likes++;
            } else {
                log::info("(GDHistoryProvider) level \"{}\" by \"{}\" does not has leveldata", level->m_levelName, level->m_creatorName);
                level->m_dislikes = 0;
            }

            this->makeLevelCopyable(level);

            level->retain();
                    
            this->_serverResponseParsed.push_back(level);
        }

        if (this->_serverResponseParsed.empty()) {
            this->_currentError = "-10";
            onComplete(this, nullptr);

            if (network != nullptr) delete network;

            return;
        }
                
        onComplete(this, this->_serverResponseParsed.at(0));

        if (network != nullptr) delete network;
    }
}

void GDHistoryProvider::parseResult(std::string &result, std::function<void(LevelProvider *, std::string, struct LevelProvider::BasicLevelInformation)> onComplete, GeodeNetwork *network) {
    if (_params.count(LPFeatures::TmpAskedLevel)) {
        struct LevelProvider::BasicLevelInformation info;

        if (result.find("Server Error") != std::string::npos) {
            this->_currentError = "-7";
            onComplete(this, "-7", info);

            if (network != nullptr) delete network;

            return;
        }

        // v2.0.0: recognize Cloudflare challenge pages before handing the
        // body to gmd-api.
        if (looksLikeCloudflareChallenge(result)) {
            this->_currentError = "-11";
            onComplete(this, "-11", info);

            if (network != nullptr) delete network;

            return;
        }

        if (result.find("This record does not contain any level data") != std::string::npos) {
            this->_currentError = "-5";
            onComplete(this, "-5", info);

            if (network != nullptr) delete network;

            return;
        }
        if (result.find("You do not have the rights to download this record") != std::string::npos) {
            this->_currentError = "-6";
            onComplete(this, "-6", info);
            
            if (network != nullptr) delete network;
            
            return;
        }

        std::ofstream gmdfile;

        std::string path = Mod::get()->getSaveDir().generic_string();
        std::string _path = fmt::format("{}/temp.gmd", path);

        gmdfile.open(_path);
        gmdfile << result;
        gmdfile.close();

        auto file = gmd::ImportGmdFile::from(_path);
        
        file.inferType();

        auto _result = file.intoLevel();
        
        if (_result.isErr() || !_result.isOk()) {
            log::info("(GDHistoryProvider) error: {}", _result.unwrapErr());
            log::info("(GDHistoryProvider) level string: {}", result);
    
            onComplete(this, "-2", info);

            if (network != nullptr) delete network;

            return;
        }

        auto level = _result.unwrap();
        level->retain();

        auto str = level->m_levelString;
        int mid = level->m_audioTrack;
        int sid = level->m_songID;

        info.musicID = mid;
        info.songID = sid;
        info._22songs = level->m_songIDs;
        info._22sfxs = level->m_sfxIDs;

        level->release();

        onComplete(this, str, info);

        if (network != nullptr) delete network;
    }
}
