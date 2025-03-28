/**************************************************************************
 * basf2 (Belle II Analysis Software Framework)                           *
 * Author: The Belle II Collaboration                                     *
 *                                                                        *
 * See git log for contributors and copyright holders.                    *
 * This file is licensed under LGPL-3.0, see LICENSE.md.                  *
 **************************************************************************/

#include <analysis/utility/PCmsLabTransform.h>

#include <analysis/modules/EventShapeCalculator/EventShapeCalculatorModule.h>

#include <analysis/dataobjects/ParticleList.h>
#include <analysis/dataobjects/Particle.h>
#include <mdst/dataobjects/MCParticle.h>

#include <framework/logging/Logger.h>

#include <analysis/ContinuumSuppression/Thrust.h>
#include <analysis/ContinuumSuppression/HarmonicMoments.h>
#include <analysis/ContinuumSuppression/CleoCones.h>
#include <analysis/ContinuumSuppression/FoxWolfram.h>
#include <analysis/ContinuumSuppression/SphericityEigenvalues.h>

#include <Math/Vector4D.h>


using namespace std;
using namespace Belle2;

//-----------------------------------------------------------------
//                 Register the Module
//-----------------------------------------------------------------
REG_MODULE(EventShapeCalculator);

//-----------------------------------------------------------------
//                 Implementation
//-----------------------------------------------------------------

EventShapeCalculatorModule::EventShapeCalculatorModule() : Module()
{
  // Set module properties
  setDescription("Module to compute event shape attributes starting from particlelists. The core algorithms are not implemented in this module, but in dedicated basf2 classes.");
  setPropertyFlags(c_ParallelProcessingCertified);
  // Parameter definitions
  addParam("particleListNames", m_particleListNames, "List of the ParticleLists to be used for the calculation of the EventShapes.",
           vector<string>());
  addParam("enableThrust", m_enableThrust, "Enables the calculation of thrust-related quantities.", true);
  addParam("enableCollisionAxis", m_enableCollisionAxis, "Enables the calculation of the  quantities related to the collision axis.",
           true);
  addParam("enableFoxWolfram", m_enableFW, "Enables the calculation of the Fox-Wolfram moments.", true);
  addParam("enableHarmonicMoments", m_enableHarmonicMoments, "Enables the calculation of the Harmonic moments.", true);
  addParam("enableJets", m_enableJets, "Enables the calculation of jet-related quantities.", true);
  addParam("enableSphericity", m_enableSphericity, "Enables the calculation of the sphericity-related quantities.", true);
  addParam("enableCleoCones", m_enableCleoCones, "Enables the calculation of the CLEO cones.", true);
  addParam("enableAllMoments", m_enableAllMoments, "Enables the calculation of FW and harmonic moments from 5 to 8", false);
  addParam("checkForDuplicates", m_checkForDuplicates,
           "Enables the check for duplicates in the input list. If a duplicate entry is found, the first one is kept.", false);
}


void EventShapeCalculatorModule::initialize()
{
  m_eventShapeContainer.registerInDataStore();
  if (m_enableJets and not m_enableThrust) {
    B2WARNING("The jet-related quantities can only be calculated if the thrust calculation is activated as well.");
    m_enableThrust = true;
  }
  if (m_enableCleoCones and not(m_enableThrust or m_enableCollisionAxis))
    B2WARNING("The CLEO cones can only be calculated if either the thrust or the collision axis calculation are activated as well.");
  if (m_enableHarmonicMoments and not(m_enableThrust or m_enableCollisionAxis))
    B2WARNING("The harmonic moments can only be calculated if either the thrust or the collision axis calculation are activated as well.");
}


