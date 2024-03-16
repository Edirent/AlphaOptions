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
 * File:    AppAutoTrade.cpp
 * Author:  raymond@burkholder.net
 * Project: rdaf/at
 * Created: March 7, 2022 14:35
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include <sstream>

#include <boost/log/trivial.hpp>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/treectrl.h>

#include <rdaf/TRint.h>
#include <rdaf/TROOT.h>
#include <rdaf/TFile.h>

#include <TFTrading/Watch.h>
#include <TFTrading/Position.h>
#include <TFTrading/BuildInstrument.h>

#include <TFVuTrading/FrameMain.h>
#include <TFVuTrading/TreeItem.hpp>
#include <TFVuTrading/PanelLogging.h>
#include <TFVuTrading/WinChartView.h>
#include <TFVuTrading/PanelProviderControlv2.hpp>

#include "Strategy.hpp"
#include "AppAutoTrade.hpp"

namespace {
  static const std::string sAppName( "ROOT AutoTrade (rdaf_at)" );
  static const std::string sVendorName( "One Unified Net Limited" );

  static const std::string sDirectory( "rdaf/at" );
  static const std::string sDbName( sDirectory + "/example.db" );
  static const std::string sStateFileName( sDirectory + "/example.state" );
  static const std::string sChoicesFilename( sDirectory + "/choices.cfg" );
  static const std::string sFileNameUtility( sDirectory + "/utility.root" );

  static const std::string sMenuItemPortfolio( "_USD" );
}

namespace {

  class CustomItemData: public wxTreeItemData {
  public:
    std::string sSymbol;
    wxMenu* pMenuPopup;
    CustomItemData( wxMenu* pMenuPopup_ )
    : pMenuPopup( pMenuPopup_ )
    {}
    CustomItemData( const std::string& sSymbol_ )
    : sSymbol( sSymbol_ ), pMenuPopup( nullptr )
    {}
    virtual ~CustomItemData() {
      if ( nullptr != pMenuPopup ) {
        // assumes binds are cleared as well
        delete pMenuPopup;
        pMenuPopup = nullptr;
      }
    }
  };
}

IMPLEMENT_APP(AppAutoTrade)

