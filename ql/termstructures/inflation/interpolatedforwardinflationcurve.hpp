/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 Francois Botha


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

/*! \file interpolatedforwardinflationcurve.hpp
    \brief Forward-inflation term structure based on the interpolation of
           instantaneous forward inflation rates (NACC convention).

    CPI growth is modelled as exp(integral of f), NOT (1+z)^t.
*/

#ifndef quantlib_interpolated_forward_inflation_curve_hpp
#define quantlib_interpolated_forward_inflation_curve_hpp

#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <cmath>
#include <utility>

namespace QuantLib {

    //! Inflation term structure based on interpolated NACC instantaneous
    //! forward rates, with CPI projection using exp(z*t).
    /*!  Stores NACC instantaneous forward inflation rates f(t) on a
         set of pillar dates.  The zero-coupon rate is recovered by integration:
         \f[
             z(T) = \frac{1}{T} \int_0^T f(t)\, dt
         \f]
         and the CPI growth factor is
         \f[
             \text{cpiRatio}(T) = \exp(z(T) \cdot T) = \exp\!\left(\int_0^T f(t)\, dt\right)
         \f]

         This class inherits from ForwardInflationTermStructure (which in turn
         inherits from ZeroInflationTermStructure), so it can be held in a
         Handle<ZeroInflationTermStructure>.

         ZeroInflationIndex::forecastFixing detects this type via dynamic_cast
         and uses exp(z*t) instead of pow(1+z, t).

        \ingroup inflationtermstructures
    */
    template <class Interpolator>
    class InterpolatedForwardInflationCurve
        : public ForwardInflationTermStructure,
          protected InterpolatedCurve<Interpolator> {
      public:
        InterpolatedForwardInflationCurve(
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
        //! \name ForwardInflationTermStructure interface
        //@{
        Rate forwardRateImpl(Time t) const override;
        //@}
        //! Override zeroRateImpl to use exact interpolation primitive.
        Rate zeroRateImpl(Time t) const override;
        //! Override cpiRatio to use primitive(t_d) - primitive(t_base) directly.
        Real cpiRatio(Time t, bool extrapolate = false) const override;

        mutable std::vector<Date> dates_;

        /*! Protected constructor for use by descendant classes that
            cannot provide pillar data at construction time (e.g.
            PiecewiseZeroForwardInflationCurve).
        */
        InterpolatedForwardInflationCurve(
            const Date& referenceDate,
            Date baseDate,
            Frequency frequency,
            const DayCounter& dayCounter,
            const ext::shared_ptr<Seasonality>& seasonality = {},
            const Interpolator& interpolator = Interpolator());
    };

    //! Forward-inflation curve with backward-flat instantaneous forward rates.
    typedef InterpolatedForwardInflationCurve<BackwardFlat>
        ForwardInflationCurve;


    // template definitions

    template <class Interpolator>
    InterpolatedForwardInflationCurve<Interpolator>::
    InterpolatedForwardInflationCurve(
        const Date& referenceDate,
        std::vector<Date> dates,
        const std::vector<Rate>& forwards,
        Frequency frequency,
        const DayCounter& dayCounter,
        const ext::shared_ptr<Seasonality>& seasonality,
        const Interpolator& interpolator)
    : ForwardInflationTermStructure(referenceDate, dates.at(0), frequency,
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
    InterpolatedForwardInflationCurve<Interpolator>::
    InterpolatedForwardInflationCurve(
        const Date& referenceDate,
        Date baseDate,
        Frequency frequency,
        const DayCounter& dayCounter,
        const ext::shared_ptr<Seasonality>& seasonality,
        const Interpolator& interpolator)
    : ForwardInflationTermStructure(referenceDate, baseDate, frequency,
                                    dayCounter, seasonality),
      InterpolatedCurve<Interpolator>(interpolator) {}


    template <class T>
    Date InterpolatedForwardInflationCurve<T>::maxDate() const {
        if (this->maxDate_ != Date())
            return this->maxDate_;
        return dates_.back();
    }

    template <class T>
    Rate InterpolatedForwardInflationCurve<T>::forwardRateImpl(Time t) const {
        return this->interpolation_(t, true);
    }

    template <class T>
    Rate InterpolatedForwardInflationCurve<T>::zeroRateImpl(Time t) const {
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
    Real InterpolatedForwardInflationCurve<T>::cpiRatio(Time t, bool extrapolate) const {
        // t is timeFromReference(d).  We need exp(integral of f from baseDate to d).
        // = exp(primitive(t) - primitive(t_base))
        // where t_base = timeFromReference(baseDate()).
        checkRange(t, extrapolate);

        Time t_base = this->times_[0];  // timeFromReference(baseDate) = first pillar time

        if (t <= t_base)
            return 1.0;

        const Time maxTime = this->times_.back();

        auto primitive = [&](Time u) -> Real {
            if (u <= maxTime)
                return this->interpolation_.primitive(u, true);
            else
                return this->interpolation_.primitive(maxTime, true)
                     + this->data_.back() * (u - maxTime);
        };

        return std::exp(primitive(t) - primitive(t_base));
    }

    template <class T>
    inline const std::vector<Time>&
    InterpolatedForwardInflationCurve<T>::times() const {
        return this->times_;
    }

    template <class T>
    inline const std::vector<Date>&
    InterpolatedForwardInflationCurve<T>::dates() const {
        return dates_;
    }

    template <class T>
    inline const std::vector<Rate>&
    InterpolatedForwardInflationCurve<T>::forwardRates() const {
        return this->data_;
    }

    template <class T>
    inline const std::vector<Real>&
    InterpolatedForwardInflationCurve<T>::data() const {
        return this->data_;
    }

    template <class T>
    inline std::vector<std::pair<Date, Rate> >
    InterpolatedForwardInflationCurve<T>::nodes() const {
        std::vector<std::pair<Date, Rate> > results(dates_.size());
        for (Size i = 0; i < dates_.size(); ++i)
            results[i] = std::make_pair(dates_[i], this->data_[i]);
        return results;
    }

}

#endif
