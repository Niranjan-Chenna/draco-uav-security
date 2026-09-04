#pragma once
#include <fstream>
#include <map>
#include <string>
#include "mission_decision_record.h"

std::string json_quote(const std::string& value);
using Event = std::map<std::string, std::string>;
void put(Event& event, const std::string& key, const std::string& value);
void put(Event& event, const std::string& key, const char* value);
void put(Event& event, const std::string& key, double value);
void put(Event& event, const std::string& key, bool value);
std::string serialize_event(const Event& event);
Event decision_event(const MissionDecisionRecord& record);

class EventLog {
    std::ofstream jsonl_;
    std::ofstream csv_;
public:
    explicit EventLog(const std::string& directory);
    void emit(Event event);
};
