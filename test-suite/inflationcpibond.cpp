/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2011 Chris Kenyon
 Copyright (C) 2012 StatPro Italia srl

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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/indexes/inflation/zacpi.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/calendars/southafrica.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflation/piecewisezeroinflationcurve.hpp>
#include <ql/termstructures/inflation/interpolatedforwardinflationcurve.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/instruments/zerocouponinflationswap.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/cashflows/cpicoupon.hpp>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/instruments/cpiswap.hpp>
#include <ql/instruments/bonds/cpibond.hpp>
#include <ql/cashflows/cashflows.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InflationCPIBondTests)

struct Datum {
    Date date;
    Rate rate;
};

typedef BootstrapHelper<ZeroInflationTermStructure> Helper;

std::vector<ext::shared_ptr<Helper> > makeHelpers(
        const std::vector<Datum>& iiData,
        const ext::shared_ptr<ZeroInflationIndex>& ii,
        const Period& observationLag,
        const Calendar& calendar,
        const BusinessDayConvention& bdc,
        const DayCounter& dc) {

    std::vector<ext::shared_ptr<Helper> > instruments;
    for (Datum datum : iiData) {
        Date maturity = datum.date;
        Handle<Quote> quote(ext::shared_ptr<Quote>(
                                new SimpleQuote(datum.rate/100.0)));
        auto h = ext::make_shared<ZeroCouponInflationSwapHelper>(
                quote, observationLag, maturity, calendar, bdc, dc, ii, CPI::Flat);
        instruments.push_back(h);
    }
    return instruments;
}


struct CommonVars { // NOLINT(cppcoreguidelines-special-member-functions)

    Calendar calendar;
    BusinessDayConvention convention;
    Date evaluationDate;
    Period observationLag;
    DayCounter dayCounter;

    ext::shared_ptr<UKRPI> ii;

    RelinkableHandle<YieldTermStructure> yTS;
    RelinkableHandle<ZeroInflationTermStructure> cpiTS;

    // setup
    CommonVars() {
        // usual setup
        calendar = UnitedKingdom();
        convention = ModifiedFollowing;
        Date today(25, November, 2009);
        evaluationDate = calendar.adjust(today);
        Settings::instance().evaluationDate() = evaluationDate;
        dayCounter = ActualActual(ActualActual::ISDA);

        ii = ext::make_shared<UKRPI>(cpiTS);

        Schedule rpiSchedule =
            MakeSchedule()
            .from(Date(1, July, 2007))
            .to(Date(1, September, 2009))
            .withFrequency(Monthly);

        Real fixData[] = {
            206.1, 207.3, 208.0, 208.9, 209.7, 210.9,
            209.8, 211.4, 212.1, 214.0, 215.1, 216.8,
            216.5, 217.2, 218.4, 217.7, 216.0, 212.9,
            210.1, 211.4, 211.3, 211.5, 212.8, 213.4,
            213.4, 213.4, 214.4
        };
        for (Size i=0; i<std::size(fixData); ++i) {
            ii->addFixing(rpiSchedule[i], fixData[i]);
        }

        yTS.linkTo(ext::shared_ptr<YieldTermStructure>(
                          new FlatForward(evaluationDate, 0.05, dayCounter)));

        // now build the zero inflation curve
        observationLag = Period(2,Months);

        std::vector<Datum> zciisData = {
            { Date(25, November, 2010), 3.0495 },
            { Date(25, November, 2011), 2.93 },
            { Date(26, November, 2012), 2.9795 },
            { Date(25, November, 2013), 3.029 },
            { Date(25, November, 2014), 3.1425 },
            { Date(25, November, 2015), 3.211 },
            { Date(25, November, 2016), 3.2675 },
            { Date(25, November, 2017), 3.3625 },
            { Date(25, November, 2018), 3.405 },
            { Date(25, November, 2019), 3.48 },
            { Date(25, November, 2021), 3.576 },
            { Date(25, November, 2024), 3.649 },
            { Date(26, November, 2029), 3.751 },
            { Date(27, November, 2034), 3.77225 },
            { Date(25, November, 2039), 3.77 },
            { Date(25, November, 2049), 3.734 },
            { Date(25, November, 2059), 3.714 },
        };

        std::vector<ext::shared_ptr<Helper> > helpers =
            makeHelpers(zciisData, ii,
                        observationLag, calendar, convention, dayCounter);

        Date baseDate = ii->lastFixingDate();

        cpiTS.linkTo(ext::make_shared<PiecewiseZeroInflationCurve<Linear>>(
                         evaluationDate, baseDate, ii->frequency(), dayCounter, helpers));
    }

