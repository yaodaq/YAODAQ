#pragma once
#include "Data.hpp"

#include <ROOT/RNTupleReader.hxx>
#include <TCanvas.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TPaveStats.h>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

// =====================================================================
//  1. Configuration & Constants
// =====================================================================
// ========== CONFIGURATION: Enable/disable features ==========
static const bool ENABLE_CHANNEL_COUNT           = true;  // Count hits per channel, rising/falling separate
static const bool ENABLE_EFFICIENCY              = true;  // Efficiency per layer
static const bool ENABLE_TOT_HIST                = true;  // TOT distribution per(computed from rising/falling edges)
static const bool ENABLE_SIGNAL_DELAY            = true;  // Time difference between hit rising edge and trigger rising edge
static const bool ENABLE_TRIGGER_COUNT           = true;  // Number of trigger hits per event (rising+falling)
static const bool ENABLE_CLUSTER_SIZE            = true;  // Cluster size per (layer, side)
static const bool ENABLE_POSITION_RECONSTRUCTION = true;  // 2D position (eta-phi)
//static const bool ENABLE_ROLLING = true;   // Enable BCID rolling histogram (for debugging time reconstruction)

// Physical parameters and hardware setup
static const double DETECTOR_LENGTH        = 1705.0;  // mm, eta direction
static const double DETECTOR_WIDTH         = 1107.0;  // mm, phi direction
static const double SIGNAL_SPEED           = 220;     // mm/ns (adjust)
static const double BCID_CLOCK             = 25.0;    // ns per BCID, adjust according to your hardware
static const int    BCID_PERIOD /*_R*/     = 256;     //BCID number which will roll back to 0
//static const int BCID_PERIOD_F = 512;  //BCID number which will roll back to 0 (Falling)
static const int    MAX_CHANNELS_PER_GROUP = 48;

//Other setup
static const int MAX_TRIGGERS_PER_EVENT = 10;  // Maximum expected trigger hits per event (adjust)

// =====================================================================
//  2. Data Structures
// =====================================================================
// Event data structure (adjust members according to your RNTuple)
struct Hit
{
  float time;
  int   channel;
  int   layer;
  int   side;
  bool  isRising;  // true = rising edge, false = falling edge
                   // add other fields if needed (e.g., leading/trailing)
};
struct EventData
{
  std::vector<Hit> hits;
  std::vector<Hit> triggerHits;
};

class Efficiency
{
public:
  explicit Efficiency() = default;
  void efficient()
  {
    ++m_num;
    ++m_denum;
  }
  void        inefficient() { ++m_denum; }
  std::size_t numerator() const noexcept { return m_num; }
  std::size_t denominator() const noexcept { return m_denum; }
  double      efficiency() const noexcept { return m_num * 1.0 / m_denum; }
  double      error() const noexcept
  {
    double eff = efficiency();
    return std::sqrt( eff * ( 1.0 - eff ) / m_denum );
  }

private:
  std::size_t m_num{ 0 };
  std::size_t m_denum{ 0 };
};

