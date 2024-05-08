/************************************************************************
 * Copyright(c) 2019, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:    VerticalSpread.hpp
 * Author:  raymond@burkholder.net
 * Project: TFOptionCombos
 * Created on June 11, 2019, 8:03 PM
 */

#pragma once

#include "ComboTraits.hpp"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace option { // option
namespace spread { // spread

namespace bear_call { // bear call

size_t LegCount();

void ChooseLegs( // throw Chain exceptions
  ComboTraits::EMarketDirection
, const mapChains_t& chains
, boost::gregorian::date
, const SpreadSpecs&
, double priceUnderlying
, const fLegSelected_t&&
);

void FillLegNote( size_t ix, ComboTraits::EMarketDirection, LegNote::values_t& );

std::string Name(
  ComboTraits::EMarketDirection
, const mapChains_t& chains
, boost::gregorian::date
, const SpreadSpecs&
, double price
, const std::string& sUnderlying
);

void AddLegOrder(
  const LegNote::Type
, pOrderCombo_t
, const ou::tf::OrderSide::EOrderSide
, uint32_t nOrderQuantity
, pPosition_t
);

void Bind( ComboTraits& traits );

} // namespace bear_call

// ====

namespace bull_put { // bull put

size_t LegCount();

void ChooseLegs( // throw Chain exceptions
  ComboTraits::EMarketDirection
, const mapChains_t& chains
, boost::gregorian::date
, const SpreadSpecs&
, double priceUnderlying
, const fLegSelected_t&&
);

void FillLegNote( size_t ix, ComboTraits::EMarketDirection, LegNote::values_t& );

std::string Name(
  ComboTraits::EMarketDirection
, const mapChains_t& chains
, boost::gregorian::date
, const SpreadSpecs&
, double price
, const std::string& sUnderlying
);

void AddLegOrder(
  const LegNote::Type
, pOrderCombo_t
, const ou::tf::OrderSide::EOrderSide
, uint32_t nOrderQuantity
, pPosition_t
);

void Bind( ComboTraits& traits );

} // namespace bull_put

} // namespace spread
} // namespace option
} // namespace tf
} // namespace ou