    // teardown
    ~CommonVars() {
        // break circular references and allow curves to be destroyed
        cpiTS.reset();
    }
};


BOOST_AUTO_TEST_CASE(testCleanPrice) {
    BOOST_TEST_MESSAGE("Checking cached pricers for CPI bond...");

    CommonVars common;

    Real notional = 1000000.0;
    std::vector<Rate> fixedRates(1, 0.1);
    DayCounter fixedDayCount = Actual365Fixed();
    BusinessDayConvention fixedPaymentConvention = ModifiedFollowing;
    Calendar fixedPaymentCalendar = UnitedKingdom();
    ext::shared_ptr<ZeroInflationIndex> fixedIndex = common.ii;
    Period contractObservationLag = Period(3,Months);
    CPI::InterpolationType observationInterpolation = CPI::Flat;
    Natural settlementDays = 3;

    Real baseCPI = 206.1;
    // set the schedules
    Date startDate(2, October, 2007);
    Date endDate(2, October, 2052);
    Schedule fixedSchedule =
        MakeSchedule().from(startDate).to(endDate)
                      .withTenor(Period(6,Months))
                      .withCalendar(UnitedKingdom())
                      .withConvention(Unadjusted)
                      .backwards();

    CPIBond bond(settlementDays, notional,
                 baseCPI, contractObservationLag, fixedIndex,
                 observationInterpolation, fixedSchedule,
                 fixedRates, fixedDayCount, fixedPaymentConvention);

    auto engine = ext::make_shared<DiscountingBondEngine>(common.yTS);
    bond.setPricingEngine(engine);

    Real storedPrice = 396.47045891;
    Real calculated = bond.dirtyPrice();
    Real tolerance = 1.0e-8;
    if (std::fabs(calculated-storedPrice) > tolerance) {
        BOOST_FAIL("failed to reproduce expected CPI-bond dirty price"
                   << std::fixed << std::setprecision(12)
                   << "\n  expected:   " << storedPrice
                   << "\n  calculated: " << calculated);
    }

    storedPrice = 394.79676679;
    calculated = bond.cleanPrice();
    if (std::fabs(calculated-storedPrice) > tolerance) {
        BOOST_FAIL("failed to reproduce expected CPI-bond clean price"
                   << std::fixed << std::setprecision(12)
                   << "\n  expected:   " << storedPrice
                   << "\n  calculated: " << calculated);
    }
}