bool AppAutoTrade::OnInit() {

  wxApp::SetVendorName( sVendorName );
  wxApp::SetAppDisplayName( sAppName );
  wxApp::SetVendorDisplayName( "(c)2022 " + sVendorName );

  wxApp::OnInit();

  m_bL2Connected = false;

  if ( !ou::tf::config::Load( sChoicesFilename, m_choices ) ) {
    return false;
  }

  m_iqfeed = ou::tf::iqfeed::Provider::Factory();
  m_alpaca = ou::tf::alpaca::Provider::Factory();

  m_iqfeed->SetThreadCount( m_choices.nThreads );
  m_alpaca->Set( m_choices.m_sAlpacaDomain, m_choices.m_sAlpacaKey, m_choices.m_sAlpacaSecret );

  // keep this commented here in case it needs to be resurrected
  //m_tws->SetClientId( m_choices.ib_client_id );

  m_pFrameMain = new FrameMain( 0, wxID_ANY, sAppName );
  wxWindowID idFrameMain = m_pFrameMain->GetId();

  m_pFrameMain->SetSize( 800, 500 );
  //m_pFrameMain->SetAutoLayout( true );

  SetTopWindow( m_pFrameMain );

  wxBoxSizer* sizerFrame;
  wxBoxSizer* sizerUpper;
  wxBoxSizer* sizerLower;

  sizerFrame = new wxBoxSizer(wxVERTICAL);
  m_pFrameMain->SetSizer(sizerFrame);

  sizerUpper = new wxBoxSizer(wxHORIZONTAL);
  sizerFrame->Add( sizerUpper, 0, wxEXPAND, 2);

  // TODO: make the panel conditional on simulation flag
  m_pPanelProviderControl = new ou::tf::v2::PanelProviderControl( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
  m_pPanelProviderControl->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
  m_pPanelProviderControl->Show();

  sizerUpper->Add( m_pPanelProviderControl, 0, wxALIGN_LEFT, 2);

  m_pPanelProviderControl->Add(
    m_iqfeed,
    true, false, false, false,
    [](){}, // fConnecting
    [this]( bool bD1, bool bD2, bool bX1, bool bX2 ){ // fConnected
      if ( bX1 || bX2 ) {
        m_iqfeed->EnableExecution( true );
      }
      if ( m_pL2Symbols ) {
        m_pL2Symbols->Connect();
      }
      ConfirmProviders();
    },
    [](){}, // fDisconnecting
    [this](){ // fDisconnected
      if ( m_pL2Symbols ) {
        m_pL2Symbols->Disconnect();
      }
      if ( m_iqfeed->ExecutionEnabled() ) {
        m_iqfeed->EnableExecution( false );
      }
    }
  );

  m_pPanelProviderControl->Add(
    m_alpaca,
    false, false, true, false,
    [](){}, // fConnecting
    [this]( bool bD1, bool bD2, bool bX1, bool bX2 ){ // fConnected
      ConfirmProviders();
    },
    [](){}, // fDisconnecting
    [](){}  // fDisconnected
  );

  // will need to change the date selection in the file, maybe use date from upperTime
  // will be removing hdf5 save/load - but need to clarify if simulation engine will continue to be used
  //   as it requires hdf5 formed timeseries, otherwise they need to be rebuilt from rdaf timeseries
  auto dt = ou::TimeSource::GlobalInstance().External();
  boost::gregorian::date dateSim( dt.date() ); // dateSim used later in a loop
  if ( m_choices.bStartSimulator ) {
    // m_sim does not need to be in PanelProviderControl
    m_sim = ou::tf::SimulationProvider::Factory();
    m_sim->SetThreadCount( m_choices.nThreads );
    if ( 0 < m_choices.sGroupDirectory.size() ) {
      m_sim->SetGroupDirectory( m_choices.sGroupDirectory );
    }

    boost::regex expr{ "(20[2-3][0-9][0-1][0-9][0-3][0-9])" };
    boost::smatch what;
    if ( boost::regex_search( m_choices.sGroupDirectory, what, expr ) ) {
      dateSim = boost::gregorian::from_undelimited_string( what[ 0 ] );
      std::cout << "simulation date " << dateSim << std::endl;
    }
    // need to be able to hookup simulation depth to the algo
    // does the symbol pump the depth through the same fibre/thread?
    // need to turn off m_pL2Symbols when running a simulation
    // need to feed the algo, not from m_pL2Symbols
    // use ou::tf::iqfeed::l2::MarketMaker directly, per symbol
  }
  else {
    m_timerOneSecond.SetOwner( this );
    Bind( wxEVT_TIMER, &AppAutoTrade::HandleOneSecondTimer, this, m_timerOneSecond.GetId() );
    m_timerOneSecond.Start( 500 );

    // perform this with iqfeed connection instead?
    m_pL2Symbols = std::make_unique<ou::tf::iqfeed::l2::Symbols>(
      [this](){
        m_bL2Connected = true;
        ConfirmProviders();
      } );
    //m_pL2Symbols->Connect();
  }

  m_pPanelLogging = new ou::tf::PanelLogging( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  m_pPanelLogging->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
  sizerUpper->Add(m_pPanelLogging, 2, wxEXPAND, 2);

  sizerLower = new wxBoxSizer(wxVERTICAL);
  sizerFrame->Add(sizerLower, 1, wxEXPAND, 2);

  m_splitterData = new wxSplitterWindow( m_pFrameMain, wxID_ANY, wxDefaultPosition, wxSize(100, 100), wxSP_3DBORDER|wxSP_3DSASH|wxNO_BORDER );
  m_splitterData->SetMinimumPaneSize(20);

  m_treeSymbols = new wxTreeCtrl( m_splitterData, wxID_ANY, wxDefaultPosition, wxSize(100, 100), wxTR_SINGLE );

  m_pWinChartView = new ou::tf::WinChartView( m_splitterData, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
  m_pWinChartView->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);

  m_splitterData->SplitVertically(m_treeSymbols, m_pWinChartView, 50);
  sizerLower->Add(m_splitterData, 1, wxGROW, 2);

  //if ( m_options.bSimStart ) {
    // just always delete it
    if ( boost::filesystem::exists( sDbName ) ) {
    boost::filesystem::remove( sDbName );
    }
  //}

  // this needs to be placed after the providers are registered
  m_pdb = std::make_unique<ou::tf::db>( sDbName );

  m_ceUnRealized.SetName( "unrealized" );
  m_ceRealized.SetName( "realized" );
  m_ceCommissionsPaid.SetName( "commission" );
  m_ceTotal.SetName( "total" );

  m_ceUnRealized.SetColour( ou::Colour::Blue );
  m_ceRealized.SetColour( ou::Colour::Purple );
  m_ceCommissionsPaid.SetColour( ou::Colour::Red );
  m_ceTotal.SetColour( ou::Colour::Green );

  m_dvChart.Add( 0, &m_ceUnRealized );
  m_dvChart.Add( 0, &m_ceRealized );
  m_dvChart.Add( 0, &m_ceTotal );
  m_dvChart.Add( 2, &m_ceCommissionsPaid );

  m_pWinChartView->SetChartDataView( &m_dvChart );

  TreeItem::Bind( m_pFrameMain, m_treeSymbols );
  m_pTreeItemRoot = new TreeItem( m_treeSymbols, "/" ); // initialize tree
  //wxTreeItemId idPortfolio = m_treeSymbols->AppendItem( idRoot, sMenuItemPortfolio, -1, -1, new CustomItemData( sMenuItemPortfolio ) );
  //m_treeSymbols->Bind( wxEVT_TREE_ITEM_MENU, &AppAutoTrade::HandleTreeEventItemMenu, this, m_treeSymbols->GetId() );
  //m_treeSymbols->Bind( wxEVT_TREE_ITEM_RIGHT_CLICK, &AppAutoTrade::HandleTreeEventItemRightClick, this, m_treeSymbols->GetId() );
  //m_treeSymbols->Bind( wxEVT_TREE_SEL_CHANGED, &AppAutoTrade::HandleTreeEventItemChanged, this, m_treeSymbols->GetId() );
  m_pTreeItemPortfolio = m_pTreeItemRoot->AppendChild(
    sMenuItemPortfolio,
    [this]( TreeItem* pTreeItem ){
      m_pWinChartView->SetChartDataView( &m_dvChart );
    }
  );

  // NOTE: during simulation, this subsystem is going to have to be temporary
  //   otherwise, the same data is read in multiple times when the simulation is run multiple times
  StartRdaf( sDirectory + m_sTSDataStreamStarted );

  // construct strategy for each symbol name in the configuration file
  for ( ou::tf::config::choices_t::mapInstance_t::value_type& vt: m_choices.mapInstance ) {

    auto& [sSymbol, choices] = vt;

    //BOOST_LOG_TRIVIAL(info) << "creating strategy for: " << sSymbol;

    TreeItem* pTreeItem = m_pTreeItemPortfolio->AppendChild(
      sSymbol,
      [this,sSymbol=vt.first]( TreeItem* pTreeItem ){
        mapStrategy_t::iterator iter = m_mapStrategy.find( sSymbol );
        assert( m_mapStrategy.end() != iter );
        m_pWinChartView->SetChartDataView( &iter->second->GetChartDataView() );
      }
    );

    pStrategy_t pStrategy = std::make_unique<Strategy>( choices, pTreeItem, m_pFile, m_pFileUtility );

    if ( m_choices.bStartSimulator ) {
      pStrategy->InitForUSEquityExchanges( dateSim );
    }

    m_mapStrategy.emplace( sSymbol, std::move( pStrategy ) );
    BOOST_LOG_TRIVIAL(info) << "strategy installed for: " << sSymbol;

    // TODO: use this to add an order list to the instrument: date, direction, type, limit
  }

  // load list of rdaf files for historical use
  // does the list need to be sorted?  can this be loaded in StartRdaf?
  for ( const vRdafFiles_t::value_type& sPath: m_vRdafFiles ) {
    BOOST_LOG_TRIVIAL(info) << "loading rdaf history: " << sPath;
    TFile* pFile = new TFile( sPath.c_str(), "READ" );
    assert( pFile->IsOpen() );

    TList* pList1 = pFile->GetListOfKeys();
    for ( const auto&& obj: *pList1 ) {
      TClass* class_ = (TClass*) obj;
      std::string name( class_->GetName() );
      std::string::size_type pos = name.find( '_', 0 );
      if ( std::string::npos != pos ) {
        std::string sSymbol = name.substr( 0, pos );
        mapStrategy_t::iterator iter = m_mapStrategy.find( sSymbol );
        if ( m_mapStrategy.end() != iter ) {
          iter->second->LoadHistory( class_ );
        }
      }
    }

    pFile->Close();
    delete pFile;
  }

  m_treeSymbols->ExpandAll();

  m_pFrameMain->Bind( wxEVT_CLOSE_WINDOW, &AppAutoTrade::OnClose, this );  // start close of windows and controls

  FrameMain::vpItems_t vItems;
  using mi = FrameMain::structMenuItem;  // vxWidgets takes ownership of the objects

  vItems.clear(); // maybe wrap this whole menu in the sim conditional
  vItems.push_back( new mi( "Close, Done", MakeDelegate( this, &AppAutoTrade::HandleMenuActionCloseAndDone ) ) );
  if ( !m_choices.bStartSimulator ) {
    vItems.push_back( new mi( "Save Values", MakeDelegate( this, &AppAutoTrade::HandleMenuActionSaveValues ) ) );
  }
  m_pFrameMain->AddDynamicMenu( "Actions", vItems );

  vItems.clear(); // TODO: need to turn off the autosave as well
  if ( !m_choices.bStartSimulator ) {
    vItems.push_back( new mi( "Flush", MakeDelegate( this, &AppAutoTrade::HandleMenuActionUtilityFlush ) ) );
    vItems.push_back( new mi( "Save", MakeDelegate( this, &AppAutoTrade::HandleMenuActionUtilitySave ) ) );
    //vItems.push_back( new mi( "Clear", MakeDelegate( this, &AppAutoTrade::HandleMenuActionUtilityClear ) ) );
    vItems.push_back( new mi( "Flush", MakeDelegate( this, &AppAutoTrade::HandleMenuActionUtilityClear ) ) );
    m_pFrameMain->AddDynamicMenu( "Utility File", vItems );
  }

  //m_pBuildInstrument = std::make_unique<ou::tf::BuildInstrument>( m_iqfeed, m_tws ); // replaced with alpaca
  m_pBuildInstrument = std::make_unique<ou::tf::BuildInstrument>( m_iqfeed );

  CallAfter(
    [this](){
      LoadState();
    }
  );

  if ( m_choices.bStartSimulator ) {
    CallAfter(
      [this](){
        m_sim->Connect();
      }
    );
  }

  m_pFrameMain->Show( true );

  return 1;
}

void AppAutoTrade::HandleOneSecondTimer( wxTimerEvent& event ) {

  double dblUnRealized;
  double dblRealized;
  double dblCommissionsPaid;
  double dblTotal;

  if ( m_pPortfolioUSD ) {
    m_pPortfolioUSD->QueryStats( dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal );

    ptime dt = ou::TimeSource::GlobalInstance().Internal();

    m_ceUnRealized.Append( dt, dblUnRealized );
    m_ceRealized.Append( dt, dblRealized );
    m_ceCommissionsPaid.Append( dt, dblCommissionsPaid );
    m_ceTotal.Append( dt, dblTotal );
  }
}

void AppAutoTrade::StartRdaf( const std::string& sFileName ) {

  int argc {};
  char** argv = nullptr;

  m_prdafApp = std::make_unique<TRint>( "rdaf_at", &argc, argv );
  ROOT::EnableImplicitMT();
  ROOT::EnableThreadSafety();

  namespace fs = boost::filesystem;
  namespace algo = boost::algorithm;
  if ( fs::is_directory( sDirectory ) ) {
    for ( fs::directory_entry& entry : fs::directory_iterator( sDirectory ) ) {
      if ( algo::ends_with( entry.path().string(), std::string( ".root" ) ) ) {
        std::string datetime( entry.path().filename().string() );
        algo::erase_last( datetime, ".root" );
        // 2020-01-31T23:59:59.123.root
        static const boost::regex regex(
          "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T"
          "[0-9][0-9]:[0-9][0-9]:[0-9][0-9]{0,1}\\.*[0-9]"
        );
        auto result
          = algo::find_regex( datetime, regex );
        if ( 0 < result.size() ) {
          ptime dt( boost::posix_time::from_iso_extended_string( datetime ) );
          if ( ( m_choices.dtLower <= dt ) && ( m_choices.dtUpper > dt ) ) {
            const std::string sFileName( sDirectory + '/' + datetime + ".root" );
            TFile* pFile = new TFile( sFileName.c_str(), "READ" );
            if ( nullptr != pFile ) {
              // run a preliminary test of the files
              if ( pFile->IsOpen() ) {
                //std::cout << "found " << sFileName << std::endl;
                m_vRdafFiles.push_back( sFileName );
                pFile->Close();
              }
              delete pFile;
            }
          }
        }
      }
    }
  }

  // open file after directory scan, so it is not included in the list
  m_pFile = std::make_shared<TFile>(
    ( sFileName + ".root" ).c_str(),
    "RECREATE",
    "tradeframe rdaf/at quotes, trades & histogram"
  );

  UpdateUtilityFile();  // re-open what exists

  //m_threadRdaf = std::move( std::thread( ThreadRdaf, this, sFileName ) );

  // example charting code in live analysis mode
  //TCanvas* c = new TCanvas("c", "Something", 0, 0, 800, 600);
  //TF1 *f1 = new TF1("f1","sin(x)", -5, 5);
  //f1->SetLineColor(kBlue+1);
  //f1->SetTitle("My graph;x; sin(x)");
  //f1->Draw();
  //c->Modified(); c->Update();
}

void AppAutoTrade::HandleMenuActionCloseAndDone() {
  std::cout << "Closing & Done" << std::endl;
  for ( mapStrategy_t::value_type& vt: m_mapStrategy ) {
    vt.second->CloseAndDone();
  }
}

void AppAutoTrade::HandleMenuActionSaveValues() {
  std::cout << "Saving collected values to ";
  CallAfter(
    [this](){
      m_nTSDataStreamSequence++; // sequence number on each save
      std::string sPath(
        "/app/" + sDirectory + "/" +
        m_sTSDataStreamStarted + "-" +
        boost::lexical_cast<std::string>( m_nTSDataStreamSequence ) );
      std::cout << sPath << std::endl;
      for ( mapStrategy_t::value_type& vt: m_mapStrategy ) {
        vt.second->SaveWatch( sPath );
      }
      //if ( m_pFile ) { // performed at exit to ensure no duplication in file
      //  m_pFile->Write();
      //}
      std::cout << "  ... Done " << std::endl;
    }
  );
}

void AppAutoTrade::ConstructIBInstrument(
  const std::string& sNamePortfolio
, const std::string& sSymbol
, fInstrumentConstructed_t&& fConstructed
) {

  using pInstrument_t = ou::tf::Instrument::pInstrument_t;
  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  m_pBuildInstrument->Queue(
    sSymbol,
    [this,&sNamePortfolio, &sSymbol, fConstructed_=std::move( fConstructed )]( pInstrument_t pInstrument, bool bConstructed ){
      if ( bConstructed ) {
        ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance() );
        im.Register( pInstrument );  // is a CallAfter required, or can this run in a thread?
      }

      ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );
      const ou::tf::Instrument::idInstrument_t& idInstrument( pInstrument->GetInstrumentName() );

      pPosition_t pPosition;
      if ( pm.PositionExists( sNamePortfolio, idInstrument ) ) {
        pPosition = pm.GetPosition( sNamePortfolio, idInstrument );
        BOOST_LOG_TRIVIAL(info) << "position loaded " << pPosition->GetInstrument()->GetInstrumentName();
      }
      else {
        pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_iqfeed );
        pPosition = pm.ConstructPosition(
          sNamePortfolio, idInstrument, "rdaf",
          "alpaca", "iq01", m_alpaca,
          pWatch
        );
        BOOST_LOG_TRIVIAL(info) << "position constructed " << pPosition->GetInstrument()->GetInstrumentName();
      }
      mapStrategy_t::iterator iterStrategy = m_mapStrategy.find( sSymbol );
      assert( m_mapStrategy.end() != iterStrategy );
      iterStrategy->second->SetPosition( pPosition );
      fConstructed_( sSymbol );
    } );

}

