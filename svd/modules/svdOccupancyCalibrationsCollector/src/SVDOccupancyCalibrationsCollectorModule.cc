/**************************************************************************
 * basf2 (Belle II Analysis Software Framework)                           *
 * Author: The Belle II Collaboration                                     *
 *                                                                        *
 * See git log for contributors and copyright holders.                    *
 * This file is licensed under LGPL-3.0, see LICENSE.md.                  *
 **************************************************************************/
#include <svd/modules/svdOccupancyCalibrationsCollector/SVDOccupancyCalibrationsCollectorModule.h>
#include <hlt/softwaretrigger/core/FinalTriggerDecisionCalculator.h>

#include <TH2F.h>

using namespace std;
using namespace Belle2;

//-----------------------------------------------------------------
//                 Register the Module
//-----------------------------------------------------------------
REG_MODULE(SVDOccupancyCalibrationsCollector);

//-----------------------------------------------------------------
//                 Implementation
//-----------------------------------------------------------------

SVDOccupancyCalibrationsCollectorModule::SVDOccupancyCalibrationsCollectorModule() : CalibrationCollectorModule()
{
  //Set module properties

  setDescription("This module collects hits from shaper digits to compute per sensor SVD occupancy ");
  setPropertyFlags(c_ParallelProcessingCertified);

  addParam("SVDShaperDigitsName", m_svdShaperDigitName, "Name of the SVDClusters list", std::string("SVDShaperDigits"));
  addParam("HistogramTree", m_tree, "Name of the tree in which the histograms are saved", std::string("tree"));
  addParam("skipHLTRejectedEvents", m_skipRejectedEvents, "If True, skip events rejected by HLT.", bool(false));
}

void SVDOccupancyCalibrationsCollectorModule::prepare()
{

  m_eventMetaData.isRequired();
  m_storeDigits.isRequired(m_svdShaperDigitName);

  //create histograms

  TH1F hOccupancy768("Occupancy768_L@layerL@ladderS@sensor@view", "Strip Occupancy of @layer.@ladder.@sensor @view/@side side", 768,
                     0,
                     768);
  hOccupancy768.GetXaxis()->SetTitle("cellID");
  TH1F hOccupancy512("Occupancy512_L@layerL@ladderS@sensor@view", "Strip Occupancy of @layer.@ladder.@sensor @view/@side side", 512,
                     0,
                     512);
  hOccupancy512.GetXaxis()->SetTitle("cellID");
  hm_occupancy = new SVDHistograms<TH1F>(hOccupancy768, hOccupancy768, hOccupancy768, hOccupancy512);

  m_histogramTree = new TTree("tree", "tree");
  m_histogramTree->Branch("hist", "TH1F", &m_hist, 32000, 0);
  m_histogramTree->Branch("layer", &m_layer, "layer/I");
  m_histogramTree->Branch("ladder", &m_ladder, "ladder/I");
  m_histogramTree->Branch("sensor", &m_sensor, "sensor/I");
  m_histogramTree->Branch("view", &m_side, "view/I");

  m_hnevents = new TH1F("hnevents", "Number of events", 3, 0, 2);

  //register objects needed to collect input to fill payloads
  registerObject<TTree>("HTreeOccupancyCalib", m_histogramTree);
  registerObject<TH1F>("HNEvents", m_hnevents);

}

void SVDOccupancyCalibrationsCollectorModule::startRun()
{

  VXD::GeoCache& geoCache = VXD::GeoCache::getInstance();

  for (auto layer : geoCache.getLayers(VXD::SensorInfoBase::SVD)) {
    for (auto ladder : geoCache.getLadders(layer)) {
      for (Belle2::VxdID sensor :  geoCache.getSensors(ladder)) {
        for (int view = SVDHistograms<TH2F>::VIndex ; view < SVDHistograms<TH2F>::UIndex + 1; view++) {
          (hm_occupancy->getHistogram(sensor, view))->Reset();
        }
      }
    }
  }
  // m_hnevents->Reset();
  getObjectPtr<TH1F>("HNEvents")->Reset();

}


void SVDOccupancyCalibrationsCollectorModule::collect()
{

  if (m_skipRejectedEvents && (m_resultStoreObjectPointer.isValid())) {
    const bool eventAccepted = SoftwareTrigger::FinalTriggerDecisionCalculator::getFinalTriggerDecision(*m_resultStoreObjectPointer);
    if (!eventAccepted) return;
  }

  int nDigits = m_storeDigits.getEntries();
  getObjectPtr<TH1F>("HNEvents")->Fill(1);

  if (nDigits == 0)
    return;

  //loop over the SVDShaperDigits
  int i = 0;
  while (i < nDigits) {
    VxdID theVxdID = m_storeDigits[i]->getSensorID();
    int side = m_storeDigits[i]->isUStrip();
    int CellID = m_storeDigits[i]->getCellID();

    hm_occupancy->fill(theVxdID, side, CellID);

    i++;
  }


}

void SVDOccupancyCalibrationsCollectorModule::finish()
{
}

void SVDOccupancyCalibrationsCollectorModule::closeRun()
{

  int nevents = getObjectPtr<TH1F>("HNEvents")->GetEntries(); //number of events processed in events

  B2RESULT("number of events " << nevents);

  VXD::GeoCache& aGeometry = VXD::GeoCache::getInstance();
  std::set<Belle2::VxdID> svdLayers = aGeometry.getLayers(VXD::SensorInfoBase::SVD);
  std::set<Belle2::VxdID>::iterator itSvdLayers = svdLayers.begin();
  while ((itSvdLayers != svdLayers.end())
         && (itSvdLayers->getLayerNumber() != 7)) { //loop on Layers

    std::set<Belle2::VxdID> svdLadders = aGeometry.getLadders(*itSvdLayers);
    std::set<Belle2::VxdID>::iterator itSvdLadders = svdLadders.begin();

    while (itSvdLadders != svdLadders.end()) { //loop on Ladders

      std::set<Belle2::VxdID> svdSensors = aGeometry.getSensors(*itSvdLadders);
      std::set<Belle2::VxdID>::iterator itSvdSensors = svdSensors.begin();

      while (itSvdSensors != svdSensors.end()) { //loop on sensors

        for (int k = 0; k < m_nSides; k ++) { //loop on Sides , k = isU(), k=0 is v-side, k=1 is u-side

          m_hist = hm_occupancy->getHistogram(*itSvdSensors, k);
          m_layer = itSvdSensors->getLayerNumber();
          m_ladder = itSvdSensors->getLadderNumber();
          m_sensor = itSvdSensors->getSensorNumber();
          m_side = k;

          getObjectPtr<TTree>("HTreeOccupancyCalib")->Fill();
          B2INFO("Filled sensors:" << m_layer << "." << m_ladder << "." << m_sensor << "." << m_side);
        }
        ++itSvdSensors;
      }
      ++itSvdLadders;
    }
    ++itSvdLayers;
  }


}
