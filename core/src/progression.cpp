#include "sdp/progression.hpp"

#include <algorithm>

namespace sdp {
namespace {

void appendUnique(std::vector<std::string>& target, const std::string& value) {
  if (std::find(target.begin(), target.end(), value) == target.end()) target.push_back(value);
}

}  // namespace

ProgressionEvent ProgressionService::onSolved(ProgressionState& state, bool was_checkpoint) {
  ProgressionEvent event;
  event.solved = true;
  event.checkpoint = was_checkpoint;
  event.altitude_before = state.altitude;

  const int previous_zone = zoneIdForAltitude(state.altitude);
  state.altitude += 1;  // 1 solved problem = 1 step up
  const int current_zone = zoneIdForAltitude(state.altitude);
  state.zone_id = current_zone;

  event.altitude_after = state.altitude;
  event.altitude_changed = true;
  event.zone_id = current_zone;
  if (const ZoneDef* zone = curriculum_.zone(current_zone)) event.zone_key = zone->key;

  event.zone_changed = current_zone != previous_zone;
  event.landmark_reached = isLandmarkAltitude(state.altitude) || event.zone_changed;
  if (event.landmark_reached) {
    event.landmark_key = "landmark." + std::to_string(state.altitude);
    if (event.zone_changed) event.landmark_key = "landmark." + event.zone_key;
  }

  event.new_milestones = milestonesForGain(event.altitude_before, event.altitude_after);
  return event;
}

ProgressionEvent ProgressionService::onWrong(const ProgressionState& state) const {
  ProgressionEvent event;
  event.solved = false;
  event.altitude_before = state.altitude;
  event.altitude_after = state.altitude;  // identical on purpose
  event.altitude_changed = false;
  event.zone_id = state.zone_id;
  if (const ZoneDef* zone = curriculum_.zone(state.zone_id)) event.zone_key = zone->key;
  return event;
}

int ProgressionService::zoneIdForAltitude(int altitude) const {
  const ZoneDef* zone = curriculum_.zoneForAltitude(altitude);
  return zone != nullptr ? zone->id : 0;
}

bool ProgressionService::isCheckpointAltitude(int altitude) const {
  return curriculum_.isZoneBoundary(altitude);
}

bool ProgressionService::isLandmarkAltitude(int altitude) const {
  const int interval = curriculum_.landmarkInterval();
  if (interval <= 0) return false;
  return altitude > 0 && (altitude % interval) == 0;
}

std::vector<std::string> ProgressionService::milestonesForGain(int before, int after) const {
  std::vector<std::string> keys;
  if (after <= before) return keys;

  for (int altitude = before + 1; altitude <= after; ++altitude) {
    if (altitude == 1) appendUnique(keys, "first_step");
    if (isLandmarkAltitude(altitude)) {
      appendUnique(keys, "altitude_" + std::to_string(altitude));
    }
    if (isCheckpointAltitude(altitude)) {
      appendUnique(keys, "checkpoint_" + std::to_string(altitude));
    }
    if (curriculum_.isZoneBoundary(altitude)) {
      const ZoneDef* zone = curriculum_.zoneForAltitude(altitude);
      if (zone != nullptr) appendUnique(keys, "zone_" + zone->key);
    }
  }
  return keys;
}

}  // namespace sdp