void AppAutoTrade::ConstructSimInstrument( const std::string& sNamePortfolio, const std::string& sSymbol ) {

  using pInstrument_t = ou::tf::Instrument::pInstrument_t;
  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  ou::tf::Instrument::pInstrument_t pInstrument = std::make_shared<ou::tf::Instrument>( sSymbol );
  const ou::tf::Instrument::idInstrument_t& idInstrument( pInstrument->GetInstrumentName() );
  ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance() );
  im.Register( pInstrument );  // is a CallAfter required, or can this run in a thread?
  pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_sim );
  ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );
  pPosition_t pPosition;
  if ( pm.PositionExists( sNamePortfolio, idInstrument ) ) {
    pPosition = pm.GetPosition( sNamePortfolio, idInstrument );
    std::cout << "sim: probably should delete database first" << std::endl;
    std::cout << "sim: position loaded " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
  }
  else {
    pWatch_t pWatch = std::make_shared<ou::tf::Watch>( pInstrument, m_sim );
    pPosition = pm.ConstructPosition(
      sNamePortfolio, idInstrument, "rdaf",
      "sim01", "sim01", m_sim,
      pWatch
    );
    std::cout << "Constructed " << pPosition->GetInstrument()->GetInstrumentName() << std::endl;
  }
  mapStrategy_t::iterator iterStrategy = m_mapStrategy.find( sSymbol );
  assert( m_mapStrategy.end() != iterStrategy );
  iterStrategy->second->SetPosition( pPosition );

}

