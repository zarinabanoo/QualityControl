// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file     QualityObserver.cxx
/// \author   Marcel Lesch
///

#include "QualityControl/DatabaseInterface.h"
#include "QualityControl/MonitorObject.h"
#include "QualityControl/QcInfoLogger.h"
#include <TPC/QualityObserver.h>
#include <boost/property_tree/ptree.hpp>
#include <TPaveText.h>
#include <TLine.h>
#include <TText.h>
#include <TCanvas.h>

using namespace o2::quality_control;
using namespace o2::quality_control::core;
using namespace o2::quality_control::postprocessing;
using namespace o2::quality_control_modules::tpc;

void QualityObserver::configure(std::string name,
                                const boost::property_tree::ptree& config)
{
  mObserverName = config.get<std::string>("qc.postprocessing." + name + ".qualityObserverName");

  for (const auto& dataSourceConfig : config.get_child("qc.postprocessing." + name + ".qualityObserverConfig")) {
    Config dataConfig;

    dataConfig.groupTitle = dataSourceConfig.second.get<std::string>("groupTitle");
    dataConfig.path = dataSourceConfig.second.get<std::string>("path");

    std::vector<std::string> inputQO;
    std::vector<std::string> inputQOTitle;

    if (const auto& qoSources = dataSourceConfig.second.get_child_optional("inputObjects"); qoSources.has_value()) {
      for (const auto& QOsource : qoSources.value()) {
        inputQO.push_back(QOsource.second.data());
      }
    }
    if (const auto& qoTitlesources = dataSourceConfig.second.get_child_optional("inputObjectTitles"); qoTitlesources.has_value()) {
      for (const auto& QOTitlesource : qoTitlesources.value()) {
        inputQOTitle.push_back(QOTitlesource.second.data());
      }
    }
    if (inputQO.size() != inputQOTitle.size()) {
      ILOG(Error, Devel) << "in config of group" << dataConfig.groupTitle << ": Number of QOs does not match number of qo titles!" << ENDM;
    }
    dataConfig.qo = inputQO;
    dataConfig.qoTitle = inputQOTitle;
    mConfig.push_back(dataConfig);

    inputQO.clear();
    inputQOTitle.clear();
  } // for (const auto& dataSourceConfig : config.get_child("qc.postprocessing." + name + ".ratioConfig"))
}

void QualityObserver::initialize(Trigger, framework::ServiceRegistry&)
{
  for (const auto& config : mConfig) {
    mQualities[config.groupTitle] = std::vector<std::string>();
  }
  mColors[Quality::Bad.getName()] = kRed;
  mColors[Quality::Medium.getName()] = kOrange - 3;
  mColors[Quality::Good.getName()] = kGreen + 2;
  mColors[Quality::Null.getName()] = kGray + 2;
}

void QualityObserver::update(Trigger t, framework::ServiceRegistry& services)
{
  auto& qcdb = services.get<repository::DatabaseInterface>();
  getQualities(t, qcdb);
  generatePanel();
}

void QualityObserver::finalize(Trigger t, framework::ServiceRegistry&)
{
  generatePanel();
  delete mCanvas;
  mCanvas = nullptr;
}

void QualityObserver::getQualities(const Trigger& t,
                                   repository::DatabaseInterface& qcdb)
{
  for (const auto& config : mConfig) {

    if (mQualities[config.groupTitle].size() > 0) {
      mQualities[config.groupTitle].clear();
    }
    for (const auto& qualityobject : config.qo) {
      const auto qo = qcdb.retrieveQO(config.path + "/" + qualityobject, t.timestamp, t.activity);
      if (qo) {
        const auto quality = qo->getQuality();
        mQualities[config.groupTitle].push_back(quality.getName());
      } else {
        mQualities[config.groupTitle].push_back(Quality::Null.getName());
      }
    }
  }
} // void QualityObserver::getQualities(const Trigger& t, repository::DatabaseInterface& qcdb)

void QualityObserver::generatePanel()
{
  // Delete the existing plots before regenerating them.
  if (mCanvas) {
    getObjectsManager()->stopPublishing(mObserverName);
    delete mCanvas;
    mCanvas = nullptr;
  }

  // Draw the ratio on a new canvas.
  TCanvas* c = new TCanvas();
  c->SetName(mObserverName.c_str());
  c->SetTitle(mObserverName.c_str());
  c->cd(1);

  TPaveText* pt = new TPaveText(0.05, 0.05, .95, .95);
  pt->SetLineColor(0);
  pt->SetFillColor(0);
  pt->SetBorderSize(1);

  for (const auto& config : mConfig) {

    pt->AddText(""); // Emtpy line needed. AddLine() places the line to the first entry of the for loop. Check late
    TText* GroupText = pt->AddText(config.groupTitle.data());
    ((TText*)pt->GetListOfLines()->Last())->SetTextAlign(22);
    for (int i = 0; i < config.qoTitle.size(); i++) {
      pt->AddText(Form("%s = #color[%d]{%s}", config.qoTitle.at(i).data(), mColors[mQualities[config.groupTitle].at(i).data()], mQualities[config.groupTitle].at(i).data()));
      // To-Check: SetTextAlign does currently not work in QCG
      ((TText*)pt->GetListOfLines()->Last())->SetTextAlign(12);
    }

    // To-Check: AddLine broken for qcg. Does not simply append line
    pt->AddLine();
    ((TLine*)pt->GetListOfLines()->Last())->SetLineWidth(3);
    ((TLine*)pt->GetListOfLines()->Last())->SetLineStyle(9);
  }

  pt->Draw();
  mCanvas = c;
  getObjectsManager()->startPublishing(c);

} // void QualityObserver::generatePanel()