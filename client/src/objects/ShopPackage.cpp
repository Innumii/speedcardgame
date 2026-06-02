#include "objects/ShopPackage.hpp"

#include "utils/HttpUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <algorithm>
#include <utils/SessionUtil.hpp>

namespace {
    std::vector<ShopPackage::Package> cachedPackages;

    int packageRank(const ShopPackage::Package& pkg) {
        const std::string key = pkg.id + " " + pkg.name;
        if (key.find("small") != std::string::npos || key.find("Small") != std::string::npos) return 0;
        if (key.find("medium") != std::string::npos || key.find("Medium") != std::string::npos) return 1;
        if (key.find("large") != std::string::npos || key.find("Large") != std::string::npos) return 2;
        return 99;
    }

    bool findMatchingBracket(const std::string& text, std::size_t openPos, std::size_t& closePos) {
        if (openPos >= text.size() || text[openPos] != '[') {
            return false;
        }

        int depth = 0;
        for (std::size_t i = openPos; i < text.size(); ++i) {
            if (text[i] == '[') {
                ++depth;
            } else if (text[i] == ']') {
                --depth;
                if (depth == 0) {
                    closePos = i;
                    return true;
                }
            }
        }

        return false;
    }

    bool extractPackagesArray(const std::string& json, std::string& out) {
        const std::string needle = "\"packages\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) {
            return false;
        }

        pos = json.find('[', pos + needle.size());
        if (pos == std::string::npos) {
            return false;
        }

        std::size_t closePos = std::string::npos;
        if (!findMatchingBracket(json, pos, closePos)) {
            return false;
        }

        out = json.substr(pos, closePos - pos + 1);
        return true;
    }
}

bool ShopPackage::loadPackagesFromService() {
    const std::string host = EnvUtil::getCardsServiceHost();
    const int port = EnvUtil::getCardsServicePort();
    const std::string path = "/cards/payments/coin-packages";

    int statusCode = -1;
    std::string responseBody;
    if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody, SessionUtil::get())) {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300) {
        return false;
    }

    std::string packagesJson;
    if (!extractPackagesArray(responseBody, packagesJson)) {
        return false;
    }

    std::vector<ShopPackage::Package> loadedPackages;
    std::size_t pos = 0;
    while (pos < packagesJson.size()) {
        if (packagesJson[pos] != '{') {
            ++pos;
            continue;
        }

        std::size_t objEnd = 0;
        if (!JsonUtil::findMatchingBrace(packagesJson, pos, objEnd)) {
            break;
        }

        const std::string packageObj = packagesJson.substr(pos, objEnd - pos + 1);
        std::string id;
        std::string name;
        int coins;
        int amountCents;
        std::string currency;
        int discountPercent = 0;

        if (JsonUtil::readJsonStringField(packageObj, "id", id) &&
            JsonUtil::readJsonStringField(packageObj, "name", name) &&
            JsonUtil::readJsonIntField(packageObj, "coins", coins) &&
            JsonUtil::readJsonIntField(packageObj, "amount_cents", amountCents) &&
            JsonUtil::readJsonStringField(packageObj, "currency", currency)) {
            JsonUtil::readJsonIntField(packageObj, "discount_percent", discountPercent);

            ShopPackage::Package pkg;
            pkg.id = id;
            pkg.name = name;
            pkg.coins = coins;
            pkg.amountCents = amountCents;
            pkg.currency = currency;
            pkg.discountPercent = discountPercent;
            loadedPackages.push_back(pkg);
        }

        pos = objEnd + 1;
    }

    std::sort(loadedPackages.begin(), loadedPackages.end(), [](const ShopPackage::Package& a, const ShopPackage::Package& b) {
        const int rankA = packageRank(a);
        const int rankB = packageRank(b);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        return a.coins < b.coins;
    });

    cachedPackages = std::move(loadedPackages);
    return !cachedPackages.empty();
}

const std::vector<ShopPackage::Package>& ShopPackage::getPackages() {
    return cachedPackages;
}

int ShopPackage::getNumberOfPackages() {
    return static_cast<int>(cachedPackages.size());
}