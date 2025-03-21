/**************************************************************************
 * basf2 (Belle II Analysis Software Framework)                           *
 * Author: The Belle II Collaboration                                     *
 *                                                                        *
 * See git log for contributors and copyright holders.                    *
 * This file is licensed under LGPL-3.0, see LICENSE.md.                  *
 **************************************************************************/
#pragma once

#include <tracking/trackFindingCDC/filters/facet/BaseFacetFilter.h>

#include <string>

namespace Belle2 {


  namespace TrackFindingCDC {
    class CDCFacet;

    /// Filter for the construction of good facets based on simple criteria
    class SimpleFacetFilter : public BaseFacetFilter {

    private:
      /// Type of the super class
      using Super = BaseFacetFilter;

    public:
      /// Constructor using default direction of flight deviation cut off.
      SimpleFacetFilter();

      /// Constructor using given direction of flight deviation cut off.
      explicit SimpleFacetFilter(double deviationCosCut);

    public:
      /// Expose the set of parameters of the filter to the module parameter list.
      void exposeParameters(ModuleParamList* moduleParamList, const std::string& prefix) final;

    public:
      /**
       *  Main filter method returning the weight of the facet.
       *  Returns NAN if the cell shall be rejected.
       */
      Weight operator()(const CDCFacet& facet) final;

    private:
      /// Memory for the used direction of flight deviation.
      double m_param_deviationCosCut;
    };
  }
}
