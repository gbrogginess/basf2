/**************************************************************************
 * basf2 (Belle II Analysis Software Framework)                           *
 * Author: The Belle II Collaboration                                     *
 *                                                                        *
 * See git log for contributors and copyright holders.                    *
 * This file is licensed under LGPL-3.0, see LICENSE.md.                  *
 **************************************************************************/

/* Own header. */
#include <klm/modules/KLMTracking/KLMTrackingModule.h>

/* KLM headers. */
#include <klm/bklm/geometry/GeometryPar.h>
#include <klm/modules/KLMTracking/KLMTrackFinder.h>
#include <klm/eklm/geometry/TransformDataGlobalAligned.h> //TODO: Is this the right module

/* Basf2 headers. */
#include <framework/dataobjects/EventMetaData.h>
#include <framework/datastore/StoreObjPtr.h>
#include <framework/datastore/StoreArray.h>
#include <framework/logging/Logger.h>
#include <tracking/dataobjects/RecoHitInformation.h>

/* C++ standard libraries*/
#include <set>
#include <iostream>

using namespace Belle2;
using namespace Belle2::KLM;
using namespace CLHEP;

REG_MODULE(KLMTracking);

KLMTrackingModule::KLMTrackingModule() : Module(),
  m_effiYX(nullptr),
  m_effiYZ(nullptr),
  m_passYX(nullptr),
  m_totalYX(nullptr),
  m_passYZ(nullptr),
  m_totalYZ(nullptr),
  m_runTotalEvents(0),
  m_runTotalEventsWithTracks(0)
{
  for (int i = 0; i < 8; ++i) {
    m_total[0][i] = nullptr;
    m_total[1][i] = nullptr;
    m_pass[0][i] = nullptr;
    m_pass[1][i] = nullptr;
    m_effiVsLayer[0][i] = nullptr;
    m_effiVsLayer[1][i] = nullptr;
  }
  setDescription("Perform standard-alone straight line tracking for KLM. ");
  addParam("MatchToRecoTrack", m_MatchToRecoTrack, "[bool], whether match KLMTrack to RecoTrack; (default is false)", false);
  addParam("MaxAngleRequired", m_maxAngleRequired,
           "[degree], match KLMTrack to RecoTrack; angle between them is required to be smaller than (default 10)", double(10.0));
  addParam("MaxDistance", m_maxDistance,
           "[cm], During efficiency calculation, distance between track and 2dhit must be smaller than (default 10)", double(10.0));
  addParam("MaxSigma", m_maxSigma,
           "[sigma], During efficiency calculation, uncertainty of 2dhit must be smaller than (default 5); ", double(5));
  addParam("MinHitList", m_minHitList,
           ", During track finding, a good track after initial seed hits must be larger than is (default 2); ", unsigned(2));
  addParam("MaxHitList", m_maxHitList,
           ", During track finding, a good track after initial seed hits must be smaller than is (default 60); ", unsigned(60));
  addParam("MinNLayer", m_minNLayer,
           ", Only look at tracks with more than n number of layers; ", int(4));
  addParam("StudyEffiMode", m_studyEffi, "[bool], run in efficieny study mode (default is false)", false);
  addParam("outputName", m_outPath, "[string],  output file name containing efficiencies plots ",
           std::string("standaloneKLMEffi.root"));
}

KLMTrackingModule::~KLMTrackingModule()
{

}