void AppAutoTrade::HandleMenuActionSimStart() {
  CallAfter(
    [this](){
      m_sim->Run();
    }
  );
}

void AppAutoTrade::HandleMenuActionSimStop() {
  CallAfter(
    [this](){
      m_sim->Stop();
    }
  );
}

// don't use this, as the pointer changes, and needs to be redistributed into the objects
void AppAutoTrade::RecreateUtilityFile() {
  m_pFileUtility = std::make_shared<TFile>(
    sFileNameUtility.c_str(),
    "RECREATE",
    "tradeframe rdaf/at utility"
  );
}

void AppAutoTrade::UpdateUtilityFile() {
  m_pFileUtility = std::make_shared<TFile>(
    sFileNameUtility.c_str(),
    "UPDATE",
    "tradeframe rdaf/at utility"
  );
}

void AppAutoTrade::HandleMenuActionUtilitySave() {
  if ( m_pFileUtility ) {
    m_pFileUtility->Write();
  }
}

void AppAutoTrade::HandleMenuActionUtilityFlush() {
  if ( m_pFileUtility ) {
    m_pFileUtility->Flush();
  }
}

void AppAutoTrade::HandleMenuActionUtilityClear() {
  RecreateUtilityFile();
}

void AppAutoTrade::HandleMenuActionSimEmitStats() {
  std::stringstream ss;
  m_sim->EmitStats( ss );
  std::cout << "Stats: " << ss.str() << std::endl;
}

