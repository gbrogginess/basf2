/**************************************************************************
 * basf2 (Belle II Analysis Software Framework)                           *
 * Author: The Belle II Collaboration                                     *
 *                                                                        *
 * See git log for contributors and copyright holders.                    *
 * This file is licensed under LGPL-3.0, see LICENSE.md.                  *
 **************************************************************************/
#include <framework/dataobjects/EventT0.h>

#include <framework/logging/Logger.h>

#include <algorithm>
#include <iterator>

using namespace Belle2;

// Final event t0
/// Check if a final event t0 is set
bool EventT0::hasEventT0() const
{
  return m_hasEventT0;
}

/// Return the final event t0, if one is set. Else, return NAN.
double EventT0::getEventT0() const
{
  B2ASSERT("Not EventT0 available, but someone tried to access it. Check with hasEventT0() method before!", hasEventT0());
  return m_eventT0.eventT0;
}

std::optional<EventT0::EventT0Component> EventT0::getEventT0Component() const
{
  if (hasEventT0()) {
    return std::make_optional(m_eventT0);
  }

  return {};
}

/// Return the final event t0 uncertainty, if one is set. Else, return NAN.
double EventT0::getEventT0Uncertainty() const
{
  return m_eventT0.eventT0Uncertainty;
}

/// Replace/set the final double T0 estimation
void EventT0::setEventT0(double eventT0, double eventT0Uncertainty, const Const::DetectorSet& detector,
                         const std::string& algorithm)
{
  setEventT0(EventT0Component(eventT0, eventT0Uncertainty, detector, algorithm));
}

void EventT0::setEventT0(const EventT0Component& eventT0)
{
  m_eventT0 = eventT0;
  m_hasEventT0 = true;
}

bool EventT0::hasTemporaryEventT0(const Const::DetectorSet& detectorSet) const
{
  for (const EventT0Component& eventT0Component : m_temporaryEventT0List) {
    if (detectorSet.contains(eventT0Component.detectorSet)) {
      return true;
    }
  }

  return false;
}

const std::vector<EventT0::EventT0Component>& EventT0::getTemporaryEventT0s() const
{
  return m_temporaryEventT0List;
}

const std::vector<EventT0::EventT0Component> EventT0::getTemporaryEventT0s(Const::EDetector detector) const
{
  std::vector<EventT0::EventT0Component> detectorT0s;

  const auto lmdSelectDetector = [detector](EventT0::EventT0Component const & c) {return c.detectorSet.contains(detector);};
  std::copy_if(m_temporaryEventT0List.begin(), m_temporaryEventT0List.end(),
               std::back_inserter(detectorT0s), lmdSelectDetector);
  return detectorT0s;
}


Const::DetectorSet EventT0::getTemporaryDetectors() const
{
  Const::DetectorSet temporarySet;

  for (const EventT0Component& eventT0Component : m_temporaryEventT0List) {
    temporarySet += eventT0Component.detectorSet;
  }

  return temporarySet;
}

unsigned long EventT0::getNumberOfTemporaryEventT0s() const
{
  return m_temporaryEventT0List.size();
}

void EventT0::addTemporaryEventT0(const EventT0Component& eventT0)
{
  m_temporaryEventT0List.push_back(eventT0);
}

void EventT0::clearTemporaries()
{
  m_temporaryEventT0List.clear();
}

void EventT0::clearEventT0()
{
  m_hasEventT0 = false;
}

std::optional<EventT0::EventT0Component> EventT0::getBestSVDTemporaryEventT0() const
{
  if (hasTemporaryEventT0(Const::EDetector::SVD)) {
    std::vector<EventT0::EventT0Component> svdT0s = getTemporaryEventT0s(Const::EDetector::SVD);
    // The most accurate SVD EventT0 candidate is the last one in the list
    return std::make_optional(svdT0s.back());
  }

  return {};
}

std::optional<EventT0::EventT0Component> EventT0::getBestCDCTemporaryEventT0() const
{
  if (hasTemporaryEventT0(Const::EDetector::CDC)) {
    std::vector<EventT0::EventT0Component> cdcT0s = getTemporaryEventT0s(Const::EDetector::CDC);
    // The most accurate CDC EventT0 candidate is the last one in the list
    return std::make_optional(cdcT0s.back());
  }

  return {};
}

std::optional<EventT0::EventT0Component> EventT0::getBestTOPTemporaryEventT0() const
{
  if (hasTemporaryEventT0(Const::EDetector::TOP)) {
    std::vector<EventT0::EventT0Component> topT0s = getTemporaryEventT0s(Const::EDetector::TOP);
    // The most accurate TOP EventT0 candidate is the last one in the list, though there should only be one at max anyway
    return std::make_optional(topT0s.back());
  }

  return {};
}

std::optional<EventT0::EventT0Component> EventT0::getBestECLTemporaryEventT0() const
{
  if (hasTemporaryEventT0(Const::EDetector::ECL)) {
    std::vector<EventT0::EventT0Component> eclT0s = getTemporaryEventT0s(Const::EDetector::ECL);
    // The most accurate ECL EevenT0 is assumed to be the one with smallest chi2/quality
    auto eclBestT0 = std::min_element(eclT0s.begin(), eclT0s.end(), [](EventT0::EventT0Component c1,
    EventT0::EventT0Component c2) {return c1.quality < c2.quality;});
    return std::make_optional(*eclBestT0);
  }

  return {};
}
