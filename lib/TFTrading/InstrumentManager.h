/************************************************************************
 * Copyright(c) 2009, One Unified. All rights reserved.                 *
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

#pragma once

// 2011/03/16 add persist-to-db superclass for saving/retrieving instruments

#include <map>
#include <mutex>
#include <string>

#include <OUCommon/ManagerBase.h>

#include "KeyTypes.h"

#include "Instrument.h"

namespace ou { // One Unified
namespace tf { // TradeFrame

class InstrumentManager
  : public ou::db::ManagerBase<InstrumentManager> {
public:

  using inherited_t = ou::db::ManagerBase<InstrumentManager>;

  using pInstrument_t = Instrument::pInstrument_t;
  using pInstrument_cref = Instrument::pInstrument_cref;
  using idInstrument_t = Instrument::idInstrument_t;
  using idInstrument_cref = Instrument::idInstrument_cref;

  InstrumentManager();
  virtual ~InstrumentManager();

  pInstrument_t ConstructInstrument(
    idInstrument_cref sInstrumentName, const std::string& sExchangeName, // generic
    InstrumentType::EInstrumentType type = InstrumentType::Unknown );
  pInstrument_t ConstructFuture(
    idInstrument_cref sInstrumentName, const std::string& sExchangeName,  // future
    boost::uint16_t year, boost::uint16_t month, boost::uint16_t day );
  pInstrument_t ConstructFuturesOption(
    idInstrument_cref sInstrumentName, const std::string& sExchangeName,  // option with yymmdd
    boost::uint16_t year, boost::uint16_t month, boost::uint16_t day,
    OptionSide::EOptionSide side,
    double strike );
  pInstrument_t ConstructOption(
    idInstrument_cref sInstrumentName, const std::string& sExchangeName,  // option with yymmdd
    boost::uint16_t year, boost::uint16_t month, boost::uint16_t day,
    OptionSide::EOptionSide side,
    double strike );
  pInstrument_t ConstructCurrency(
    idInstrument_cref idInstrumentName,
    const std::string& sExchangeName,
    Currency::ECurrency base, Currency::ECurrency counter );

  void Register( pInstrument_t& pInstrument );

  bool Exists( idInstrument_cref );
  bool Exists( idInstrument_cref, pInstrument_t& );
  bool Exists( pInstrument_cref );
  pInstrument_t Get( idInstrument_cref ); // for getting existing associated with id
  void Delete( idInstrument_cref );

  pInstrument_t LoadInstrument( keytypes::eidProvider_t, const idInstrument_t& ); // may have exeption?

  template<typename F> void ScanOptions( F f, idInstrument_cref, boost::uint16_t year, boost::uint16_t month, boost::uint16_t day );

  virtual void AttachToSession( ou::db::Session* pSession );
  virtual void DetachFromSession( ou::db::Session* pSession );

protected:

  void Assign( pInstrument_cref pInstrument );
  bool LoadInstrument( idInstrument_t idInstrument, pInstrument_t& pInstrument );
  void LoadAlternateInstrumentNames( pInstrument_t& pInstrument );

private:

  using mapInstruments_t = std::map<idInstrument_t,pInstrument_t>;
  using iterInstruments_t = mapInstruments_t::iterator;
  mapInstruments_t m_mapInstruments;

  using keyAltName_t = std::pair<keytypes::eidProvider_t, std::string>;
  using keyAltName_ref_t = std::pair<const keytypes::eidProvider_t&, const std::string&>;
  struct keyAltName_compare {
    bool operator()( const keyAltName_t& key1, const keyAltName_t& key2 ) const {
      if ( key1.first < key2.first ) {
        return true;
      }
      else {
        return key1.second < key2.second;
      }
    }
  };
  using mapAltNames_t = std::map<keyAltName_t, pInstrument_t, keyAltName_compare>;
  mapAltNames_t m_mapAltNames;

  std::mutex m_mutexLoadInstrument;

  void SaveAlternateInstrumentName(
    const keytypes::eidProvider_t&,
    const keytypes::idInstrument_t&, const keytypes::idInstrument_t&,
    pInstrument_t
    );

  void HandleRegisterTables( ou::db::Session& session );
  void HandleRegisterRows( ou::db::Session& session );
  void HandlePopulateTables( ou::db::Session& session );

  void HandleAlternateNameAdded( const Instrument::AlternateNameChangeInfo_t& );
  void HandleAlternateNameChanged( const Instrument::AlternateNameChangeInfo_t& );
};

} // namespace tf
} // namespace ou