void AppAutoTrade::LoadPortfolio( const std::string& sName ) {

  ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );

  if ( pm.PortfolioExists( sName ) ) {
    m_pPortfolioUSD = pm.GetPortfolio( sName );
  }
  else {
    m_pPortfolioUSD
      = pm.ConstructPortfolio(
          sName, "tf01", "USD",
          ou::tf::Portfolio::EPortfolioType::Standard,
          ou::tf::Currency::Name[ ou::tf::Currency::USD ] );
  }
}

void AppAutoTrade::ConfirmProviders() {

  bool bValidCombo( false );

  if ( m_choices.bStartSimulator ) {
    bValidCombo = true;
    static const std::string sNamePortfolio( "SIM" );
    LoadPortfolio( sNamePortfolio );
    for ( mapStrategy_t::value_type& vt: m_mapStrategy ) {
      ConstructSimInstrument( sNamePortfolio, vt.first );
    }

    FrameMain::vpItems_t vItems;
    using mi = FrameMain::structMenuItem;  // vxWidgets takes ownership of the objects
    vItems.push_back( new mi( "Start", MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimStart ) ) );
    vItems.push_back( new mi( "Stop",  MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimStop ) ) );
    vItems.push_back( new mi( "Stats",  MakeDelegate( this, &AppAutoTrade::HandleMenuActionSimEmitStats ) ) );
    m_pFrameMain->AddDynamicMenu( "Simulation", vItems );

    m_sim->Run();
  }
  else {
    if ( m_iqfeed->Connected() && m_alpaca->Connected() ) {
      if ( m_bL2Connected ) {
        bValidCombo = true;
        std::cout << "ConfirmProviders: using iqfeed and alpaca for data/execution" << std::endl;
        static const std::string sNamePortfolio( "alpaca" );
        LoadPortfolio( sNamePortfolio );
        for ( mapStrategy_t::value_type& vt: m_mapStrategy ) {
          Strategy& strategy( *vt.second );
          ConstructIBInstrument(
            sNamePortfolio, vt.first,
            [this,&strategy]( const std::string& sSymbol ){
              //mapStrategy_t::iterator iter = m_mapStrategy.find( sSymbol );
              //Strategy& strategy( *iter->second );
              using EFeed = ou::tf::config::symbol_t::EFeed;
              auto symbol = m_iqfeed->GetSymbol( sSymbol );
              if ( m_pL2Symbols ) {
                switch ( strategy.Feed() ) {
                  case EFeed::L1:
                    break;
                  case EFeed::L2M:
                    BOOST_LOG_TRIVIAL(info) << "ConfirmProviders starting L2M for: " << sSymbol;
                    m_pL2Symbols->WatchAdd(
                      sSymbol,
                      [symbol]( const ou::tf::DepthByMM& md ){
                        symbol->SubmitMarketDepthByMM( md );
                      }
                      );
                    break;
                  case EFeed::L2O:
                    BOOST_LOG_TRIVIAL(info) << "ConfirmProviders starting L2O for: " << sSymbol;
                    m_pL2Symbols->WatchAdd(
                      sSymbol,
                      [symbol]( const ou::tf::DepthByOrder& md ){
                        symbol->SubmitMarketDepthByOrder( md );
                      }
                      );
                    break;
                }
              }
              else {
                assert( false ); // m_pL2Symbols needs to be available
              }
          }
          );
        }
      }
      else {
        std::cout << "ConfirmProviders: waiting for iqfeed level 2 connection" << std::endl;
      }
    }
  }

  if ( !bValidCombo ) {
    std::cout << "ConfirmProviders: waiting for data and execution providers" << std::endl;
  }
}