void EventShapeCalculatorModule::event()
{

  PCmsLabTransform T;
  double sqrtS = T.getCMSEnergy();

  if (!m_eventShapeContainer) m_eventShapeContainer.create();

  const int nPart = parseParticleLists(m_particleListNames);
  if (nPart == 0) return;

  // --------------------
  // Calculates the FW moments
  // --------------------
  if (m_enableFW) {
    FoxWolfram fw(m_p4List);
    if (m_enableAllMoments) {
      fw.calculateAllMoments();
      for (short i = 0; i < 9; i++) {
        m_eventShapeContainer->setFWMoment(i, fw.getH(i));
      }
    } else {
      fw.calculateBasicMoments();
      for (short i = 0; i < 5; i++) {
        m_eventShapeContainer->setFWMoment(i, fw.getH(i));
      }
    }
  }

  // --------------------
  // Calculates the sphericity quantities
  // --------------------
  if (m_enableSphericity) {
    SphericityEigenvalues Sph(m_p4List);
    Sph.calculateEigenvalues();
    if (Sph.getEigenvalue(0) < Sph.getEigenvalue(1) || Sph.getEigenvalue(0) < Sph.getEigenvalue(2)
        || Sph.getEigenvalue(1) < Sph.getEigenvalue(2))
      B2WARNING("Eigenvalues not ordered!!!!!!!!!!");

    for (short i = 0; i < 3; i++) {
      m_eventShapeContainer->setSphericityEigenvalue(i, Sph.getEigenvalue(i));
      m_eventShapeContainer->setSphericityEigenvector(i, Sph.getEigenvector(i));
    }
  }


  // --------------------
  // Calculates thrust and thrust-related quantities
  // --------------------
  if (m_enableThrust) {
    ROOT::Math::XYZVector thrust = Thrust::calculateThrust(m_p4List);
    float thrustVal = thrust.R();
    thrust = thrust.Unit();
    m_eventShapeContainer->setThrustAxis(thrust);
    m_eventShapeContainer->setThrust(thrustVal);

    // --- If required, calculates the HarmonicMoments ---
    if (m_enableHarmonicMoments) {
      HarmonicMoments MM(m_p4List, thrust);
      if (m_enableAllMoments) {
        MM.calculateAllMoments();
        for (short i = 0; i < 9; i++) {
          auto moment = MM.getMoment(i, sqrtS);
          m_eventShapeContainer->setHarmonicMomentThrust(i, moment);
        }
      } else {
        MM.calculateBasicMoments();
        for (short i = 0; i < 5; i++) {
          auto moment = MM.getMoment(i, sqrtS);
          m_eventShapeContainer->setHarmonicMomentThrust(i, moment);
        }
      }
    }

    // --- If required, calculates the cleo cones w/ respect to the thrust axis ---
    if (m_enableCleoCones) {
      // Cleo cone class constructor. Unfortunately this class is designed
      // to use the ROE, so the constructor takes two std::vector of momenta ("all" and "ROE"),
      // then a vector to be used as axis, and finally two flags that determine if the cleo cones
      // are calculated using the ROE, all the particles or both. Here we use the m_p4List as dummy
      // list of the ROE momenta, that is however not used at all since the calculate only the
      // cones with all the particles. This whole class would need some heavy restructuring...
      CleoCones cleoCones(m_p4List, m_p4List, thrust, true, false);
      std::vector<float> cones;
      cones = cleoCones.cleo_cone_with_all();
      for (short i = 0; i < 10; i++) {
        m_eventShapeContainer->setCleoConeThrust(i, cones[i]);
      }
    } // end of if m_enableCleoCones


    // --- If required, calculates the jet 4-momentum using the thrust axis ---
    if (m_enableJets) {
      ROOT::Math::PxPyPzEVector p4FWD;
      ROOT::Math::PxPyPzEVector p4BKW;
      for (const auto& p4 : m_p4List) {
        if (p4.Vect().Dot(thrust) > 0)
          p4FWD += p4;
        else
          p4BKW += p4;
      }
      m_eventShapeContainer->setForwardHemisphere4Momentum(p4FWD);
      m_eventShapeContainer->setBackwardHemisphere4Momentum(p4BKW);
    } // end of if m_enableJets
  }// end of if m_enableThrust



  // --------------------
  // Calculates the collision axis quantities
  // --------------------
  if (m_enableCollisionAxis) {

    ROOT::Math::XYZVector collisionAxis(0., 0., 1.);

    // --- If required, calculates the cleo cones w/ respect to the collision axis ---
    if (m_enableCleoCones) {
      CleoCones cleoCones(m_p4List, m_p4List, collisionAxis, true, false);
      std::vector<float> cones;
      cones = cleoCones.cleo_cone_with_all();
      for (short i = 0; i < 10; i++) {
        m_eventShapeContainer->setCleoConeCollision(i, cones[i]);
      }
    }

    // --- If required, calculates the HarmonicMoments ---
    if (m_enableHarmonicMoments) {
      HarmonicMoments MM(m_p4List, collisionAxis);
      if (m_enableAllMoments) {
        MM.calculateAllMoments();
        for (short i = 0; i < 9; i++) {
          auto moment = MM.getMoment(i, sqrtS);
          m_eventShapeContainer->setHarmonicMomentCollision(i, moment);
        }
      } else {
        MM.calculateBasicMoments();
        for (short i = 0; i < 5; i++) {
          auto moment = MM.getMoment(i, sqrtS);
          m_eventShapeContainer->setHarmonicMomentCollision(i, moment);
        }
      }
    } // end of m_enableHarmonicMoments
  } // end of m_enableCollisionAxis
} // end of event()