// =====================================================================
//  3. Analyzer Class
// =====================================================================
class RPCDataAnalyzer
{
  // 3.1 Constructor & Destructor
public:
  RPCDataAnalyzer()
  {
    // Create histograms based on enabled features
    if( ENABLE_CHANNEL_COUNT )
    {
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx            = layer * 2 + side;
          hRisingCount[idx]  = new TH1F( Form( "rising_layer%d_side%d", layer, side ), Form( "Layer %d, Side %d Channel Distribution;Channel Number;Counts", layer, side + 1 ), MAX_CHANNELS_PER_GROUP, -0.5, MAX_CHANNELS_PER_GROUP - 0.5 );
          hFallingCount[idx] = new TH1F( Form( "falling_layer%d_side%d", layer, side ), Form( "Layer %d, Side %d Channel Distribution;Channel Number;Counts", layer, side + 1 ), MAX_CHANNELS_PER_GROUP, -0.5, MAX_CHANNELS_PER_GROUP - 0.5 );
        }
      }
    }
    if( ENABLE_EFFICIENCY )
    {
      // Assume 3 layers: index 0,1,2 -> bins 0..3 (so bin 1 for layer 0)
      hTotalHits   = new TH1F( "total_hits", "Total Hits per Layer;Layer;Hits", 3, 0, 3 );
      hTriggerHits = new TH1F( "trigger_hits", "Trigger Hits per Layer;Layer;Hits", 3, 0, 3 );
    }
    if( ENABLE_TOT_HIST )
    {
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx   = layer * 2 + side;
          hTot[idx] = new TH1F( Form( "tot_layer%d_side%d", layer, side + 1 ), Form( "Layer %d, Side %d TOT;TOT (ns);Counts", layer, side + 1 ), 30, 0, 30 );
        }
      }
    }
    if( ENABLE_SIGNAL_DELAY )
    {
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx     = layer * 2 + side;
          hDelay[idx] = new TH1F( Form( "delay_layer%d_side%d", layer, side + 1 ), Form( "Layer %d, Side %d Signal Delay;Delay (ns);Counts", layer, side + 1 ), 100, -200, 50 );  // range can be adjusted
        }
      }
    }
    if( ENABLE_TRIGGER_COUNT ) { hTriggerCount = new TH1F( "trigger_count", "Number of Trigger Hits per Event;Trigger Hits;Events", MAX_TRIGGERS_PER_EVENT, -0.5, MAX_TRIGGERS_PER_EVENT - 0.5 ); }
    if( ENABLE_CLUSTER_SIZE )
    {
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx           = layer * 2 + side;
          hClusterSize[idx] = new TH1F( Form( "cluster_layer%d_side%d", layer, side + 1 ), Form( "Layer %d, Side %d Cluster Size;Cluster Size;Counts", layer, side + 1 ), 11, -0.5, 10.5 );  // bins for sizes 0..10
        }
      }
    }

    if( ENABLE_POSITION_RECONSTRUCTION )
    {
      for( int layer = 0; layer < 3; ++layer )
      {
        hPosEtaPhi[layer] = new TH2F( Form( "pos_eta_phi_layer%d", layer ), Form( "Layer %d 2D Hitmap;Phi (mm);Eta (mm)", layer ), 48, 0, DETECTOR_WIDTH,  // phi bins, one per channel approx
                                      DETECTOR_LENGTH / ( SIGNAL_SPEED * 5 / 6 / 2 ), 0, DETECTOR_LENGTH );                                                // eta bins
      }
    }
    /*if (ENABLE_ROLLING)
  {
    hRolling = new TH1F("rolling", "BCID rolling count;Rolling value;Hits",10, -0.5, 9.5);   // Adjust bins if rollover exceeds 9
  }	*/
  }

  //Clean up histograms to avoid memory leaks
  ~RPCDataAnalyzer()
  {
    for( auto h: hRisingCount ) delete h;
    for( auto h: hFallingCount ) delete h;
    for( auto h: hTot ) delete h;
    for( auto h: hDelay ) delete h;
    delete hTriggerCount;
    delete hTotalHits;
    delete hTriggerHits;
    for( auto h: hClusterSize ) delete h;
    for( auto h: hPosEtaPhi ) delete h;
    delete hRolling;
  }

  // 3.2 Output Configuration
  // Set output directory (will be created by caller)
  void setOutputDir( const std::string& dir ) { fOutputDir = dir; }

  // 3.3 Event Processing
  //process a single event
  void processEvent( const EventData& ev, TH1D* my_ )
  {
    // ----- Channel counting (rising/falling separated, per layer/side) -----
    if( ENABLE_CHANNEL_COUNT )
    {
      for( const auto& hit: ev.hits )
      {
        int layer = hit.layer;
        if( my_ ) my_->Fill( layer );

        int side = hit.side;
        int ch   = hit.channel;
        // Check validity
        if( layer < 0 || layer >= 3 || side < 0 || side >= 2 ) continue;
        if( ch < 0 || ch >= MAX_CHANNELS_PER_GROUP ) continue;
        int idx = layer * 2 + side;
        if( hit.isRising ) hRisingCount[idx]->Fill( ch );
        else
          hFallingCount[idx]->Fill( ch );
      }
    }

    // ----- Efficiency (event-based: three-fold / two-fold) -----
    if( ENABLE_EFFICIENCY )
    {
      // --- efficiency per layer ---
      bool layerHasHit[3]   = { false, false, false };
      bool sideHasHit[3][2] = { { false, false }, { false, false }, { false, false } };

      for( const auto& hit: ev.hits )
      {
        int l = hit.layer;
        int s = hit.side;
        if( l >= 0 && l < 3 && s >= 0 && s < 2 )
        {
          layerHasHit[l]   = true;
          sideHasHit[l][s] = true;
        }
      }

      // Update per-layer hit counts (original)
      for( int i = 0; i < 3; ++i )
      {
        if( layerHasHit[i] ) layerHitCount[i]++;
      }

      // Check three-fold coincidence (original)
      if( layerHasHit[0] && layerHasHit[1] && layerHasHit[2] ) { threeFold++; }

      // Update pair counts (original)
      if( layerHasHit[1] && layerHasHit[2] ) pairCount[0]++;
      if( layerHasHit[0] && layerHasHit[2] ) pairCount[1]++;
      if( layerHasHit[0] && layerHasHit[1] ) pairCount[2]++;

      // --- New efficiency per (layer, side) ---
      totalEvents++;  // Count total events

      for( int l = 0; l < 3; ++l )
      {
        bool otherLayersHaveHit = false;
        if( l == 0 ) otherLayersHaveHit = layerHasHit[1] && layerHasHit[2];
        else if( l == 1 )
          otherLayersHaveHit = layerHasHit[0] && layerHasHit[2];
        else
          otherLayersHaveHit = layerHasHit[0] && layerHasHit[1];
        if( otherLayersHaveHit )
        {
          for( int s = 0; s < 2; ++s )
          {
            int idx = l * 2 + s;
            if( sideHasHit[l][s] ) m_efficiencies[idx].efficient();
            else
              m_efficiencies[idx].inefficient();
          }
        }
      }
    }
    // ----- TOT distribution (compute from rising/falling edges) -----
    if( ENABLE_TOT_HIST )
    {
      std::vector<const Hit*> validHits;
      for( const auto& hit: ev.hits )
      {
        if( hit.channel >= 0 && hit.channel < MAX_CHANNELS_PER_GROUP ) { validHits.push_back( &hit ); }
      }

      // Sort by (channel, layer, side, time) to group matching edges
      std::sort( validHits.begin(), validHits.end(),
                 []( const Hit* a, const Hit* b )
                 {
                   if( a->channel != b->channel ) return a->channel < b->channel;
                   if( a->layer != b->layer ) return a->layer < b->layer;
                   if( a->side != b->side ) return a->side < b->side;
                   return a->time < b->time;
                 } );

      // Iterate groups and match rising -> falling
      size_t i = 0;
      while( i < validHits.size() )
      {
        size_t j = i;
        // Find the range of hits with the same (channel, layer, side)
        while( j < validHits.size() && validHits[j]->channel == validHits[i]->channel && validHits[j]->layer == validHits[i]->layer && validHits[j]->side == validHits[i]->side ) { j++; }

        // Process this group [i, j)
        float startTime = -1.0f;  // Store time of last unmatched rising edge
        for( size_t k = i; k < j; ++k )
        {
          if( validHits[k]->isRising ) { startTime = validHits[k]->time; }
          else
          {  // Falling edge
            if( startTime >= 0 )
            {
              float tot = validHits[k]->time - startTime;
              if( tot > 0 )
              {
                int idx = validHits[k]->layer * 2 + validHits[k]->side;
                if( idx >= 0 && idx < 6 ) hTot[idx]->Fill( tot );
              }
              startTime = -1.0f;  // Reset after successful match
            }
            // else: orphan falling edge (no rising before), ignore
          }
        }
        i = j;  // Move to next group
      }
    }

    // ----- Signal delay (rising edge time difference with trigger) -----
    if( ENABLE_SIGNAL_DELAY )
    {
      // Determine trigger reference time: use the first trigger hit that is rising edge
      float triggerTime = -1.0f;
      for( const auto& trg: ev.triggerHits )
      {
        if( trg.isRising )
        {
          triggerTime = trg.time;
          break;  // use the first rising trigger hit
        }
      }
      if( triggerTime >= 0 )
      {
        // Loop over ordinary hits, only rising edges
        for( const auto& hit: ev.hits )
        {
          if( !hit.isRising ) continue;
          int layer = hit.layer;
          int side  = hit.side;
          int ch    = hit.channel;  // optional, not used for grouping
          if( layer < 0 || layer >= 3 || side < 0 || side >= 2 ) continue;
          int   idx   = layer * 2 + side;
          float delay = hit.time - triggerTime;
          hDelay[idx]->Fill( delay );
        }
      }
    }

    // ----- Trigger count per event -----
    if( ENABLE_TRIGGER_COUNT )
    {
      int nTrig = ev.triggerHits.size();  // counts rising and falling together
      if( nTrig < MAX_TRIGGERS_PER_EVENT ) { hTriggerCount->Fill( nTrig ); }
      else
      {
        // Optionally fill overflow bin (if you have an overflow bin, but we just ignore or fill last bin)
        // Here we simply fill the last bin as overflow
        hTriggerCount->Fill( MAX_TRIGGERS_PER_EVENT - 1 );
      }
    }

    // ----- Cluster size per layer/side -----
    if( ENABLE_CLUSTER_SIZE )
    {
      // Group hits by (layer, side) and collect unique channels
      std::array<std::array<std::set<int>, 2>, 3> channelSets;  // [layer][side] set of channels
      for( const auto& hit: ev.hits )
      {
        int l = hit.layer;
        int s = hit.side;
        if( l >= 0 && l < 3 && s >= 0 && s < 2 ) { channelSets[l][s].insert( hit.channel ); }
      }

      // For each (layer, side), sort channels and find clusters
      for( int l = 0; l < 3; ++l )
      {
        for( int s = 0; s < 2; ++s )
        {
          const auto& chSet = channelSets[l][s];
          if( chSet.empty() ) continue;
          std::vector<int> chVec( chSet.begin(), chSet.end() );
          std::sort( chVec.begin(), chVec.end() );
          int clusterSize = 1;
          for( size_t i = 1; i < chVec.size(); ++i )
          {
            if( chVec[i] == chVec[i - 1] + 1 ) { clusterSize++; }
            else
            {
              int idx = l * 2 + s;
              hClusterSize[idx]->Fill( clusterSize );
              clusterSize = 1;
            }
          }
          // fill the last cluster
          int idx = l * 2 + s;
          hClusterSize[idx]->Fill( clusterSize );
        }
      }
    }

    //----- 2D position reconstruction -----
    if( ENABLE_POSITION_RECONSTRUCTION )
    {
      // Group rising edges by (layer, channel) for both sides
      struct SideTimes
      {
        float t0 = -99999.0f, t1 = -99999.0f;
      };
      std::map<std::pair<int, int>, SideTimes> timeMap;  // key = (layer, channel)

      for( const auto& hit: ev.hits )
      {
        if( !hit.isRising ) continue;
        int l  = hit.layer;
        int s  = hit.side;
        int ch = hit.channel;
        if( l < 0 || l >= 3 || s < 0 || s >= 2 ) continue;
        auto key = std::make_pair( l, ch );
        if( s == 0 ) timeMap[key].t0 = hit.time;
        else
          timeMap[key].t1 = hit.time;
      }

      // For each valid pair, compute eta and phi, fill 2D histogram
      for( const auto& item: timeMap )
      {
        int   layer = item.first.first;
        int   ch    = item.first.second;
        float t1    = item.second.t0;
        float t2    = item.second.t1;
        if( t1 < -99998 || t2 < -99998 ) continue;

        // Eta from time difference (side 1 => eta = 0)
        double eta = ( t1 - t2 ) * SIGNAL_SPEED / 2.0 + DETECTOR_LENGTH / 2.0;
        if( eta < 0 ) eta = 0;
        if( eta > DETECTOR_LENGTH ) eta = DETECTOR_LENGTH;

        // Phi from channel number (assuming uniform distribution over width)
        // Use MAX_CHANNELS_PER_GROUP (48) to map channel to position
        double phi = ( ch + 0.5 ) * DETECTOR_WIDTH / MAX_CHANNELS_PER_GROUP;

        hPosEtaPhi[layer]->Fill( phi, eta );
      }
    }
    //other new functions
  }

  // 3.4 Finalization
  // Finalize: produce and save plots after all events processed
  void finalize()
  {
    if( !c1 ) c1 = new TCanvas( "c1", "Results", 1200, 800 );
    std::string pdfFile   = outPath( "analysis_results.pdf" );
    bool        firstPage = true;
    // ----- Channel count histograms (6 panels: 3 layers �� 2 sides) -----
    if( ENABLE_CHANNEL_COUNT )
    {
      c1->Clear();
      c1->Divide( 2, 3 );  // 3 rows, 2 columns
      int pad = 1;
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx = layer * 2 + side;
          c1->cd( pad++ );
          // Rising edge: blue
          hRisingCount[idx]->SetLineColor( kBlue );
          hRisingCount[idx]->SetFillColorAlpha( kBlue, 0.3 );
          hRisingCount[idx]->Draw( "hist" );
          // Falling edge: red
          hFallingCount[idx]->SetLineColor( kRed );
          hFallingCount[idx]->SetFillColorAlpha( kRed, 0.3 );
          hFallingCount[idx]->Draw( "hist same" );
          // Legend
          TLegend* leg = new TLegend( 0.12, 0.8, 0.32, 0.9 );
          leg->AddEntry( hRisingCount[idx], "Rising", "f" );
          leg->AddEntry( hFallingCount[idx], "Falling", "f" );
          leg->Draw();
        }
      }
      print_page( c1, pdfFile, "Channel Counts (Rising/Falling per Layer and Side)", true );
    }

    // ----- TOT histograms -----
    if( ENABLE_TOT_HIST )
    {
      c1->Clear();
      c1->Divide( 2, 3 );
      for( int i = 0; i < 6; ++i )
      {
        c1->cd( i + 1 );
        hTot[i]->Draw();
      }
      print_page( c1, pdfFile, "TOT Distribution per Layer and Side" );
    }

    // ----- Signal delay histograms -----
    if( ENABLE_SIGNAL_DELAY )
    {
      c1->Clear();
      c1->Divide( 2, 3 );
      int pad = 1;
      for( int layer = 0; layer < 3; ++layer )
      {
        for( int side = 0; side < 2; ++side )
        {
          int idx = layer * 2 + side;
          c1->cd( pad++ );
          hDelay[idx]->Draw();
        }
      }
      print_page( c1, pdfFile, "Signal Delay per Layer and Side" );
    }

    // ----- Trigger count histogram -----
    if( ENABLE_TRIGGER_COUNT )
    {
      c1->Clear();
      c1->SetLogy();
      hTriggerCount->Draw();
      print_page( c1, pdfFile, "Trigger Count per Event" );
    }

    // ----- Cluster size histograms -----
    if( ENABLE_CLUSTER_SIZE )
    {
      c1->Clear();
      c1->Divide( 2, 3 );
      int pad = 1;
      for( int l = 0; l < 3; ++l )
      {
        for( int s = 0; s < 2; ++s )
        {
          int idx = l * 2 + s;
          c1->cd( pad++ );
          hClusterSize[idx]->Draw();
        }
      }
      print_page( c1, pdfFile, "Cluster Size per Layer and Side" );
    }

    // ----- 2D position reconstruction -----
    if( ENABLE_POSITION_RECONSTRUCTION )
    {
      c1->Clear();
      c1->Divide( 3, 1 );
      for( int layer = 0; layer < 3; ++layer )
      {
        c1->cd( layer + 1 );
        gPad->SetLeftMargin( 0.12 );
        gPad->SetBottomMargin( 0.15 );
        gPad->SetTopMargin( 0.15 );
        gPad->SetRightMargin( 0.14 );
        hPosEtaPhi[layer]->GetYaxis()->SetTitleOffset( 1.6 );
        hPosEtaPhi[layer]->GetXaxis()->SetTitleOffset( 0.8 );
        hPosEtaPhi[layer]->GetXaxis()->SetLabelOffset( -0.0 );
        hPosEtaPhi[layer]->Draw( "COLZ" );
        gPad->Update();
        TPaveStats* st = (TPaveStats*)gPad->GetPrimitive( "stats" );
        if( st )
        {
          st->SetX1NDC( 0.72 );
          st->SetX2NDC( 1 );
          st->SetY1NDC( 0.85 );
          st->SetY2NDC( 0.95 );
          gPad->Modified();
          gPad->Update();
        }
        TLatex* latex = new TLatex();
        latex->SetTextSize( 0.045 );
        latex->SetTextAlign( 22 );
        latex->DrawLatexNDC( 0.5, 0.1, "Side eta 1" );
        latex->DrawLatexNDC( 0.5, 0.88, "Side eta 2" );
        latex->DrawLatexNDC( 0.08, 0.1, "Ch 0" );
        latex->DrawLatexNDC( 0.92, 0.1, "Ch 48" );
        delete latex;
      }
      print_page( c1, pdfFile, "2D Position Reconstruction (Eta vs Phi)" );
    }

    /*// ----- Rolling count histogram -----
    if (ENABLE_ROLLING && hRolling) {
	  c1->Clear();
	  c1->SetLogy();
	  hRolling->Draw();
	  printPage("BCID Rolling Count");
	}*/
    // Add other plots here...

    // Close PDF file
    c1->Clear();
    c1->Update();
    c1->Print( ( pdfFile + ")" ).c_str() );

    if( ENABLE_EFFICIENCY ) writeSummary();
    delete c1;
  }

  void ProcessEvent( const DCT::Event& dctEvent, TH1D* _my = nullptr )
  {
    EventData localEv;
    /*  // Separate last BCID and rolling counters for rising and falling edges
	    int lastBCID_R = -1;   // invalid initial
	    int lastBCID_F = -1;
	    int rollingR = 0;
	    int rollingF = 0;*/
    bool      first = true;
    int       bc0   = 0;
    for( const auto& dctHit: dctEvent.hits )
    {
      if( first )
      {
        bc0   = dctHit.getBCID() % BCID_PERIOD;
        first = false;
      }
      int bcid = dctHit.getBCID();
      if( bcid - bc0 > BCID_PERIOD / 2 ) bcid = bcid - BCID_PERIOD;
      else if( bcid - bc0 < -BCID_PERIOD / 2 )
        bcid = bcid + BCID_PERIOD;
      bool isRising = dctHit.getRise();

      /*// Select appropriate last BCID and rolling counter based on edge type
	  int* lastBCID_ptr = isRising ? &lastBCID_R : &lastBCID_F;
	  int* rolling_ptr = isRising ? &rollingR : &rollingF;
	  int period = isRising ? BCID_PERIOD_R : BCID_PERIOD_F;
	
	  // Detect rollover: if current BCID is smaller than previous (and previous valid)
	  if(*lastBCID_ptr != -1 && bcid - *lastBCID_ptr < -10) {
	    (*rolling_ptr)++;
	  }*/

      // Compute absolute time using the appropriate period and rolling count
      float time = static_cast<float>( bcid * BCID_CLOCK + dctHit.getFineTime() );

      // Fill hit or trigger
      if( !dctHit.isTrigger() )
      {
        Hit h;
        h.time     = time;
        h.channel  = dctHit.getStrip();
        h.layer    = dctHit.getLayer();
        h.side     = dctHit.getSide() - 1;
        h.isRising = isRising;
        localEv.hits.push_back( h );
      }
      else
      {
        Hit trigger;
        trigger.time     = time;
        trigger.channel  = dctHit.getStrip();
        trigger.layer    = dctHit.getLayer();
        trigger.side     = dctHit.getSide() - 1;
        trigger.isRising = isRising;
        localEv.triggerHits.push_back( trigger );
      }

      /*// Update the last BCID for this edge type
	*lastBCID_ptr = bcid;*/
    }
    processEvent( localEv, _my );

    /*// Debug output for rolling counts (only if enabled)
  if (ENABLE_ROLLING) {
    // Fill histogram with the total rolling count (use rising as representative, or combine)
	// Here we fill with rising rolling (or you can fill both separately)
	hRolling->Fill(rollingR);
	// Optionally fill falling too, but histogram is 1D; we can fill with sum or separate.
	// For simplicity, fill rising only.          
	// Print details if rolling is large
	if (rollingR > 2 || rollingF > 2) {
	  std::cout << "\n--- Event " << processed
	  << " | rising rolling = " << rollingR
	  << ", falling rolling = " << rollingF
	  << " (BCIDs: ";
	for (const auto& dctHit : dctEvent.hits) {
	  std::cout << dctHit.getBCID()
	  << (dctHit.isTrigger() ? "(T)" : "")
	  << (dctHit.getRise() ? "(r)" : "(f)")
	  << "(" << (int)dctHit.getSide() << ")"
	  << "  ";
	}
    std::cout << " ) ---" << std::endl;
  }
  }
  */
  }

  // ----- Two running modes -----
  // 3.5 File Processing
  // Mode 1: Process from a ROOT file containing DCT::Event RNTuple (via RDataFrame)
  void runFromFile( const std::string& filename )
  {
    auto reader = ROOT::RNTupleReader::Open( "hits", filename );
    if( !reader )
    {
      std::cerr << "Error: Cannot open RNTuple 'hits' in file " << filename << std::endl;
      return;
    }
    auto viewEvent    = reader->GetView<DCT::Event>( "event" );
    auto totalEntries = reader->GetNEntries();

    spdlog::info( "\n************************************\nProcessing {} events...", totalEntries );

    uint64_t processed = 0;
    for( auto entryId: reader->GetEntryRange() )
    {
      const DCT::Event& dctEvent = viewEvent( entryId );
      ProcessEvent( dctEvent, nullptr );
      processed++;
      if( processed % 5000 == 0 ) { spdlog::info( "Processed {}/{} events", processed, totalEntries ); }
    }
    spdlog::info( "Finished processing {} events.\n************************************\n", processed );
  }

  void writeSummary()
  {
    // Write to file: include original stats + new efficiencies
    std::ofstream fout( outPath( "run_summary.txt" ) );
    if( fout.is_open() )
    {
      fout << "Total events processed: " << totalEvents << "\n";
      fout << "Hits per layer (events with at least one hit):\n";
      for( int i = 0; i < 3; ++i ) fout << "  Layer " << i << ": " << layerHitCount[i] << "\n";
      fout << "Three-fold coincidence events: " << threeFold << "\n";
      fout << "Pair counts (other two layers) and layer efficiencies:\n";
      for( int i = 0; i < 3; ++i )
      {
        double oldEff = ( pairCount[i] > 0 ) ? (double)threeFold / pairCount[i] : 0.0;
        fout << "  Layer " << i << ": denominator = " << pairCount[i] << ", efficiency = " << oldEff << "\n";
      }
      fout << "\nEfficiency per (layer, side) with uncertainty:\n";
      for( int l = 0; l < 3; ++l )
      {
        for( int s = 0; s < 2; ++s )
        {
          int idx = l * 2 + s;
          fout << "  Layer " << l << ", Side " << ( s + 1 ) << ": efficiency = " << m_efficiencies[idx].efficiency() << " +/- " << m_efficiencies[idx].error() << " (denominator = " << m_efficiencies[idx].denominator() << ")\n";
        }
      }
      fout.close();
    }
    else
    {
      std::cerr << "Warning: Could not write " << outPath( "run_summary.txt" ) << "\n";
    }

    // Simplified terminal output
    std::cout << "========================================" << std::endl;
    std::cout << "Brief Report of the analysis" << std::endl;
    std::cout << "Total events processed: " << totalEvents << std::endl;
    for( int l = 0; l < 3; ++l )
    {
      for( int s = 0; s < 2; ++s )
      {
        int idx = l * 2 + s;
        std::cout << "Layer " << l << ", Side " << ( s + 1 ) << " efficiency: " << m_efficiencies[idx].efficiency() << " +/- " << m_efficiencies[idx].error() << std::endl;
      }
    }
    std::cout << "========================================" << std::endl;
  }

  //  4. Private Member Variables