void AppAutoTrade::SaveState() {
  std::cout << "Saving Config ..." << std::endl;
  std::ofstream ofs( sStateFileName );
  boost::archive::text_oarchive oa(ofs);
  oa & *this;
  std::cout << "  done." << std::endl;
}

void AppAutoTrade::LoadState() {
  try {
    std::cout << "Loading Config ..." << std::endl;
    std::ifstream ifs( sStateFileName );
    boost::archive::text_iarchive ia(ifs);
    ia & *this;
    std::cout << "  done." << std::endl;
  }
  catch(...) {
    std::cout << "load exception" << std::endl;
  }
}

int AppAutoTrade::OnExit() {
  // Exit Steps: #4
//  DelinkFromPanelProviderControl();  generates stack errors

  return wxAppConsole::OnExit();
}

void AppAutoTrade::OnClose( wxCloseEvent& event ) {
  // Exit Steps: #2 -> FrameMain::OnClose

  //m_pWinChartView->SetChartDataView( nullptr, false );
  //delete m_pChartData;
  //m_pChartData = nullptr;

  //m_pFrameControls->Close();

  if ( !m_choices.bStartSimulator ) {
    m_timerOneSecond.Stop();
    Unbind( wxEVT_TIMER, &AppAutoTrade::HandleOneSecondTimer, this, m_timerOneSecond.GetId() );
  }

  if ( m_pdb ) m_pdb.reset();

  // NOTE: when running the simuliation, perform a deletion instead
  //   use the boost file system utilities?
  //   or the object Delete() operator may work
  if ( m_choices.bStartSimulator ) {
    if ( m_pFile ) { // performed at exit to ensure no duplication in file
      //m_pFile->Delete(); // double free or corruption here
    }
  }
  else {
    if ( m_pFile ) { // performed at exit to ensure no duplication in file
      m_pFile->Write();
    }
  }

//  if ( 0 != OnPanelClosing ) OnPanelClosing();
  // event.Veto();  // possible call, if needed
  // event.CanVeto(); // if not a
  SaveState();
  event.Skip();  // auto followed by Destroy();
}
