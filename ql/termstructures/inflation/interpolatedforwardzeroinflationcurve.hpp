/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007, 2008 Chris Kenyon
 Copyright (C) 2009 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file interpolatedforwardzeroinflationcurve.hpp
    \brief Zero-inflation term structure based on the interpolation of
           instantaneous forward inflation rates.
*/

#ifndef quantlib_interpolated_forward_zero_inflation_curve_hpp
#define quantlib_interpolated_forward_zero_inflation_curve_hpp

#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <utility>

namespace QuantLib {

    //! Zero-inflation term structure based on interpolated instantaneous
    //! forward inflation rates.
    /*!  The zero-coupon inflation rate z(T) is recovered from the stored
         instantaneous forward inflation rates f(t) by integration:
         \f[
             z(T) = \frac{1}{T} \int_0^T f(t)\, dt
         \f]
         This is the exact inflation analogue of InterpolatedForwardCurve in
         the yield-curve world.  Using BackwardFlat (the default) gives
         piecewise-constant instantaneous forward inflation rates, which
         implies a step-function forward curve and a linearly-interpolated
         (in time-weighted sense) zero-coupon curve between pillars.

        \ingroup inflationtermstructures
    */
    template <class Interpolator>
    class InterpolatedForwardZeroInflationCurve
        : public ZeroInflationTermStructure,
          protected InterpolatedCurve<Interpolator> {
      public:
        InterpolatedForwardZeroInflationCurve(
            const Date& referenceDate,
            std::vector<Date> dates,
            const std::vector<Rate>& forwards,
            Frequency frequency,
            const DayCounter& dayCounter,
            const ext::shared_ptr<Seasonality>& seasonality = {},
            const Interpolator& interpolator = Interpolator());

        //! \name InflationTermStructure interface
        //@{
        Date maxDate() const override;
        //@}

        //! \name Inspectors
        //@{
        const std::vector<Date>& dates() const;
        const std::vector<Time>& times() const;
        const std::vector<Real>& data() const;
        const std::vector<Rate>& forwardRates() const;
        std::vector<std::pair<Date, Rate> > nodes() const;
        //@}

      protected:
        //! \name ZeroInflationTermStructure interface
        //@{
        Rate zeroRateImpl(Time t) const override;
        //@}
        mutable std::vector<Date> dates_;

        /*! Protected constructor for use by descendant classes that
            cannot provide pillar data at construction time (e.g.
            PiecewiseZeroForwardInflationCurve).
        */
        InterpolatedForwardZeroInflationCurve(
            const Date& referenceDate,
            Date baseDate,
            Frequency frequency,
            const DayCounter& dayCounter,
            const ext::shared_ptr<Seasonality>& seasonality = {},
            const Interpolator& interpolator = Interpolator());
    };

    //! Zero-inflation term structure with backward-flat instantaneous
    //! forward rates (piecewise-constant forward inflation curve).
    typedef InterpolatedForwardZeroInflationCurve<BackwardFlat>
        ForwardZeroInflationCurve;


    // template definitions

    template <class Interpolator>
    InterpolatedForwardZeroInflationCurve<Interpolator>::
    InterpolatedForwardZeroInflationCurve(
        const Date& referenceDate,
        std::vector<Date> dates,
        const std::vector<Rate>& forwards,
        Frequency frequency,
        const DayCounter& dayCounter,
        const ext::shared_ptr<Seasonality>& seasonality,
        const Interpolator& interpolator)
    : ZeroInflationTermStructure(referenceDate, dates.at(0), frequency,
                                  dayCounter, seasonality),
      InterpolatedCurve<Interpolator>(std::vector<Time>(), forwards, interpolator),
      dates_(std::move(dates)) {

        QL_REQUIRE(dates_.size() > 1, "too few dates: " << dates_.size());

        QL_REQUIRE(this->data_.size() == dates_.size(),
                   "forwards/dates count mismatch: "
                   << this->data_.size() << " vs " << dates_.size());

        for (Size i = 1; i < dates_.size(); i++) {
            QL_REQUIRE(this->data_[i] > -1.0,
                       "forward inflation rate[" << i << "] < -100%");
        }

        this->setupTimes(dates_, referenceDate, dayCounter);
        this->setupInterpolation();
        this->interpolation_.update();
    }

    template <class Interpolator>
    InterpolatedForwardZeroInflationCurve<Interpolator>::
    InterpolatedForwardZeroInflationCurve(
        const Date& referenceDate,
        Date baseDate,
        Frequency frequency,
        const DayCounter& dayCounter,
        const ext::shared_ptr<Seasonality>& seasonality,
        const Interpolator& interpolator)
    : ZeroInflationTermStructure(referenceDate, baseDate, frequency,
                                  dayCounter, seasonality),
      InterpolatedCurve<Interpolator>(interpolator) {}


    template <class T>
    Date InterpolatedForwardZeroInflationCurve<T>::maxDate() const {
        if (this->maxDate_ != Date())
            return this->maxDate_;
        return dates_.back();
    }

    template <class T>
    Rate InterpolatedForwardZeroInflationCurve<T>::zeroRateImpl(Time t) const {
        if (t <= 0.0)
            return this->data_[0];

        const Time maxTime = this->times_.back();
        Real integral;
        if (t <= maxTime) {
            integral = this->interpolation_.primitive(t, true);
        } else {
            // flat instantaneous-forward extrapolation beyond last pillar
            integral = this->interpolation_.primitive(maxTime, true)
                     + this->data_.back() * (t - maxTime);
        }
        return integral / t;
    }

    template <class T>
    inline const std::vector<Time>&
    InterpolatedForwardZeroInflationCurve<T>::times() const {
        return this->times_;
    }

    template <class T>
    inline const std::vector<Date>&
    InterpolatedForwardZeroInflationCurve<T>::dates() const {
        return dates_;
    }

    template <class T>
    inline const std::vector<Rate>&
    InterpolatedForwardZeroInflationCurve<T>::forwardRates() const {
        return this->data_;
    }

    template <class T>
    inline const std::vector<Real>&
    InterpolatedForwardZeroInflationCurve<T>::data() const {
        return this->data_;
    }

    template <class T>
    inline std::vector<std::pair<Date, Rate> >
    InterpolatedForwardZeroInflationCurve<T>::nodes() const {
        std::vector<std::pair<Date, Rate> > results(dates_.size());
        for (Size i = 0; i < dates_.size(); ++i)
            results[i] = std::make_pair(dates_[i], this->data_[i]);
        return results;
    }

}

#endif
