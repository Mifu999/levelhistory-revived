#pragma once

class GJGameLevel;
class GeodeNetwork;

#include <string>
#include <unordered_map>
#include <functional>
#include <variant>
#include <vector>
#include <cstdint>

class LevelProvider {
public:
    enum LPFeatures {
        QueryID, QueryLevelName, 
        LimitLevelArray, SetLevelArrayPage,
        ReturnMultipleLevels, SpecificRecord,
        TmpAskedLevel
    };

    struct Cached { 
        std::unordered_map<enum LPFeatures, std::variant<std::string, int>> _params;
        std::string _result;
        uint64_t _timestamp;

        bool operator ==(const struct Cached &ref) {
            bool _member0 = _params == ref._params;
            bool _member1 = _result == ref._result;

            return _member0 && _member1;
        }
    };
protected:
    std::string _currentError;
    std::vector<struct Cached> _cachedResults;
    std::string _baseUrl;
    std::unordered_map<enum LPFeatures, std::variant<std::string, int>> _params;

    std::string url_encode(const std::string value);
    void makeLevelCopyable(GJGameLevel *level);

    bool requestCached();
    bool requestCached(std::string &result);
    int getCacheID();

    virtual void parseResult(std::string &result, std::function<void(LevelProvider *, GJGameLevel *)> onComplete, GeodeNetwork *network = nullptr) = 0;

    virtual unsigned int getTimestampMaxDiff();

    std::string encodeParams();
public:
    struct BasicLevelInformation {
        int musicID = 0;
        int songID = 0;
        std::string _22songs = "";
        std::string _22sfxs = "";
    };

    // v2.0.0: estimated upload date of a level ID, as provided by the
    // archive (ISO timestamps, may be empty if unknown).
    struct DateEstimation {
        std::string approx = "";
        std::string low = "";
        std::string high = "";
        bool ok = false;
    };

    // v2.0.0: global statistics about the archive.
    struct ArchiveStats {
        long long levelCount = -1;
        long long levelStringCount = -1;
        long long songCount = -1;
        long long userCount = -1;
        long long saveCount = -1;
        long long requestCount = -1;
        bool ok = false;
    };

    virtual std::string getName() = 0;
    virtual void downloadLevel(std::function<void(LevelProvider *, GJGameLevel *)> onComplete) = 0;

    virtual std::unordered_map<enum LPFeatures, bool> getFeatures() = 0;

    virtual void setParameter(enum LPFeatures parameter, int value);
    virtual void setParameter(enum LPFeatures parameter, std::string value);

    void cleanupParameters();

    virtual std::vector<GJGameLevel *> askMultipleLevels();

    virtual void cleanupLevels(bool withRelease) = 0;

    virtual void getLevelData(int id, std::function<void(LevelProvider *, std::string, struct BasicLevelInformation)> onComplete) = 0;

    virtual std::string getErrorCodeDescription(std::string err) = 0;
    virtual std::string getErrorCode();

    virtual std::string getDescription();

    // v2.0.0: optional capabilities. Default implementations report the
    // capability as unsupported, so existing providers keep working.
    virtual bool hasDateEstimation();
    virtual void getDateEstimation(int id, std::function<void(LevelProvider *, struct DateEstimation)> onComplete);

    virtual bool hasArchiveStats();
    virtual void getArchiveStats(std::function<void(LevelProvider *, struct ArchiveStats)> onComplete);

    // v2.0.0: web page of a given level on the provider's website ("" if none).
    virtual std::string getWebsiteLevelURL(int id);

    std::string getBaseURL();
    void setBaseURL(std::string url);
};
