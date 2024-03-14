/************************************************************************
 * Copyright(c) 2022, One Unified. All rights reserved.                 *
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
 * File:    Strategy.h
 * Author:  raymond@burkholder.net
 * Project: rdaf/at
 * Created: March 7, 2022 14:35
 */

#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <OUCharting/ChartEntryBars.h>
#include <OUCharting/ChartEntryMark.h>
#include <OUCharting/ChartEntryShape.h>
#include <OUCharting/ChartEntryVolume.h>
#include <OUCharting/ChartEntryIndicator.h>

#include <OUCharting/ChartDataView.h>

#include <TFTimeSeries/BarFactory.h>

#include <TFTrading/Order.h>
#include <TFTrading/Position.h>
#include <TFTrading/DailyTradeTimeFrames.h>

#include <TFIQFeed/Level2/Symbols.hpp>
#include <TFIQFeed/Level2/FeatureSet.hpp>

#include "ConfigParser.hpp"

class TH2D;
class TFile;
class TTree;
class TClass;

namespace ou {
  class ChartDataView;
namespace tf {
  class TreeItem;
} // namespace tf
} // namespace ou

class Strategy:
  public ou::tf::DailyTradeTimeFrame<Strategy>
{
  friend class boost::serialization::access;
  friend ou::tf::DailyTradeTimeFrame<Strategy>;
public:

  using price_t = ou::tf::Trade::price_t;
  using volume_t = ou::tf::Trade::volume_t;

  using TreeItem = ou::tf::TreeItem;
  using pOrder_t = ou::tf::Order::pOrder_t;
  using pPosition_t = ou::tf::Position::pPosition_t;
  using pFile_t = std::shared_ptr<TFile>;

  Strategy(
    const ou::tf::config::symbol_t&
  , TreeItem*
  , pFile_t
  , pFile_t
  );
  virtual ~Strategy();

  void SetPosition( pPosition_t );

  ou::tf::config::symbol_t::EFeed Feed() const { return m_config.eFeed; }

  void LoadHistory( TClass* );

  void HandleUpdateL2Ask( price_t price, volume_t volume, bool bAdd );
  void HandleUpdateL2Bid( price_t price, volume_t volume, bool bAdd );

  void SaveWatch( const std::string& );

  ou::ChartDataView& GetChartDataView() { return m_cdv; }

  void CloseAndDone();

protected:
private:

  enum EChartSlot { Price, Volume, Skew, PL, ET, MarketDepth };
  enum class ETradeState {
    Init,  // initiaize state in current market
    Search,  // looking for long or short enter
    LongSubmitted, // order has been submitted, waiting for confirmation
    LongExit,  // position exists, looking for exit
    ShortSubmitted,  // order has been submitted, waiting for confirmtaion
    ShortExit,  // position exists, looking for exit
    LongExitSubmitted, // wait for exit to complete
    ShortExitSubmitted, // wait for exit to complete
    NoTrade, // from the config file, no trading, might be a future
    EndOfDayCancel,
    EndOfDayNeutral,
    Done // no more action
    };

  TreeItem* m_pTreeItemSymbol;
  TreeItem* m_pTreeItemOrder;

  bool m_bChangeConfigFileMessageLatch;

  ETradeState m_stateTrade;

  ou::tf::Quote m_quote;

  const ou::tf::config::symbol_t& m_config;

  pFile_t m_pFile;
  pFile_t m_pFileUtility;

  pOrder_t m_pOrder;
  pPosition_t m_pPosition;

  ou::ChartDataView m_cdv;

  ou::ChartEntryIndicator m_ceQuoteAsk;
  ou::ChartEntryIndicator m_ceQuoteBid;

  ou::ChartEntryIndicator m_ceTrade;
  ou::ChartEntryVolume m_ceVolume;

  ou::ChartEntryShape m_ceLongEntry;
  ou::ChartEntryShape m_ceLongFill;
  ou::ChartEntryShape m_ceLongExit;
  ou::ChartEntryShape m_ceShortEntry;
  ou::ChartEntryShape m_ceShortFill;
  ou::ChartEntryShape m_ceShortExit;

  ou::ChartEntryIndicator m_ceProfitLoss;

  ou::ChartEntryIndicator m_cdMarketDepthAsk;
  ou::ChartEntryIndicator m_cdMarketDepthBid;

  ou::ChartEntryIndicator m_ceSkewness;
  ou::ChartEntryIndicator m_ceExecutionTime;

  ou::tf::BarFactory m_bfQuotes01Sec;

  using pMarketMaker_t = ou::tf::iqfeed::l2::MarketMaker::pMarketMaker_t;
  pMarketMaker_t m_pMarketMaker;

  using pOrderBased_t = ou::tf::iqfeed::l2::OrderBased::pOrderBased_t;
  pOrderBased_t m_pOrderBased;

  // ==
  struct QuoteForBranch {
    double time;
    double ask;
    uint64_t askvol;
    double bid;
    uint64_t bidvol;
  } m_branchQuote;

  struct TradeForBranch {
    double time;
    double price;
    uint64_t vol;
    int64_t direction;
  } m_branchTrade;

  // https://root.cern/doc/master/classTTree.html
  using pTTree_t = std::shared_ptr<TTree>;
  pTTree_t m_pTreeQuote;
  pTTree_t m_pTreeTrade;

  using pTH2D_t = std::shared_ptr<TH2D>;
  pTH2D_t m_pHistVolume;
  pTH2D_t m_pHistVolumeDemo;

  ou::tf::iqfeed::l2::FeatureSet m_FeatureSet;

  void InitRdaf();

  void HandleQuote( const ou::tf::Quote& );
  void HandleTrade( const ou::tf::Trade& );
  void HandleDepthByMM( const ou::tf::DepthByMM& );
  void HandleDepthByOrder( const ou::tf::DepthByOrder& );

  void HandleBarQuotes01Sec( const ou::tf::Bar& bar );

  void HandleRHTrading( const ou::tf::Bar& bar );
  void HandleCancel( boost::gregorian::date, boost::posix_time::time_duration );
  void HandleGoNeutral( boost::gregorian::date, boost::posix_time::time_duration );

  void EnterLong( const ou::tf::Bar& );
  void EnterShort( const ou::tf::Bar& );

  void ExitLong( const ou::tf::Bar& );
  void ExitShort( const ou::tf::Bar& );

  void ShowOrder( pOrder_t );

  void HandleOrderCancelled( const ou::tf::Order& );
  void HandleOrderFilled( const ou::tf::Order& );

  void Clear();
  void SetupChart();

};
