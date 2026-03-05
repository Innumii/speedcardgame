#include "utils/LoadAvailableCards.hpp"

#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "utils/CSVUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace {
	std::string toLower(std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return value;
	}
}

bool LoadAvailableCardsUtil::loadFromService(std::vector<std::unique_ptr<Card>>& outCards) {
	const std::string host = EnvUtil::getServiceHost("CARDS_SERVICE", "127.0.0.1", "api.myapp.com");
	const int port = EnvUtil::getServicePort("CARDS_SERVICE", 8082, 443);
	const std::string path = "/cards/cards";

	int statusCode = -1;
	std::string responseBody;
	if (!HttpUtil::sendHttp(host, port, "GET", path, "", statusCode, responseBody)) {
		return false;
	}

	std::vector<std::unique_ptr<Card>> fetchedCards;
	std::size_t pos = 0;
	while (true) {
		const std::size_t objStart = responseBody.find('{', pos);
		if (objStart == std::string::npos) break;
		const std::size_t objEnd = responseBody.find('}', objStart);
		if (objEnd == std::string::npos) break;
		const std::string obj = responseBody.substr(objStart, objEnd - objStart + 1);

		int cid = -1;
		int cost = 0;
		int value = 0;
		int power = 0;
		int toughness = 0;
		std::string name;
		std::string type;
		std::string effect;

		JsonUtil::readJsonIntField(obj, "cid", cid);
		JsonUtil::readJsonStringField(obj, "name", name);
		JsonUtil::readJsonStringField(obj, "type", type);
		JsonUtil::readJsonIntField(obj, "cost", cost);
		JsonUtil::readJsonIntField(obj, "value", value);
		JsonUtil::readJsonIntField(obj, "power", power);
		JsonUtil::readJsonIntField(obj, "toughness", toughness);
		JsonUtil::readJsonStringField(obj, "effect", effect);

		if (!name.empty()) {
			const std::string typeLower = toLower(type);
			const int manaValue = value > 0 ? value : cost;
			if (typeLower == "creature") {
				fetchedCards.push_back(std::make_unique<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));
			} else {
				fetchedCards.push_back(std::make_unique<SpellCard>(name, effect, manaValue, cost, cid));
			}
		}

		pos = objEnd + 1;
	}

	if (fetchedCards.empty()) {
		return false;
	}

	outCards = std::move(fetchedCards);
	return true;
}

bool LoadAvailableCardsUtil::loadFromCsv(std::vector<std::unique_ptr<Card>>& outCards) {
	const std::string envPath = EnvUtil::getEnvOrDefault("CARDS_CSV_PATH", "");
	std::ifstream file;
	if (!envPath.empty() && CSVUtil::tryOpenCsv(envPath, file)) {
	} else if (CSVUtil::tryOpenCsv("cards/cards.csv", file)) {
	} else if (CSVUtil::tryOpenCsv("../cards/cards.csv", file)) {
	} else if (CSVUtil::tryOpenCsv("../../cards/cards.csv", file)) {
	} else {
		return false;
	}

	std::string line;
	if (!std::getline(file, line)) {
		return false;
	}

	std::vector<std::unique_ptr<Card>> fetchedCards;
	std::vector<std::string> fields;
	while (std::getline(file, line)) {
		if (line.empty()) continue;
		CSVUtil::parseCsvLine(line, fields);
		if (fields.size() < 8) continue;

		int cid = -1;
		int cost = 0;
		int value = 0;
		int power = 0;
		int toughness = 0;

		try {
			cid = std::stoi(fields[0]);
			cost = std::stoi(fields[3]);
			value = std::stoi(fields[4]);
			power = std::stoi(fields[5]);
			toughness = std::stoi(fields[6]);
		} catch (...) {
			continue;
		}

		const std::string name = fields[1];
		const std::string type = fields[2];
		const std::string effect = fields[7];

		if (name.empty()) continue;
		const std::string typeLower = toLower(type);
		const int manaValue = value > 0 ? value : cost;

		if (typeLower == "creature") {
			fetchedCards.push_back(std::make_unique<CreatureCard>(name, effect, manaValue, cost, power, toughness, cid));
		} else {
			fetchedCards.push_back(std::make_unique<SpellCard>(name, effect, manaValue, cost, cid));
		}
	}

	if (fetchedCards.empty()) {
		return false;
	}

	outCards = std::move(fetchedCards);
	return true;
}