private:
  void createCanvas()
  {
    if( !getCanvas( "toto" ) ) m_canvas["toto"] = new TCanvas( "gg", "ggg" );
  }
  TCanvas* getCanvas( const std::string name )
  {
    if( m_canvas.find( name ) != m_canvas.end() ) return m_canvas[name];
    else
      return nullptr;
  }
  std::map<std::string, TCanvas*> m_canvas;
  std::string                     outPath( const std::string& fname )
  {
    if( fOutputDir.empty() ) return fname;
    return fOutputDir + "/" + fname;
  };
  void print_page( TCanvas* can, const std::string file, const std::string title, bool first_page = true )
  {
    can->SetTitle( title.c_str() );
    if( first_page ) can->Print( ( file + "(" ).c_str(), ( "Title:" + title ).c_str() );
    else
      can->Print( file.c_str(), ( "Title:" + title ).c_str() );
  }

  TCanvas*             c1{ nullptr };
  // Histogram pointers
  // Histograms for channel counting: rising and falling edges, per (layer,side)
  std::array<TH1F*, 6> hRisingCount{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
  std::array<TH1F*, 6> hFallingCount{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

  TH1F*                hTotalHits{ nullptr };
  TH1F*                hTriggerHits{ nullptr };
  std::array<TH1F*, 6> hTot{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
  ;
  std::array<TH1F*, 6> hDelay{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
  ;
  TH1F* hTriggerCount{ nullptr };  // Histogram of number of trigger hits per event

  // Counters for new efficiency definition (event-based)
  long long totalEvents      = 0;
  long long layerHitCount[3] = { 0, 0, 0 };  // events with at least one hit in each layer
  long long threeFold        = 0;            // events with hits in all 3 layers
  long long pairCount[3]     = { 0, 0, 0 };  // for layer i: events with hits in the other two layers

  std::string fOutputDir;  // output directory for all result files

  // For efficiency per (layer, side)
  std::array<Efficiency, 6> m_efficiencies;

  std::array<TH1F*, 6> hClusterSize{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

  std::array<TH2F*, 3> hPosEtaPhi{ nullptr, nullptr, nullptr };  // Eta vs Phi per layer

  TH1F* hRolling{ nullptr };  // Histogram of BCID rolling count per hit
};