BOOST_AUTO_TEST_CASE(testCPILegWithoutBaseCPI) {
    BOOST_TEST_MESSAGE("Checking CPI leg with or without explicit base CPI fixing...");

    CommonVars common;

    Real notional = 1000000.0;
    std::vector<Rate> fixedRates(1, 0.1);
    DayCounter fixedDayCount = Actual365Fixed();
    BusinessDayConvention fixedPaymentConvention = ModifiedFollowing;
    Calendar fixedPaymentCalendar = UnitedKingdom();
    ext::shared_ptr<ZeroInflationIndex> fixedIndex = common.ii;
    Period contractObservationLag = Period(3, Months);
    CPI::InterpolationType observationInterpolation = CPI::Flat;
    Natural settlementDays = 3;
    bool growthOnly = false;
    Real baseCPI = 206.1;
    // set the schedules
    Date baseDate(1, July, 2007);
    Date startDate(2, October, 2007);
    Date endDate(2, October, 2052);
    Schedule fixedSchedule = MakeSchedule()
                                 .from(startDate)
                                 .to(endDate)
                                 .withTenor(Period(6, Months))
                                 .withCalendar(fixedPaymentCalendar)
                                 .withConvention(Unadjusted)
                                 .backwards();

    Leg legWithBaseDate = CPILeg(fixedSchedule, fixedIndex, Null<Real>(), contractObservationLag)
                              .withSubtractInflationNominal(growthOnly)
                              .withNotionals(notional)
                              .withBaseDate(baseDate)
                              .withFixedRates(fixedRates)
                              .withPaymentDayCounter(fixedDayCount)
                              .withObservationInterpolation(observationInterpolation)
                              .withPaymentAdjustment(fixedPaymentConvention)
                              .withPaymentCalendar(fixedPaymentCalendar);

    Leg legWithBaseCPI = CPILeg(fixedSchedule, fixedIndex, baseCPI, contractObservationLag)
                             .withSubtractInflationNominal(growthOnly)
                             .withNotionals(notional)
                             .withFixedRates(fixedRates)
                             .withPaymentDayCounter(fixedDayCount)
                             .withObservationInterpolation(observationInterpolation)
                             .withPaymentAdjustment(fixedPaymentConvention)
                             .withPaymentCalendar(fixedPaymentCalendar);

    Date settlementDate = fixedPaymentCalendar.advance(common.evaluationDate, settlementDays * Days,
                                                       fixedPaymentConvention);

    Real npvWithBaseDate =
        CashFlows::npv(legWithBaseDate, **common.yTS, false, settlementDate, settlementDate);
    Real accruedsBaseDate = CashFlows::accruedAmount(legWithBaseDate, false, settlementDate);

    Real npvWithBaseCPI =
        CashFlows::npv(legWithBaseCPI, **common.yTS, false, settlementDate, settlementDate);
    Real accruedsBaseCPI = CashFlows::accruedAmount(legWithBaseCPI, false, settlementDate);


    Real cleanPriceWithBaseDate = (npvWithBaseDate - accruedsBaseDate) * 100. / notional;
    Real cleanPriceWithBaseCPI = (npvWithBaseCPI - accruedsBaseCPI) * 100. / notional;

    Real tolerance = 1.0e-8;
    if (std::fabs(cleanPriceWithBaseDate - cleanPriceWithBaseCPI) > tolerance) {
        BOOST_FAIL("prices of CPI leg with base date and explicit base CPI fixing are not equal "
                   << std::fixed << std::setprecision(12)
                   << "\n  clean npv of leg with baseDate:   " << cleanPriceWithBaseDate
                   << "\n clean npv of leg with explicit baseCPI: " << cleanPriceWithBaseCPI);
    }
    // Compare to expected price
    Real storedPrice = 394.79676680;
    if (std::fabs(cleanPriceWithBaseDate - storedPrice) > tolerance) {
        BOOST_FAIL("failed to reproduce expected CPI-bond clean price"
                   << std::fixed << std::setprecision(12) << "\n  expected:   " << storedPrice
                   << "\n  calculated: " << cleanPriceWithBaseDate);
    }
}

