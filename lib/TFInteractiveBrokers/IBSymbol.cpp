/************************************************************************
 * Copyright(c) 2009, One Unified. All rights reserved.                 *
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

#include <OUCommon/TimeSource.h>

#include "IBSymbol.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace ib { // Interactive Brokers

Symbol::Symbol( inherited_t::idSymbol_t idSym, pInstrument_t pInstrument, TickerId idTicker )
:
  ou::tf::Symbol<Symbol>( pInstrument, idSym ),
    m_TickerId( idTicker ),
    m_bAskFound( false ), m_bAskSizeFound( false ),
    m_bBidFound( false ), m_bBidSizeFound( false ),
    m_bLastTimeStampFound( false ), m_bLastFound( false ), m_bLastSizeFound( false ),
    m_nAskSize( 0 ), m_nBidSize( 0 ), m_nLastSize( 0 ),
    m_dblAsk( 0 ), m_dblBid( 0 ), m_dblLast( 0 ),
    m_nVolume( 0 ),
    m_dblHigh( 0 ), m_dblLow( 0 ), m_dblClose( 0 ),
    m_bQuoteTradeWatchInProgress( false ), m_bDepthWatchInProgress( false ),
    m_dblOptionPrice( 0 ), m_dblUnderlyingPrice( 0 ), m_dblPvDividend( 0 )
{
  inherited_t::m_id = idSym;
}

Symbol::Symbol( pInstrument_t pInstrument, TickerId idTicker )
:
  ou::tf::Symbol<Symbol>( pInstrument ),
    m_TickerId( idTicker ),
    m_bAskFound( false ), m_bAskSizeFound( false ),
    m_bBidFound( false ), m_bBidSizeFound( false ),
    m_bLastTimeStampFound( false ), m_bLastFound( false ), m_bLastSizeFound( false ),
    m_nAskSize( 0 ), m_nBidSize( 0 ), m_nLastSize( 0 ),
    m_dblAsk( 0 ), m_dblBid( 0 ), m_dblLast( 0 ),
    m_nVolume( 0 ),
    m_dblHigh( 0 ), m_dblLow( 0 ), m_dblClose( 0 ),
    m_bQuoteTradeWatchInProgress( false ), m_bDepthWatchInProgress( false ),
    m_dblOptionPrice( 0 ), m_dblUnderlyingPrice( 0 ), m_dblPvDividend( 0 )
{
}

Symbol::~Symbol(void) {
}

void Symbol::AcceptTickPrice(TickType tickType, double price) {
  switch ( tickType ) {
    case TickType::BID:
      if ( price != m_dblBid ) {
        m_dblBid = price;
        m_bBidFound = true;
        BuildQuote();
      }
      break;
    case TickType::ASK:
      if ( price != m_dblAsk ) {
        m_dblAsk = price;
        m_bAskFound = true;
        BuildQuote();
      }
      break;
    case TickType::LAST:
      m_dblLast = price;
      m_bLastFound = true;
      BuildTrade();
      break;
    case TickType::HIGH:
      m_dblHigh = price;
//      std::cout << m_sSymbolName << " High " << price << std::endl;
      break;
    case TickType::LOW:
      m_dblLow = price;
//      std::cout << m_sSymbolName << " Low " << price << std::endl;
      break;
    case TickType::CLOSE:
      m_dblClose = price;
//      std::cout << m_sSymbolName << " Close " << price << std::endl;
      break;
  }
}

void Symbol::AcceptTickSize(TickType tickType, Decimal size_decimal) {

  // 2024/03/17 found with a BID_SIZE forex quote
  static const Decimal scary( 0x7c00000000000000 ); // 8935141660703064064

  uint32_t size;
  if ( scary == size_decimal ) {
    size = 0;
  }
  else {
    // go native at some point?  [high conversion overhead]
    uint32_t size = __bid64_to_uint32_rnint( size_decimal );
  }

  switch ( m_pInstrument->GetInstrumentType() ) {
  case InstrumentType::Stock:
  case InstrumentType::ETF:
    size *= 100;
    break;
  case InstrumentType::Currency:
    // any re-sizing?
    break;
  default:
    break;
  }

  switch ( tickType ) {
    case TickType::BID_SIZE:
      if ( size != m_nBidSize ) {
        m_nBidSize = size;
        m_bBidSizeFound = true;
        BuildQuote();
      }
      break;
    case TickType::ASK_SIZE:
      if ( size != m_nAskSize ) {
        m_nAskSize = size;
        m_bAskSizeFound = true;
        BuildQuote();
      }
      break;
    case TickType::LAST_SIZE:
      m_nLastSize = size;
      m_bLastSizeFound = true;
      BuildTrade();
      break;
    case TickType::VOLUME:
      m_nVolume = size;
      m_bLastFound = m_bLastSizeFound = false;  // reset flags to get in sync
      //BuildTrade();
      break;
  }
}

void Symbol::AcceptTickString(TickType tickType, const std::string& value) {
  switch ( tickType ) {
    case TickType::LAST_TIMESTAMP:
      m_bLastTimeStampFound = true;
      m_bLastFound = m_bLastSizeFound = false; // timestamp seems to lead the trade and size
      BuildTrade();
      break;
  }
}

void Symbol::BuildQuote() {
//  if ( m_bAskFound && m_bBidFound && m_bAskSizeFound && m_bBidSizeFound ) {
    if ( m_bAskFound || m_bBidFound ) {
    //boost::local_time::local_date_time ldt =
    //  boost::local_time::local_microsec_clock::local_time();
    Quote quote( ou::TimeSource::GlobalInstance().External(), m_dblBid, m_nBidSize, m_dblAsk, m_nAskSize );
    //std::cout << "Q:" << quote.m_dt << " "
    //  << quote.m_nBidSize << "@" << quote.m_dblBid << " "
    //  << quote.m_nAskSize << "@" << quote.m_dblAsk
    //  << std::endl;
    m_OnQuote( quote );
    // 2010-06-21 not sure if these flags should be reset
    //   basics are if Ask or Bid value changes, then emit regardless of Size
    //   size doesn't matter for now
    m_bAskFound = m_bBidFound = m_bAskSizeFound = m_bBidSizeFound = false;
  }
}

void Symbol::BuildTrade() {
  //if ( !m_bLastTimeStampFound && m_bLastFound && m_bLastSizeFound ) {
  //  std::cout << m_sSymbolName << " Trade is weird" << std::endl;
  //}
  //if ( m_bLastTimeStampFound && m_bLastFound && m_bLastSizeFound ) {
  if ( m_bLastFound && m_bLastSizeFound ) {
    Trade trade( ou::TimeSource::GlobalInstance().External(), m_dblLast, m_nLastSize );
    //std::cout << "T:" << trade.m_dt << " " << trade.m_nTradeSize << "@" << trade.m_dblTrade << std::endl;
    m_OnTrade( trade );
    //m_bLastTimeStampFound = m_bLastFound = m_bLastSizeFound = false;
    m_bLastFound = m_bLastSizeFound = false;
  }
}

void Symbol::Greeks( double optPrice, double undPrice, double pvDividend,
                        double impliedVol, double delta, double gamma, double vega, double theta ) {

  m_dblOptionPrice = optPrice;
  m_dblUnderlyingPrice = undPrice;
  m_dblPvDividend = pvDividend;

  ptime dt;
  ou::TimeSource::GlobalInstance().External( &dt );

  Greek greek( dt, impliedVol, delta, gamma, theta, vega, 0 );

  m_OnGreek( greek );

}

} // namespace ib
} // namespace tf
} // namespace ou