int EventShapeCalculatorModule::parseParticleLists(vector<string> particleListNames)
{
  PCmsLabTransform T;
  m_p4List.clear();

  unsigned int nParticlesInAllLists = 0;
  unsigned short nParticleLists = particleListNames.size();
  if (nParticleLists == 0)
    B2WARNING("No particle lists found. EventShape calculation not performed.");

  // This vector temporarily stores the mdstSource of particle objects
  // that have been processed so far (not only the momenta)
  // in order to check for duplicates before pushing the 3- and 4- vectors
  // in the corresponding lists
  std::vector<int> usedMdstSources;

  // Loops over the number of particle lists
  for (unsigned short iList = 0; iList < nParticleLists; iList++) {
    string particleListName = particleListNames[iList];
    StoreObjPtr<ParticleList> particleList(particleListName);

    // Loops over the number of particles in the list
    nParticlesInAllLists += particleList->getListSize();

    for (unsigned int iPart = 0; iPart < particleList->getListSize(); iPart++) {
      const Particle* part = particleList->getParticle(iPart);
      const MCParticle* mcParticle = part->getMCParticle();
      if (mcParticle and mcParticle->isInitial()) continue;

      if (m_checkForDuplicates) {

        std::vector<const Belle2::Particle*> finalStateDaughters = part->getFinalStateDaughters();

        for (const auto fsp : finalStateDaughters) {
          int mdstSource = fsp->getMdstSource();
          auto result = std::find(usedMdstSources.begin(), usedMdstSources.end(), mdstSource);
          if (result == usedMdstSources.end()) {
            usedMdstSources.push_back(mdstSource);
            ROOT::Math::PxPyPzEVector p4CMS = T.rotateLabToCms() * fsp->get4Vector();
            m_p4List.push_back(p4CMS);
            B2DEBUG(19, "non-duplicate has pdgCode " << fsp->getPDGCode() << " and mdstSource " << mdstSource);
          } else {
            B2DEBUG(19, "duplicate has pdgCode " << fsp->getPDGCode() << " and mdstSource " << mdstSource);
            B2DEBUG(19, "Duplicate particle found. The new one won't be used for the calculation of the event shape variables. "
                    "Please, double check your input lists and try to make them mutually exclusive.");
          }
        }
      } else {
        ROOT::Math::PxPyPzEVector p4CMS = T.rotateLabToCms() * part->get4Vector();
        m_p4List.push_back(p4CMS);
      }
    }
  }

  return nParticlesInAllLists;
}

