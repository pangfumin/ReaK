
/*
 *    Copyright 2014 Sven Mikael Persson
 *
 *    THIS SOFTWARE IS DISTRIBUTED UNDER THE TERMS OF THE GNU GENERAL PUBLIC LICENSE v3 (GPLv3).
 *
 *    This file is part of ReaK.
 *
 *    ReaK is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    ReaK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with ReaK (as LICENSE in the root folder).  
 *    If not, see <http://www.gnu.org/licenses/>.
 */

#include "target_pred_config_widget.hpp"

#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "serialization/archiver_factory.hpp"

namespace ReaK {
  
namespace rkqt {


static QString last_used_path;



TargetPredConfigWidget::TargetPredConfigWidget(QWidget * parent, Qt::WindowFlags flags) :
                                               QDockWidget(tr("Predictor"), parent, flags),
                                               Ui::TargetPredConfig(),
                                               inertia_tensor(1.0, 0.0, 0.0, 1.0, 0.0, 1.0),
                                               IMU_orientation(),
                                               IMU_location(),
                                               earth_orientation(),
                                               mag_field_direction(1.0,0.0,0.0)
{
  this->QDockWidget::setWidget(new QWidget(this));
  setupUi(this->QDockWidget::widget());
  
  connect(this->kf_model_selection, SIGNAL(currentIndexChanged(int)), this, SLOT(onUpdateAvailableOptions(int)));
  connect(this->actionValuesChanged, SIGNAL(triggered()), this, SLOT(onConfigsChanged()));
  connect(this->load_button, SIGNAL(clicked()), this, SLOT(loadPredictorConfig()));
  connect(this->save_button, SIGNAL(clicked()), this, SLOT(savePredictorConfig()));
  
  
  updateConfigs();
  
};

TargetPredConfigWidget::~TargetPredConfigWidget() {
  delete this->QDockWidget::widget();
};


double TargetPredConfigWidget::getTimeStep() const {
  return this->time_step_spin->value();
};


double TargetPredConfigWidget::getMass() const {
  return this->mass_spin->value();
};

mat<double,mat_structure::diagonal> TargetPredConfigWidget::getInputDisturbance() const {
  mat<double,mat_structure::diagonal> result(6,true);
  result(2,2) = result(1,1) = result(0,0) = this->Qf_spin->value();
  result(5,5) = result(4,4) = result(3,3) = this->Qt_spin->value();
  return result;
};

mat<double,mat_structure::diagonal> TargetPredConfigWidget::getMeasurementNoise() const {
  
  std::size_t m_noise_size = 6;
  if( this->gyro_check->isChecked() )
    m_noise_size = 9;
  if( this->IMU_check->isChecked() )
    m_noise_size = 15;
  
  mat<double,mat_structure::diagonal> result(m_noise_size,true);
  result(2,2) = result(1,1) = result(0,0) = this->Rpos_spin->value();
  result(5,5) = result(4,4) = result(3,3) = this->Rang_spin->value();
  if( this->gyro_check->isChecked() ) {
    result(8,8) = result(7,7) = result(6,6) = this->Rgyro_spin->value();
  };
  if( this->IMU_check->isChecked() ) {
    result(11,11) = result(10,10) = result(9,9) = this->Racc_spin->value();
    result(14,14) = result(13,13) = result(12,12) = this->Rmag_spin->value();
  };
  
  return result;
};


double TargetPredConfigWidget::getTimeHorizon() const {
  return this->horizon_spin->value();
};

double TargetPredConfigWidget::getPThreshold() const {
  return this->Pthreshold_spin->value();
};


std::string TargetPredConfigWidget::getServerAddress() const {
  return this->ip_addr_edit->text().toStdString();
};

int TargetPredConfigWidget::getPortNumber() const {
  return this->port_spin->value();
};

bool TargetPredConfigWidget::useUDP() const {
  return this->udp_radio->isChecked();
};

bool TargetPredConfigWidget::useTCP() const {
  return this->tcp_radio->isChecked();
};

std::string TargetPredConfigWidget::getStartScript() const {
  return this->start_script_edit->text().toStdString();
};


void TargetPredConfigWidget::onConfigsChanged() {
  
};


void TargetPredConfigWidget::updateConfigs() {
  
};



void TargetPredConfigWidget::onUpdateAvailableOptions(int filter_method) {
  
  switch(filter_method) {
    case 0:  // IEKF
      this->IMU_check->setChecked(false);
      this->IMU_check->setEnabled(false);
      break;
    case 1:  // IMKF
    case 2:  // IMKFv2
    default:
      this->IMU_check->setEnabled(true);
      break;
  };
  
};


void TargetPredConfigWidget::savePredictorConfig() {
  QString fileName = QFileDialog::getSaveFileName(
    this, tr("Save Predictor Configurations..."), last_used_path,
    tr("Target Predictor Configurations (*.tpred.rkx *.tpred.rkb *.tpred.pbuf)"));
  
  if( fileName == tr("") )
    return;
  
  last_used_path = QFileInfo(fileName).absolutePath();
  
  onConfigsChanged();
  
  std::string filtering_method = "iekf";
  switch(this->kf_model_selection->currentIndex()) {
    case 0:  // IEKF
      filtering_method = "iekf";
      break;
    case 1:  // IMKF
      filtering_method = "imkf";
      break;
    case 2:  // IMKFv2
      filtering_method = "imkfv2";
      break;
  };
  
  std::string predictive_assumption = "no_meas";
  switch(this->predict_assumption_selection->currentIndex()) {
    case 0:  // No future measurements
      predictive_assumption = "no_meas";
      break;
    case 1:  // Maximum-likelihood Measurements
      predictive_assumption = "ml_meas";
      break;
    case 2:  // Full certainty
      predictive_assumption = "certain";
      break;
  };
  
  double mass = this->getMass();
  double time_step = this->getTimeStep();
  
  mat<double,mat_structure::diagonal> input_disturbance = this->getInputDisturbance();
  mat<double,mat_structure::diagonal> measurement_noise = this->getMeasurementNoise();
  
  double max_time_horizon = this->getTimeHorizon();
  double cov_norm_threshold = this->getPThreshold();
  
  std::string server_address = this->getServerAddress();
  int server_port = this->getPortNumber();
  std::string server_protocol = "tcp";
  if(this->useUDP())
    server_protocol = "udp";
  std::string start_script = this->getStartScript();
  
  try {
    (*serialization::open_oarchive(fileName.toStdString())) 
      & RK_SERIAL_SAVE_WITH_NAME(filtering_method)
      & RK_SERIAL_SAVE_WITH_NAME(predictive_assumption)
      & RK_SERIAL_SAVE_WITH_NAME(mass)
      & RK_SERIAL_SAVE_WITH_NAME(inertia_tensor)
      & RK_SERIAL_SAVE_WITH_NAME(time_step)
      & RK_SERIAL_SAVE_WITH_NAME(IMU_orientation)
      & RK_SERIAL_SAVE_WITH_NAME(IMU_location)
      & RK_SERIAL_SAVE_WITH_NAME(earth_orientation)
      & RK_SERIAL_SAVE_WITH_NAME(mag_field_direction)
      & RK_SERIAL_SAVE_WITH_NAME(input_disturbance)
      & RK_SERIAL_SAVE_WITH_NAME(measurement_noise)
      & RK_SERIAL_SAVE_WITH_NAME(max_time_horizon)
      & RK_SERIAL_SAVE_WITH_NAME(cov_norm_threshold)
      & RK_SERIAL_SAVE_WITH_NAME(server_address)
      & RK_SERIAL_SAVE_WITH_NAME(server_port)
      & RK_SERIAL_SAVE_WITH_NAME(server_protocol)
      & RK_SERIAL_SAVE_WITH_NAME(start_script);
  } catch(...) {
    QMessageBox::information(this,
                "File Type Not Supported!",
                "Sorry, this file-type is not supported!",
                QMessageBox::Ok);
    return;
  };
  
};

void TargetPredConfigWidget::loadPredictorConfig() {
  QString fileName = QFileDialog::getOpenFileName(
    this, tr("Open Predictor Configurations..."), last_used_path,
    tr("Target Predictor Configurations (*.tpred.rkx *.tpred.rkb *.tpred.pbuf)"));
  
  if( fileName == tr("") )
    return;
  
  last_used_path = QFileInfo(fileName).absolutePath();
  
  
  double mass, time_step, max_time_horizon, cov_norm_threshold;
  mat<double,mat_structure::diagonal> input_disturbance, measurement_noise;
  
  std::string filtering_method, predictive_assumption, server_address, server_protocol, start_script;
  int server_port;
  
  try {
    (*serialization::open_iarchive(fileName.toStdString())) 
      & RK_SERIAL_LOAD_WITH_NAME(filtering_method)
      & RK_SERIAL_LOAD_WITH_NAME(predictive_assumption)
      & RK_SERIAL_LOAD_WITH_NAME(mass)
      & RK_SERIAL_LOAD_WITH_NAME(inertia_tensor)
      & RK_SERIAL_LOAD_WITH_NAME(time_step)
      & RK_SERIAL_LOAD_WITH_NAME(IMU_orientation)
      & RK_SERIAL_LOAD_WITH_NAME(IMU_location)
      & RK_SERIAL_LOAD_WITH_NAME(earth_orientation)
      & RK_SERIAL_LOAD_WITH_NAME(mag_field_direction)
      & RK_SERIAL_LOAD_WITH_NAME(input_disturbance)
      & RK_SERIAL_LOAD_WITH_NAME(measurement_noise)
      & RK_SERIAL_LOAD_WITH_NAME(max_time_horizon)
      & RK_SERIAL_LOAD_WITH_NAME(cov_norm_threshold)
      & RK_SERIAL_LOAD_WITH_NAME(server_address)
      & RK_SERIAL_LOAD_WITH_NAME(server_port)
      & RK_SERIAL_LOAD_WITH_NAME(server_protocol)
      & RK_SERIAL_LOAD_WITH_NAME(start_script);
      
  } catch(...) {
    QMessageBox::information(this,
                "File Type Not Supported!",
                "Sorry, this file-type is not supported!",
                QMessageBox::Ok);
    return;
  };
  
  if(filtering_method == "iekf") 
    this->kf_model_selection->setCurrentIndex(0);
  else if(filtering_method == "imkf") 
    this->kf_model_selection->setCurrentIndex(1);
  else if(filtering_method == "imkfv2") 
    this->kf_model_selection->setCurrentIndex(2);
  else
    this->kf_model_selection->setCurrentIndex(0);
  
  if(predictive_assumption == "no_meas")
    this->predict_assumption_selection->setCurrentIndex(0);
  else if(predictive_assumption == "ml_meas")
    this->predict_assumption_selection->setCurrentIndex(1);
  else if(predictive_assumption == "certain")
    this->predict_assumption_selection->setCurrentIndex(2);
  else
    this->predict_assumption_selection->setCurrentIndex(0);
  
  this->mass_spin->setValue(mass);
  this->time_step_spin->setValue(time_step);
  
  this->Qf_spin->setValue((input_disturbance(0,0) + input_disturbance(1,1) + input_disturbance(2,2)) / 3.0);
  this->Qt_spin->setValue((input_disturbance(3,3) + input_disturbance(4,4) + input_disturbance(5,5)) / 3.0);
  
  this->Rpos_spin->setValue((measurement_noise(0,0) + measurement_noise(1,1) + measurement_noise(2,2)) / 3.0);
  this->Rang_spin->setValue((measurement_noise(3,3) + measurement_noise(4,4) + measurement_noise(5,5)) / 3.0);
  if(measurement_noise.get_col_count() > 6) {
    this->Rgyro_spin->setValue((measurement_noise(6,6) + measurement_noise(7,7) + measurement_noise(8,8)) / 3.0);
    if(measurement_noise.get_col_count() > 9) {
      this->Racc_spin->setValue((measurement_noise(9,9) + measurement_noise(10,10) + measurement_noise(11,11)) / 3.0);
      this->Rmag_spin->setValue((measurement_noise(12,12) + measurement_noise(13,13) + measurement_noise(14,14)) / 3.0);
    };
  };
  
  this->horizon_spin->setValue(max_time_horizon);
  this->Pthreshold_spin->setValue(cov_norm_threshold);
  
  this->ip_addr_edit->setText(QString::fromStdString(server_address));
  this->port_spin->setValue(server_port);
  this->udp_radio->setChecked(server_protocol == "udp");
  this->tcp_radio->setChecked(server_protocol != "udp");
  this->start_script_edit->setText(QString::fromStdString(start_script));
  
  updateConfigs();
};



};

};














