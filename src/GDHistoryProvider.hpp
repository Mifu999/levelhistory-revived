#pragma once

#include "LevelProvider.hpp"
#include <nlohmann/json.hpp>

class GDHistoryProvider : public LevelProvider {
protected:
    std::vector<GJGameLevel *> _serverResponseParsed;
public:
    GDHistoryProvider();

    std::string getName() override;
    void downloadLevel(std::function<void(LevelProvider *, GJGameLevel *)> onComplete) override;

    std::unordered_map<LPFeatures, bool> getFeatures() override;

    std::vector<GJGameLevel *> askMultipleLevels() override;
    void cleanupLevels(bool withRelease) override;

    void getLevelData(int id, std::function<void(LevelProvider *, std::string, struct BasicLevelInformation)> onComplete) override;

    virtual std::string getErrorCodeDescription(std::string err) override;

    std::string getDescription() override;

    // v2.0.0: new GDHistory API capabilities
    bool hasDateEstimation() override;
    void getDateEstimation(int id, std::function<void(LevelProvider *, struct DateEstimation)> onComplete) override;

    bool hasArchiveStats() override;
    void getArchiveStats(std::function<void(LevelProvider *, struct ArchiveStats)> onComplete) override;

    std::string getWebsiteLevelURL(int id) override;

    void parseResult(std::string &result, std::function<void(LevelProvider *, GJGameLevel *)> onComplete, GeodeNetwork *network = nullptr) override;
    void parseResult(std::string &result, std::function<void(LevelProvider *, std::string, struct LevelProvider::BasicLevelInformation)> onComplete, GeodeNetwork *network = nullptr);
};