void KLMTrackingModule::initialize()
{

  hits2D.isRequired();
  m_storeTracks.registerInDataStore();
  m_storeTracks.registerRelationTo(hits2D);
  m_storeTracks.registerRelationTo(recoTracks);
  recoHitInformation.registerRelationTo(hits2D);
  hits2D.registerRelationTo(recoTracks);

  if (m_studyEffi)
    B2INFO("KLMTrackingModule::initialize this module is running in efficiency study mode!");

  m_file = new TFile(m_outPath.c_str(), "recreate");
  TString hname;
  std::string labelFB[2] = {"BB", "BF"};
  int Nbin = 16;
  float gmin = -350;
  float gmax = 350;
  int gNbin = 150;

  //TODO: Extend to include EKLM...
  m_totalYX  = new TH2F("totalYX", " denominator Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_passYX  = new TH2F("passYX", " numerator Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_totalYZ  = new TH2F("totalYZ", " denominator Y vs. Z", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_passYZ  = new TH2F("passYZ", " numerator Y vs. Z", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYX  = new TH2F("effiYX", " effi. Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYZ  = new TH2F("effiYZ", " effi. Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYX->GetXaxis()->SetTitle("x (cm)");
  m_effiYX->GetYaxis()->SetTitle("y (cm)");
  m_effiYZ->GetXaxis()->SetTitle("z (cm)");
  m_effiYZ->GetYaxis()->SetTitle("y (cm)");
  for (int iF = 0; iF < 2; iF++) {
    for (int iS = 0; iS < 8; iS++) {
      hname.Form("effi_%s%i", labelFB[iF].c_str(), iS);
      m_effiVsLayer[iF][iS]  = new TEfficiency(hname, hname, Nbin, 0, 16);
      hname.Form("total_%s%i", labelFB[iF].c_str(), iS);
      m_total[iF][iS] = new TH1F(hname, hname, Nbin, 0, 16);
      hname.Form("pass_%s%i", labelFB[iF].c_str(), iS);
      m_pass[iF][iS] = new TH1F(hname, hname, Nbin, 0, 16);
    }
  }

  //EKLM Plots TODO: Not tested yet
  /*
  m_totalYXE  = new TH2F("totalYX", " denominator Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_passYXE  = new TH2F("passYX", " numerator Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_totalYZE  = new TH2F("totalYZ", " denominator Y vs. Z", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_passYZE  = new TH2F("passYZ", " numerator Y vs. Z", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYXE  = new TH2F("effiYX", " effi. Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYZE  = new TH2F("effiYZ", " effi. Y vs. X", gNbin, gmin, gmax, gNbin, gmin, gmax);
  m_effiYXE->GetXaxis()->SetTitle("x (cm)");
  m_effiYXE->GetYaxis()->SetTitle("y (cm)");
  m_effiYZE->GetXaxis()->SetTitle("z (cm)");
  m_effiYZE->GetYaxis()->SetTitle("y (cm)");
  for (int iF = 0; iF < 2; iF++) {
    for (int iS = 0; iS < 8; iS++) {
      hname.Form("effi_%s%i", labelFB[iF].c_str(), iS);
      m_effiVsLayer[iF][iS]  = new TEfficiency(hname, hname, Nbin, 0, 16);
      hname.Form("total_%s%i", labelFB[iF].c_str(), iS);
      m_total[iF][iS] = new TH1F(hname, hname, Nbin, 0, 16);
      hname.Form("pass_%s%i", labelFB[iF].c_str(), iS);
      m_pass[iF][iS] = new TH1F(hname, hname, Nbin, 0, 16);
    }
  } //end of EKLM layer info
  */

}

void KLMTrackingModule::beginRun()
{
  StoreObjPtr<EventMetaData> eventMetaData("EventMetaData", DataStore::c_Event);
  m_runNumber.push_back((int)eventMetaData->getRun());
  m_runTotalEvents = 0;
  m_runTotalEventsWithTracks = 0;
}

void KLMTrackingModule::event()
{
  m_storeTracks.clear();
  bool thereIsATrack = false;

  if (!m_studyEffi) {
    runTracking(0, KLMElementNumbers::c_BKLM, -1, -1, -1);
    runTracking(0, KLMElementNumbers::c_EKLM, -1, -1, -1);
    if (m_storeTracks.getEntries() > 0)
      thereIsATrack = true;
  } else {
    for (int iSection = 0; iSection < 2; iSection++) {
      for (int iSector = 0; iSector < 8; iSector++) {
        for (int iLayer = 0; iLayer < 15; iLayer++) {
          runTracking(1, KLMElementNumbers::c_BKLM, iSection, iSector, iLayer);
          if (m_storeTracks.getEntries() > 0)
            thereIsATrack = true;
          generateEffi(KLMElementNumbers::c_BKLM, iSection, iSector, iLayer);
          //clear tracks so prepare for the next layer efficieny study
          m_storeTracks.clear();
        }
      }
    }
  }

  m_runTotalEvents++;
  if (thereIsATrack)
    m_runTotalEventsWithTracks++;
}

void KLMTrackingModule::runTracking(int mode, int iSubdetector, int iSection, int iSector, int iLayer)
{
  //m_storeTracks.clear(); //done in event stage

  KLMTrackFitter* m_fitter = new KLMTrackFitter();
  KLMTrackFinder*  m_finder = new KLMTrackFinder();
  m_finder->registerFitter(m_fitter);

  if (hits2D.getEntries() < 1)
    return;
  if (mode == 1) { //efficieny study
    for (int j = 0; j < hits2D.getEntries(); j++) {
      if (hits2D[j]->getSubdetector() != iSubdetector) //TODO: shoud we kee?
        continue;
      hits2D[j]->isOnStaTrack(false);
    }
  }

  for (int hi = 0; hi < hits2D.getEntries() - 1; ++hi) {
    if (hits2D[hi]->getSubdetector() != iSubdetector) //TODO: Should we keep?
      continue;

    if (mode == 1 && isLayerUnderStudy(iSection, iSector, iLayer, hits2D[hi]))
      continue;
    if (mode == 1 && !isSectorUnderStudy(iSection, iSector, hits2D[hi]))
      continue;
    if (hits2D[hi]->isOnStaTrack())
      continue;
    if (hits2D[hi]->isOutOfTime())
      continue;
    for (int hj = hi + 1; hj < hits2D.getEntries(); ++hj) {

      if (hits2D[hj]->isOnStaTrack())
        continue;
      if (hits2D[hj]->isOutOfTime())
        continue;
      // at least for track seed, hits should remain in the same subdetector
      if (hits2D[hi]->getSubdetector() != hits2D[hj]->getSubdetector())
        continue;
      if (sameSector(hits2D[hi], hits2D[hj]) &&
          std::abs(hits2D[hi]->getLayer() - hits2D[hj]->getLayer()) < 3)
        continue;

      std::list<KLMHit2d*> sectorHitList;


      std::list<KLMHit2d*> seed;
      seed.push_back(hits2D[hi]);
      seed.push_back(hits2D[hj]);

      for (int ho = 0; ho < hits2D.getEntries(); ++ho) {

        // Exclude seed hits.
        if (ho == hi || ho == hj)
          continue;
        if (mode == 1 && (hits2D[ho]->getSubdetector() != iSubdetector))
          continue;
        if (mode == 1 && isLayerUnderStudy(iSection, iSector, iLayer, hits2D[hj]))
          continue;
        if (mode == 1 && !isSectorUnderStudy(iSection, iSector, hits2D[hj]))
          continue;
        if (hits2D[ho]->isOnStaTrack())
          continue;
        //TODO: consider removing the commented lines below
        if (mode == 1 && !sameSector(hits2D[ho], hits2D[hi]))
          continue;
        if (hits2D[ho]->isOutOfTime())
          continue;
        sectorHitList.push_back(hits2D[ho]);
      }

      /* Require at least four hits (minimum for good track, already two as seed, so here we require 2) but
       * no more than 60 (most likely noise, 60 would be four good tracks).
       * TODO: Should be tuned since we have EKLM hits now. 60 was from BKLMTracking
       */
      if (sectorHitList.size() < m_minHitList || sectorHitList.size() > m_maxHitList)
        continue;

      std::list<KLMHit2d*> m_hits;

      if (m_finder->filter(seed, sectorHitList, m_hits, iSubdetector)) {
        KLMTrack* m_track = m_storeTracks.appendNew();
        m_track->setTrackParam(m_fitter->getTrackParam());
        m_track->setTrackParamErr(m_fitter->getTrackParamErr());
        m_track->setTrackChi2(m_fitter->getChi2());
        m_track->setNumHitOnTrack(m_fitter->getNumHit());
        m_track->setIsValid(m_fitter->isValid());
        m_track->setIsGood(m_fitter->isGood());
        std::list<KLMHit2d*>::iterator j;
        m_hits.sort(sortByLayer);
        int nBKLM = 0; int nEKLM = 0;
        for (j = m_hits.begin(); j != m_hits.end(); ++j) {
          (*j)->isOnStaTrack(true);
          m_track->addRelationTo((*j));
          if ((*j)->getSubdetector() ==  KLMElementNumbers::c_BKLM)
            nBKLM += 1;
          else if ((*j)->getSubdetector() ==  KLMElementNumbers::c_EKLM)
            nEKLM += 1;
        } //end of klmhit2d loop
        B2DEBUG(31, "KLMTracking::runTracking totalHit " << m_hits.size() << ", nBKLM " << nBKLM << ", nEKLM " << nEKLM);
        m_track->setInSubdetector(nBKLM, nEKLM);

        //match KLMTrack to RecoTrack
        if (mode == 0) {
          RecoTrack* closestTrack = nullptr;
          B2DEBUG(30, "KLMTracking::runTracking started RecoTrack matching");
          if (m_MatchToRecoTrack) {
            if (findClosestRecoTrack(m_track, closestTrack)) {
              B2DEBUG(30, "KLMTracking::runTracking was able to find ClosestRecoTrack");
              m_track->addRelationTo(closestTrack);
              for (j = m_hits.begin(); j != m_hits.end(); ++j) {
                unsigned int sortingParameter = closestTrack->getNumberOfTotalHits();
                if ((*j)->getSubdetector() == KLMElementNumbers::c_BKLM)
                  closestTrack->addBKLMHit((*j), sortingParameter, RecoHitInformation::OriginTrackFinder::c_LocalTrackFinder);
                else if ((*j)->getSubdetector() == KLMElementNumbers::c_EKLM) {
                  for (const EKLMAlignmentHit& alignmentHit : (*j)->getRelationsFrom<EKLMAlignmentHit>()) {
                    closestTrack->addEKLMHit(&(alignmentHit), sortingParameter,
                                             RecoHitInformation::OriginTrackFinder::c_LocalTrackFinder);
                  }
                }
              } //end of hit loop
            } //end of closest recotrack
          }
        } //end of KLMTrack to RecoTrack
      } //end of finder loop
    }
  }

  delete m_fitter;
  delete m_finder;

}

void KLMTrackingModule::endRun()
{
  m_totalEvents.push_back(m_runTotalEvents);
  m_totalEventsWithTracks.push_back(m_runTotalEventsWithTracks);
}

void KLMTrackingModule::terminate()
{
  for (long unsigned int i = 0; i < m_runNumber.size(); i++) {
    float ratio = (float)m_totalEventsWithTracks.at(i) / (float)m_totalEvents.at(i);
    B2INFO("KLMTrackingModule::terminate run " << m_runNumber.at(i) << " --> " << ratio * 100 << "% of events has 1+ KLMTracks");
  }

  m_file->cd();
  for (int iF = 0; iF < 2; iF++) {
    for (int iS = 0; iS < 8; iS++) {
      m_effiVsLayer[iF][iS]->Write();
      m_total[iF][iS]->Write();
      m_pass[iF][iS]->Write();
    }
  }

  for (int i = 0; i < m_totalYX->GetNbinsX(); i++) {
    for (int j = 0; j < m_totalYX->GetNbinsY(); j++) {
      float num = m_passYX->GetBinContent(i + 1, j + 1);
      float denom = m_totalYX->GetBinContent(i + 1, j + 1);
      if (num > 0) {
        m_effiYX->SetBinContent(i + 1, j + 1, num / denom);
        m_effiYX->SetBinError(i + 1, j + 1, sqrt(num * (denom - num) / (denom * denom * denom)));
      } else {
        m_effiYX->SetBinContent(i + 1, j + 1, 0);
        m_effiYX->SetBinError(i + 1, j + 1, 0);
      }

      num = m_passYZ->GetBinContent(i + 1, j + 1);
      denom = m_totalYZ->GetBinContent(i + 1, j + 1);
      if (num > 0) {
        m_effiYZ->SetBinContent(i + 1, j + 1, num / denom);
        m_effiYZ->SetBinError(i + 1, j + 1, sqrt(num * (denom - num) / (denom * denom * denom)));
      } else {
        m_effiYZ->SetBinContent(i + 1, j + 1, 0);
        m_effiYZ->SetBinError(i + 1, j + 1, 0);
      }
    }
  }


  m_totalYX->SetOption("colz");
  m_passYX->SetOption("colz");
  m_totalYZ->SetOption("colz");
  m_passYZ->SetOption("colz");
  m_effiYX->SetOption("colz");
  m_effiYZ->SetOption("colz");

  m_totalYX->Write();
  m_passYX->Write();
  m_totalYZ->Write();
  m_passYZ->Write();
  m_effiYX->Write();
  m_effiYZ->Write();
  m_file->Close();

}

bool KLMTrackingModule::sameSector(KLMHit2d* hit1, KLMHit2d* hit2)
{
  if (hit1->getSection() == hit2->getSection() && hit1->getSector() == hit2->getSector())
    return true;
  else return false;
}


bool KLMTrackingModule::findClosestRecoTrack(KLMTrack* klmTrk, RecoTrack*& closestTrack)
{

  //StoreArray<RecoTrack> recoTracks;
  RelationVector<KLMHit2d> klmHits = klmTrk->getRelationsTo<KLMHit2d> ();

  if (klmHits.size() < 1) {
    B2INFO("KLMTrackingModule::findClosestRecoTrack, something is wrong! there is a KLMTrack but no klmHits");
    return false;
  }
  if (recoTracks.getEntries() < 1) {
    B2DEBUG(20, "KLMTrackingModule::findClosestRecoTrack, there is no recoTrack");
    return false;
  }
  double oldDistanceSq = INFINITY;
  double oldAngle = INFINITY;
  closestTrack = nullptr;
  //klmHits are already sorted by layer
  //possible two hits in one layer?
  //genfit requires TVector3 rather than XYZVector

  TVector3 firstKLMHitPosition(klmHits[0]->getPosition().X(),
                               klmHits[0]->getPosition().Y(),
                               klmHits[0]->getPosition().Z());

  // To get direction (angle) below, we have two points on the klmTrk:
  //     (x1, TrackParam[0]+TrackParam[1]*x1, TrackParam[2]+TrackParam[3]*x1)
  //     (x2, TrackParam[0]+TrackParam[1]*x2, TrackParam[2]+TrackParam[3]*x2)
  // the difference vector is
  //     (x2-x1, TrackParam[1]*(x2-x1), TrackParam[3]*(x2-x1))
  // which is proportional to
  //     (1, TrackParam[1], TrackParam[3]).
  TVector3 klmTrkVec(1.0,  klmTrk->getTrackParam()[1], klmTrk->getTrackParam()[3]);

  TMatrixDSym cov(6);
  TVector3 pos; // initializes to (0,0,0)
  TVector3 mom; // initializes to (0,0,0)

  for (RecoTrack& track : recoTracks) {
    if (track.wasFitSuccessful()) {
      try {
        genfit::MeasuredStateOnPlane state = track.getMeasuredStateOnPlaneFromLastHit();
        B2DEBUG(30, "KLMTracking::findClosestRecoTrack, finished MSOP from last hit");
        //! Translates MeasuredStateOnPlane into 3D position, momentum and 6x6 covariance.
        state.getPosMomCov(pos, mom, cov);
        if (mom.Y() * pos.Y() < 0) {
          state = track.getMeasuredStateOnPlaneFromFirstHit();
        }
        const TVector3& distanceVec = firstKLMHitPosition - pos;
        state.extrapolateToPoint(firstKLMHitPosition);
        double newDistanceSq = distanceVec.Mag2();
        double angle = klmTrkVec.Angle(mom);
        // choose closest distance or minimum open angle ?
        // overwrite old distance
        if (newDistanceSq < oldDistanceSq) {
          oldDistanceSq = newDistanceSq;
          closestTrack = &track;
          oldAngle = angle;
        }

        /* if(angle<oldAngle)
        {
        oldAngle=angle;
        closestTrack = &track;
        }
        */
        B2DEBUG(30, "KLMTracking::findClosestRecoTrack, step one done");
      } catch (genfit::Exception& e) {
      }// try
    }
  }

  // can not find matched RecoTrack
  // problem here is the errors of the track parameters are not considered!
  // best way is the positon or vector direction are required within 5/10 sigma ?
  if (oldAngle > m_maxAngleRequired)
    return false;
  // found matched RecoTrack
  else {
    B2DEBUG(28, "KLMTrackingModule::findClosestRecoTrack RecoTrack found! ");
    return true;
  }

}

//TODO: GENERALIZE ME [HARDEST PART!? :'( ]
void KLMTrackingModule::generateEffi(int iSubdetector, int iSection, int iSector, int iLayer)
{
  //TODO: let's comment out during testing. remove this later

  std::set<int> m_pointUsed;
  std::set<int> layerList;
  m_pointUsed.clear();
  if (m_storeTracks.getEntries() < 1)
    return;
  B2DEBUG(10, "KLMTrackingModule:generateEffi: " << iSection << " " << iSector << " " << iLayer);

  for (int it = 0; it < m_storeTracks.getEntries(); it++) {
    //if(m_storeTracks[it]->getTrackChi2()>10) continue;
    //if(m_storeTracks[it]->getNumHitOnTrack()<6) continue;
    int cnt1 = 0;
    int cnt2 = 0;

    layerList.clear();


    RelationVector<KLMHit2d> relatedHit2D = m_storeTracks[it]->getRelationsTo<KLMHit2d>();
    for (const KLMHit2d& hit2D : relatedHit2D) {
      if (hit2D.getSubdetector() != iSubdetector)
        continue;
      if (hit2D.getLayer() > iLayer + 1)
      {cnt1++; layerList.insert(hit2D.getLayer());}
      if (hit2D.getLayer() < iLayer + 1)
      {cnt2++; layerList.insert(hit2D.getLayer());}
      if (hit2D.getLayer() == iLayer + 1) {
        B2DEBUG(10, "generateEffi: Hit info. Secti/sector/Lay = " << hit2D.getSection()
                << "/" << hit2D.getSector() - 1 << "/" << hit2D.getLayer() - 1);
        B2DEBUG(11, "generateEffi: Hit info. x/y/z = " << hit2D.getPositionX()
                << "/" << hit2D.getPositionY() << "/" << hit2D.getPositionZ());
      }
    }

    if ((int)layerList.size() < m_minNLayer)
      continue;

    if (iLayer != 0 && cnt2 < 1)
      return;
    if (iLayer != 14 && cnt1 < 1)
      return;
    //TODO: Extend to includ EKLM?
    //m_GeoPar = GeometryPar::instance(); w/ geometry cuts.

    if (iSubdetector == KLMElementNumbers::c_BKLM) {

      const bklm::GeometryPar* bklmGeo = m_GeoPar->BarrelInstance();
      const bklm::Module* module = bklmGeo->findModule(iSection, iSector + 1, iLayer + 1);
      const bklm::Module* refmodule = bklmGeo->findModule(iSection, iSector + 1, 1);
      int minPhiStrip = module->getPhiStripMin();
      int maxPhiStrip = module->getPhiStripMax();
      int minZStrip = module->getZStripMin();
      int maxZStrip = module->getZStripMax();

      CLHEP::Hep3Vector local = module->getLocalPosition(minPhiStrip, minZStrip);
      CLHEP::Hep3Vector local2 = module->getLocalPosition(maxPhiStrip, maxZStrip);
      float minLocalY, maxLocalY;
      float minLocalZ, maxLocalZ;
      if (local[1] > local2[1]) {
        maxLocalY = local[1];
        minLocalY = local2[1];
      } else {
        maxLocalY = local2[1];
        minLocalY = local[1];
      }
      if (local[2] > local2[2]) {
        maxLocalZ = local[2];
        minLocalZ = local2[2];
      } else {
        maxLocalZ = local2[2];
        minLocalZ = local[2];
      }

      //in global coordinates
      TVectorD trkPar = m_storeTracks[it]->getTrackParam();

      //line in local coordinates
      Hep3Vector point1(0, trkPar[0], trkPar[2]);
      Hep3Vector point2(1, trkPar[0] + trkPar[1], trkPar[2] + trkPar[3]);

      Hep3Vector refPoint1(0., 0., 0.); Hep3Vector refPoint2(0., 0., 0.);
      refPoint1 = refmodule->globalToLocal(point1);
      refPoint2 = refmodule->globalToLocal(point2);

      Hep3Vector refSlope(refPoint2[0] - refPoint1[0], refPoint2[1] - refPoint1[1], refPoint2[2] - refPoint1[2]);



      //defined in coordinates relative to layer 1 of this sector.
      float reflocalX = fabs(bklmGeo->getActiveMiddleRadius(iSection, iSector + 1,
                                                            iLayer + 1) - bklmGeo->getActiveMiddleRadius(iSection, iSector + 1, 1));
      if (refmodule->isFlipped())
        reflocalX = -reflocalX;
      float X_coord = (reflocalX - refPoint1[0]) / refSlope[0];
      float reflocalY = refPoint1[1] + refSlope[1] * X_coord;
      float reflocalZ = refPoint1[2] + refSlope[2] * X_coord;

      //Hep3Vector global(globalX, globalY, globalZ);
      Hep3Vector reflocal(reflocalX, reflocalY, reflocalZ);
      Hep3Vector global(0., 0., 0.);
      global = refmodule->localToGlobal(reflocal);


      float localX = module->globalToLocal(global)[0];
      float localY = module->globalToLocal(global)[1];
      float localZ = module->globalToLocal(global)[2];

      B2DEBUG(10, "KLMTrackingModule:generateEffi: RefLocal " << reflocalX << " " << reflocalY << " " << reflocalZ);
      B2DEBUG(10, "KLMTrackingModule:generateEffi: Global " << global[0] << " " << global[1] << " " << global[2]);
      B2DEBUG(10, "KLMTrackingModule:generateEffi: Local " << localX << " " << localY << " " << localZ);



      //geometry cut
      if (localY > minLocalY && localY < maxLocalY && localZ > minLocalZ && localZ < maxLocalZ) {

        bool m_iffound = false;
        m_total[iSection][iSector]->Fill(iLayer + 1);
        m_totalYX->Fill(global[0], global[1]);
        m_totalYZ->Fill(global[2], global[1]);

        for (int he = 0; he < hits2D.getEntries(); ++he) {
          if (!isLayerUnderStudy(iSection, iSector, iLayer, hits2D[he])) {
            B2DEBUG(11, "not isLayerUnderStudy");
            continue;
          }
          if (hits2D[he]->isOutOfTime()) {
            B2DEBUG(11, "hit isOutOfTime");
            continue;
          }
          //if alreday used, skip
          if (m_pointUsed.find(he) != m_pointUsed.end()) {
            B2DEBUG(11, "passed unused");
            continue;
          }
          B2DEBUG(11, "KLMTrackingModule:generateEffi: Reached Distance Check");
          double error, sigma;
          float distance = distanceToHit(m_storeTracks[it], hits2D[he], error, sigma);
          float hitX = hits2D[he]->getPositionX();
          float hitY = hits2D[he]->getPositionY();
          float hitZ = hits2D[he]->getPositionZ();
          float deltaX = hitX - global[0]; float deltaY = hitY - global[1];  float deltaZ = hitZ - global[2];
          float dist = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
          B2DEBUG(10, "dist w/ hit = " << dist << ", dist func = " << distance << ", error = " << error);
          if (distance < m_maxDistance && sigma < m_maxSigma) {
            m_iffound = true;
            B2DEBUG(10, "KLMTrackingModule:generateEffi: Hit found!");
          }
          if (m_iffound) {
            m_pointUsed.insert(he);
            m_pass[iSection][iSector]->Fill(iLayer + 1);
            m_passYX->Fill(global[0], global[1]);
            m_passYZ->Fill(global[2], global[1]);
            break;
          }
        }

        m_effiVsLayer[iSection][iSector]->Fill(m_iffound, iLayer + 1);
        //efficiencies will be defined at terminate stage
      } //end of BKLM geometry cut

    } //end of BKLM section


  }//end of loop tracks
}


bool KLMTrackingModule::sortByLayer(KLMHit2d* hit1, KLMHit2d* hit2)
{

  return hit1->getLayer() < hit2->getLayer();

}

bool KLMTrackingModule::isLayerUnderStudy(int section, int iSector, int iLayer, KLMHit2d* hit)
{
  if (hit->getSection() == section && hit->getSector() == iSector + 1 &&  hit->getLayer() == iLayer + 1)
    return true;
  else return false;
}

bool KLMTrackingModule::isSectorUnderStudy(int section, int iSector, KLMHit2d* hit)
{
  if (hit->getSection() == section && hit->getSector() == iSector + 1)
    return true;
  else return false;
}

double KLMTrackingModule::distanceToHit(KLMTrack* track, KLMHit2d* hit,
                                        double& error,
                                        double& sigma)
{

  double x, y, z, dx, dy, dz, distance;

  error = DBL_MAX;
  sigma = DBL_MAX;

  TVectorD m_GlobalPar = track->getTrackParam();


  if (hit->getSubdetector() == KLMElementNumbers::c_BKLM) {

    const bklm::GeometryPar* bklmGeo = m_GeoPar->BarrelInstance();
    const Belle2::bklm::Module* corMod = bklmGeo->findModule(hit->getSection(), hit->getSector(), hit->getLayer());

    //x = hit->getPositionX();
    //y = m_GlobalPar[ 0 ] + x * m_GlobalPar[ 1 ];
    //z = m_GlobalPar[ 2 ] + x * m_GlobalPar[ 3 ];

    //since there are z-planes, let's exploit this fact.
    z = hit->getPositionZ();
    x = (z - m_GlobalPar[ 2 ]) / m_GlobalPar[ 3 ];
    y = m_GlobalPar[ 0 ] + x * m_GlobalPar[ 1 ];

    dx = x - hit->getPositionX() ;
    dy = y - hit->getPositionY();
    dz = z - hit->getPositionZ();

    double x2 = hit->getPositionX();
    double y2 = m_GlobalPar[ 0 ] + x2 * m_GlobalPar[ 1 ];
    double z2 = m_GlobalPar[ 2 ] + x2 * m_GlobalPar[ 3 ];

    double dx2 = x2 - hit->getPositionX();
    double dy2 = y2 - hit->getPositionY();
    double dz2 = z2 - hit->getPositionZ();

    double dist2 = sqrt(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);


    distance = sqrt(dx * dx + dy * dy + dz * dz);
    double hit_localPhiErr = corMod->getPhiStripWidth() / sqrt(12);
    double hit_localZErr = corMod->getZStripWidth() / sqrt(12);

    //error from tracking is ignored here
    error = sqrt(pow(hit_localPhiErr, 2) +
                 pow(hit_localZErr, 2));
    B2DEBUG(11, "Dist = " << distance << ", error = " << error);
    B2DEBUG(11, "Dist2 = " << dist2 << ", error = " << error);
  } //end of BKLM section

  else if (hit->getSubdetector() == KLMElementNumbers::c_EKLM) {

    const EKLM::GeometryData* eklmGeo = m_GeoPar->EndcapInstance();


    // use z coordinate as main point of interest
    // should be close enough to distance of closest appraoch
    z = hit->getPositionZ();
    x = (z - m_GlobalPar[ 2 ]) / m_GlobalPar[ 3 ];
    y = m_GlobalPar[ 0 ] + x * m_GlobalPar[ 1 ];

    dx = x - hit->getPositionX();
    dy = y - hit->getPositionY();
    dz = 0.;

    distance = sqrt(dx * dx + dy * dy + dz * dz);


    //here get the resolustion of a hit, repeated several times, ugly. should we store this in KLMHit2d object ?
    double hit_xErr = (eklmGeo->getStripGeometry()->getWidth()) * (Unit::cm / CLHEP::cm) *
                      (hit->getXStripMax() - hit->getXStripMin()) / sqrt(12);
    double hit_yErr = (eklmGeo->getStripGeometry()->getWidth()) * (Unit::cm / CLHEP::cm) *
                      (hit->getYStripMax() - hit->getYStripMin()) / sqrt(12);


    //error from tracking is ignored here
    error = sqrt(pow(hit_xErr, 2) +
                 pow(hit_yErr, 2));
  } //end of EKLM section
  else {
    B2WARNING("KLMTracking::distanceToHit Received KLMHit2d that's not from E/B-KLM. Setting distance to -1");
    distance = -1.;
  }


  if (error != 0.0) {
    sigma = distance / error;
  } else {
    sigma = DBL_MAX;
  }

  return distance;

}