BOOST_AUTO_TEST_CASE(testI2050ForwardInflationCurve) {
    BOOST_TEST_MESSAGE("Valuing SA government I2050 CPI bond using "
                       "InterpolatedForwardInflationCurve...");

    // --- Evaluation date ---
    Calendar calendar = SouthAfrica();
    Date today(14, April, 2026);
    Date evaluationDate = calendar.adjust(today);
    Settings::instance().evaluationDate() = evaluationDate;

    DayCounter dc = Actual365Fixed();
    DayCounter bondDC = ActualActual(ActualActual::Bond);

    // --- ZACPI index: 1-month observation lag, monthly frequency ---
    RelinkableHandle<ZeroInflationTermStructure> cpiHandle;
    auto zacpi = ext::make_shared<ZACPI>(cpiHandle);

    // Recent monthly fixings (dated last day of each reference month)
    zacpi->addFixing(Date(30, November, 2025), 103.4);
    zacpi->addFixing(Date(31, December, 2025), 103.6);
    zacpi->addFixing(Date(31, January,  2026), 103.8);
    zacpi->addFixing(Date(28, February, 2026), 104.2);

    // --- Nominal yield curve: InterpolatedForwardCurve<BackwardFlat> ---
    // Reference date = evaluationDate; first segment rate matches the Jul 14 pillar
    // so that discount factors from settlement (Apr 17 = +3BD) match Bloomberg exactly.
    // Pillar dates and continuously-compounded (NACC) forward rates in %
    std::vector<Date> nomDates = {
        Date(14, April,   2026),
        Date(15, April,   2026),
        Date(14, July,    2026),
        Date(14, October, 2026),
        Date(14, January, 2027),
        Date(14, April,   2027),
        Date(31, January, 2030),
        Date(31, March,   2032),
        Date(28, February,2035),
        Date( 2, February,2037),
        Date(31, January, 2040),
        Date( 1, February,2044),
        Date(28, February,2048),
        Date(31, March,   2053),
        Date(31, December,2055),
        Date(31, December,2060),
    };
    std::vector<Rate> nomFwds = {
        6.559410569 / 100.0,
        6.559410569 / 100.0,
        6.810905617 / 100.0,
        7.929858912 / 100.0,
        7.811823285 / 100.0,
        7.522586781 / 100.0,
        7.638835351 / 100.0,
        8.709441317 / 100.0,
        9.147473075 / 100.0,
       10.63271018  / 100.0,
       10.87614446  / 100.0,
        9.722351512 / 100.0,
        8.310534995 / 100.0,
        7.778595401 / 100.0,
        5.2         / 100.0,
        7.4         / 100.0,
    };

    RelinkableHandle<YieldTermStructure> nominalTS;
    nominalTS.linkTo(ext::make_shared<ForwardCurve>(nomDates, nomFwds, dc, calendar));

    // --- Inflation forward curve: InterpolatedForwardInflationCurve<BackwardFlat> ---
    // Pillar dates and NACC instantaneous forward inflation rates in %
    std::vector<Date> cpiDates = {
        Date(1,  February, 2026),
        Date(1,  December, 2027),
        Date(1,  January,  2029),
        Date(1,  November, 2032),
        Date(1,  September,2033),
        Date(1,  October,  2037),
        Date(1,  January,  2046),
        Date(1,  October,  2050),
        Date(1,  December, 2052),
        Date(1,  January,  2056),  // UPR 2154 pillar
        Date(1,  September,2060),
    };
    std::vector<Rate> cpiFwds = {
        4.167219216 / 100.0,
        4.167219216 / 100.0,
        3.44340625  / 100.0,
        4.273729518 / 100.0,
        4.241394049 / 100.0,
        6.131479915 / 100.0,
        5.543215177 / 100.0,
        4.273784232 / 100.0,
        4.778595401 / 100.0,
        4.4         / 100.0,  // UPR 2154 pillar
        4.0         / 100.0,
    };

    Period observationLag(4, Months);

    auto cpiCurve = ext::make_shared<ForwardInflationCurve>(
        Date(1,  February, 2026), cpiDates, cpiFwds, Monthly, dc);
    cpiHandle.linkTo(cpiCurve);

    // --- I2050 bond ---
    Natural settlementDays = 3;
    Real notional         = 100.0;
    Real baseCPI          = 53.88458726;
    Rate couponRate       = 0.025;

    Date startDate(16, July, 2012);
    Date maturityDate(31, December, 2050);

    Schedule schedule = MakeSchedule()
        .from(startDate)
        .to(maturityDate)
        .withTenor(Period(6, Months))
        .withCalendar(calendar)
        .withConvention(Following)
        .backwards();

    CPIBond bond(settlementDays, notional,
                 baseCPI, observationLag, zacpi,
                 CPI::Linear, schedule,
                 std::vector<Rate>(1, couponRate),
                 bondDC, Following);

    auto engine = ext::make_shared<DiscountingBondEngine>(nominalTS);
    bond.setPricingEngine(engine);

    Real tolerance = 1.0e-5;
    Real calculated = bond.dirtyPrice();
    Real expected   = 146.08008606771900;
    BOOST_TEST_MESSAGE("I2050 dirty price"
        << std::fixed << std::setprecision(10)
        << "  expected="   << expected
        << "  calculated=" << calculated
        << "  diff="       << calculated - expected);
    if (std::fabs(calculated - expected) > tolerance)
        BOOST_FAIL("I2050 dirty price mismatch"
            << std::fixed << std::setprecision(10)
            << "\n  expected:   " << expected
            << "\n  calculated: " << calculated
            << "\n  difference: " << std::fabs(calculated - expected));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
