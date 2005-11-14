/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Ferdinando Ametrano
 Copyright (C) 2000-2005 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file pathgenerator.hpp
    \brief Generates random paths using a sequence generator
*/

#ifndef quantlib_montecarlo_path_generator_hpp
#define quantlib_montecarlo_path_generator_hpp

#include <ql/stochasticprocess.hpp>
#include <ql/MonteCarlo/brownianbridge.hpp>

namespace QuantLib {

    //! Generates random paths using a sequence generator
    /*! Generates random paths with drift(S,t) and variance(S,t)
        using a gaussian sequence generator

        \ingroup mcarlo

        \test the generated paths are checked against cached results
    */
    template <class GSG>
    class PathGenerator {
      public:
        typedef Sample<Path> sample_type;
        // constructors
        PathGenerator(const boost::shared_ptr<StochasticProcess>&,
                      Time length,
                      Size timeSteps,
                      const GSG& generator,
                      bool brownianBridge);
        PathGenerator(const boost::shared_ptr<StochasticProcess>&,
                      const TimeGrid& timeGrid,
                      const GSG& generator,
                      bool brownianBridge);
        //! \name inspectors
        //@{
        const sample_type& next() const;
        const sample_type& antithetic() const;
        Size size() const { return dimension_; }
        const TimeGrid& timeGrid() const { return timeGrid_; }
        //@}
      private:
        const sample_type& next(bool antithetic) const;
        bool brownianBridge_;
        GSG generator_;
        Size dimension_;
        TimeGrid timeGrid_;
        boost::shared_ptr<StochasticProcess1D> process_;
        mutable sample_type next_;
        BrownianBridge<GSG> bb_;
    };


    // template definitions

    template <class GSG>
    PathGenerator<GSG>::PathGenerator(
                          const boost::shared_ptr<StochasticProcess>& process,
                          Time length,
                          Size timeSteps,
                          const GSG& generator,
                          bool brownianBridge)
    : brownianBridge_(brownianBridge), generator_(generator),
      dimension_(generator_.dimension()), timeGrid_(length, timeSteps),
      process_(boost::dynamic_pointer_cast<StochasticProcess1D>(process)),
      next_(Path(timeGrid_),1.0), bb_(process_, timeGrid_, generator_) {
        QL_REQUIRE(dimension_==timeSteps,
                   "sequence generator dimensionality (" << dimension_
                   << ") != timeSteps (" << timeSteps << ")");
    }

    template <class GSG>
    PathGenerator<GSG>::PathGenerator(
                          const boost::shared_ptr<StochasticProcess>& process,
                          const TimeGrid& timeGrid,
                          const GSG& generator,
                          bool brownianBridge)
    : brownianBridge_(brownianBridge), generator_(generator),
      dimension_(generator_.dimension()), timeGrid_(timeGrid),
      process_(boost::dynamic_pointer_cast<StochasticProcess1D>(process)),
      next_(Path(timeGrid_),1.0), bb_(process_, timeGrid_, generator_) {
        QL_REQUIRE(dimension_==timeGrid_.size()-1,
                   "sequence generator dimensionality (" << dimension_
                   << ") != timeSteps (" << timeGrid_.size()-1 << ")");
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::next() const {
        return next(false);
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::antithetic() const {
        return next(true);
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::next(bool antithetic) const {

        if (brownianBridge_) {
            typedef typename BrownianBridge<GSG>::sample_type sequence_type;
            const sequence_type& stdDev_ =
                antithetic ? bb_.last() : bb_.next();

            next_.weight = stdDev_.weight;

            Path& path = next_.value;

            path.front() = process_->x0();

            Time t = timeGrid_[0];
            Time dt= timeGrid_.dt(0);
            Real diffusion = stdDev_.value[0];
            path[1] = process_->apply(process_->expectation(t,path.front(),dt),
                                      antithetic ? -diffusion : diffusion);

            for (Size i=2; i<path.length(); i++) {
                t = timeGrid_[i-1];
                dt = timeGrid_.dt(i-1);
                diffusion = stdDev_.value[i-1] - stdDev_.value[i-2];
                path[i] =
                    process_->apply(process_->expectation(t,path[i-1],dt),
                                    antithetic ? -diffusion : diffusion);
            }
            return next_;
        } else {
            typedef typename GSG::sample_type sequence_type;
            const sequence_type& sequence_ =
                antithetic ? generator_.lastSequence()
                           : generator_.nextSequence();

            next_.weight = sequence_.weight;

            Path& path = next_.value;

            // starting point for asset value
            path.front() = process_->x0();

            for (Size i=1; i<path.length(); i++) {
                Time t = timeGrid_[i-1];
                Time dt = timeGrid_.dt(i-1);
                path[i] = process_->evolve(t, path[i-1], dt,
                                           antithetic ?
                                               -sequence_.value[i-1] :
                                                sequence_.value[i-1]);
            }

            return next_;
        }
    }

}


#endif
