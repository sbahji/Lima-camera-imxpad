
//###########################################################################
// This file is part of LImA, a Library for Image
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
//
// XpadCamera.cpp
// Created on: August 01, 2013
// Author: Hector PEREZ PONCE

#include <sstream>
#include <iostream>
#include <string>
#include <math.h>
#include <iomanip>
#include "imXpadCamera.h"
#include "lima/Exceptions.h"
#include "lima/Debug.h"
#include <unistd.h>
#include <sys/stat.h>
#include <ostream>
#include <fstream>


using namespace lima;
using namespace lima::imXpad;

#define CHECK_DETECTOR_ACCESS \
{ \
	if (m_thread_running == false || (m_thread_running && m_process_id >0) || (m_acq_frame_nb == m_nb_frames)) \
	{ \      
} \
	else \
	{ \
	    return; \
	} \
} \

//---------------------------
//- utility thread
//---------------------------

class Camera::AcqThread: public Thread
{
	DEB_CLASS_NAMESPC(DebModCamera, "Camera", "AcqThread");
public:
	AcqThread(Camera &aCam);
	virtual ~AcqThread();

protected:
	virtual void threadFunction();

private:
	Camera& m_cam;
} ;

//---------------------------
// @brief  Ctor
//---------------------------m_npixels

Camera::Camera(std::string hostname, int port, unsigned int moduleMask) : m_host_name(hostname), m_port(port)
{
	DEB_CONSTRUCTOR();

	//use module mask to enable/disable some modules. 
	//Do not apply if 0, in order to keep compatibility with others versions
	if(moduleMask!=0)
		m_module_mask = moduleMask;
	
	m_acq_thread = new AcqThread(*this);
	m_acq_thread->start();

	m_xpad = new XpadClient();
	m_xpad_alt = new XpadClient();

	if (m_xpad->connectToServer(m_host_name, m_port) < 0)
	{
		THROW_HW_ERROR(Error) << "[ " << m_xpad->getErrorMessage() << " ]";
	}
	if (m_xpad_alt->connectToServer(m_host_name, m_port) < 0)
	{
		THROW_HW_ERROR(Error) << "[ " << m_xpad_alt->getErrorMessage() << " ]";
	}
	m_state.state = XpadStatus::Idle;
	std::string xpad_type, xpad_model;

	init();
	getDetectorTypeFromHardware(xpad_type);
	m_xpad_type = xpad_type;
	getDetectorModelFromHardware(xpad_model);
	m_xpad_model = xpad_model;
	getModuleMask();
	getChipMask();
	getModuleNumber();
	getChipNumber();
	setImageType(Bpp32);
	setNbFrames(1);
	setAcquisitionMode(0); //standard
	setExpTime(1);
	setLatTime(5000);
	setOverflowTime(4000);
	setImageFileFormat(1); //binary
	setGeometricalCorrectionFlag(1);
	setFlatFieldCorrectionFlag(0);
	setImageTransferFlag(1);
	setTrigMode(IntTrig);
	setOutputSignalMode(0);
	setStackImages(1);
	setWaitAcqEndTime(10000);
	getBurstNumber();


}

Camera::~Camera()
{
	DEB_DESTRUCTOR();
	quit();
}

int Camera::init()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::init ***********";
	m_acq_frame_nb = 0;
	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "Init";
	m_xpad->sendWait(cmd.str(), ret);

	if (ret == 1 )
		throw LIMA_HW_EXC(Error, "Detector BUSY!");
	else if (ret == -1)
		throw LIMA_HW_EXC(Error, "xpadInit FAILED!");

	m_xpad_alt->sendWait(cmd.str(), ret);

	if (ret == 1 )
		throw LIMA_HW_EXC(Error, "Detector BUSY!");
	else if (ret == -1)
		throw LIMA_HW_EXC(Error, "xpadInit FAILED!");

	DEB_TRACE() << "********** Outside of Camera::init ***********";
	
	//use module mask to enable/disable some modules. 
	//Do not apply if 0, in order to keep compatibility with others versions
	if(m_module_mask!=0)
		setModuleMask(m_module_mask);
	return ret;
}

void Camera::quit()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::exit ***********";

	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "Exit";
	m_xpad->sendNoWait(cmd.str());
	m_xpad_alt->sendNoWait(cmd.str());

	DEB_TRACE() << "********** Outside of Camera::exit ***********";
}

void Camera::reset()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::reset ***********";

	std::stringstream cmd1;
	cmd1 << "ResetDetector";
	m_xpad->sendNoWait(cmd1.str());
	DEB_TRACE() << "Reset of detector  -> OK";

	DEB_TRACE() << "********** Outside of Camera::reset ***********";
}

int Camera::prepareAcq()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::prepareAcq ***********";

	//waitAcqEnd();

	int value;
	std::stringstream cmd1;

	m_image_file_format = 1;

	cmd1	<< "SetExposureParameters "
	 << m_nb_frames << " "
	 << m_exp_time_usec << " "
	 << m_lat_time_usec << " "
	 << m_overflow_time << " "
	 << m_xpad_trigger_mode << " "
	 << m_xpad_output_signal_mode << " "
	 << m_geometrical_correction_flag << " "
	 << m_flat_field_correction_flag << " "
	 << m_image_transfer_flag << " "
	 << m_image_file_format << " "
	 << m_acquisition_mode << " "
	 << m_stack_images << " "
	 << "/opt/imXPAD/tmp_corrected/";

	m_xpad->sendWait(cmd1.str(), value);

	if (!value)
	{
		DEB_TRACE() << "Default exposure parameter applied SUCCESFULLY";

		if (!m_image_transfer_flag)
		{
			std::stringstream fileName;
			fileName << "/opt/imXPAD/tmp_corrected/burst_" << m_burst_number << "_*";
			remove(fileName.str().c_str());
			//m_burst_number = getBurstNumber();

			//cout << "Burst number = " << m_burst_number << endl;
		}
	}
	//else
	//throw LIMA_HW_EXC(Error, "SetExposure FAILED!");

	DEB_TRACE() << "********** Outside of Camera::prepareAcq ***********";

	return value;
}

void Camera::startAcq()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::startAcq ***********";

	if(0 < m_nb_frames)
	{
		waitAcqEnd();

		m_acq_frame_nb = 0;
		StdBufferCbMgr& buffer_mgr = m_buffer_ctrl_obj.getBuffer();
		buffer_mgr.setStartTimestamp(Timestamp::now());

		m_wait_flag = false;
		m_quit = false;
		m_process_id = 0;
		m_cond.broadcast();

		while (!m_thread_running)
			m_cond.wait();
	}
	else
	{
		DEB_ERROR() << "Error: Live video not supported !!!";
		throw LIMA_HW_EXC(Error, "Live video not supported !!!");
	}

	DEB_TRACE() << "********** Outside of Camera::startAcq ***********";
}

void Camera::waitAcqEnd()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::waitAcqEnd ***********";

	while (m_thread_running)
		m_cond.wait();

	usleep(m_dead_time);

	DEB_TRACE() << "********** Outside of Camera::waitAcqEnd ***********";
}

void Camera::setWaitAcqEndTime(unsigned int time)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::setWaitAcqEndTime ***********";

	m_dead_time = time;

	DEB_TRACE() << "********** Outside of Camera::waitAcqEnd ***********";
}

void Camera::stopAcq()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::stopAcq ***********";

	//waitAcqEnd();

	DEB_TRACE() << "********** Outside of Camera::stopAcq ***********";
}

void Camera::sendExposeCommand()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::sendExposeCommmand ***********";

	m_xpad->sendExposeCommand();

	DEB_TRACE() << "********** Outside of Camera::sendExposeCommand ***********";
}

int Camera::readFrameExpose(void *bptr, int frame_nb)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::readFrameExpose ***********";

	DEB_TRACE() << "reading frame " << frame_nb;

	int ret;

	ret = m_xpad->getDataExpose(bptr, (m_pixel_depth == Camera::B2) ? 0 : 1);

	DEB_TRACE() << "********** Outside of Camera::readFrameExpose ***********";

	return ret;
}

int Camera::getDataExposeReturn()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getExposeCommandReturn ***********";

	int ret;
	m_xpad->getExposeCommandReturn(ret);

	DEB_TRACE() << "********** Outside of Camera::getExposeCommandReturn ***********";

	return ret;
}

void Camera::getStatus(XpadStatus& status)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getStatus ***********";
	CHECK_DETECTOR_ACCESS
	std::stringstream cmd;
	std::string str;
	unsigned short pos;
	cmd << "GetDetectorStatus";

	m_xpad_alt->sendWaitCustom(cmd.str(), str);
	pos = str.find(".");
	std::string state = str.substr (0, pos);
	if (state == "Idle")
	{
		status.state = XpadStatus::Idle;
	}
	else if (state == "Acquiring")
	{
		status.state = XpadStatus::Acquiring;
	}
	else if (state == "Loading/Saving_calibration")
	{
		status.state = XpadStatus::CalibrationManipulation;
	}
	else if (state == "Calibrating")
	{
		status.state = XpadStatus::Calibrating;
	}
	else if (state == "Digital_Test")
	{
		status.state = XpadStatus::DigitalTest;
	}
	else if (state == "Resetting")
	{
		status.state = XpadStatus::Resetting;
	}
	m_state.state = status.state;
	DEB_TRACE() << "XpadStatus.state is [" << status.state << "]";

	DEB_TRACE() << "********** Outside of Camera::getStatus ***********";

}

int Camera::getNbHwAcquiredFrames()
{
	DEB_MEMBER_FUNCT();
	return m_acq_frame_nb;
}

void Camera::AcqThread::threadFunction()
{
	DEB_MEMBER_FUNCT();

	AutoMutex aLock(m_cam.m_cond.mutex());
	StdBufferCbMgr& buffer_mgr = m_cam.m_buffer_ctrl_obj.getBuffer();

	while (1)
	{
		while (m_cam.m_wait_flag || m_cam.m_quit)
		{
			DEB_TRACE() << "Acquisition thread waiting...";
			DEB_TRACE() << "wait flag value = " << m_cam.m_wait_flag;
			DEB_TRACE() << "quit flag value = " << m_cam.m_quit;
			m_cam.m_thread_running = false;
			m_cam.m_cond.broadcast();
			aLock.unlock();
			m_cam.m_cond.wait();
		}

		DEB_TRACE() << "Acqisition thread running...";
		m_cam.m_thread_running = true;
		m_cam.m_cond.broadcast();
		aLock.unlock();

		switch (m_cam.m_process_id)
		{
			case 0:
			{  //startAcq()

				DEB_TRACE() << "Starting to acquire images...";

				if (m_cam.m_quit == false)
				{
					m_cam.sendExposeCommand();

					bool continueFlag = true;
					int ret;

					if (m_cam.m_image_transfer_flag == 1)
					{
						while (continueFlag && (!m_cam.m_nb_frames || m_cam.m_acq_frame_nb < m_cam.m_nb_frames))
						{

							DEB_TRACE() << m_cam.m_acq_frame_nb;
							void *bptr = buffer_mgr.getFrameBufferPtr(m_cam.m_acq_frame_nb);

							ret = m_cam.readFrameExpose(bptr, m_cam.m_acq_frame_nb);

							if ( ret == 0 )
							{
								HwFrameInfoType frame_info;
								frame_info.acq_frame_nb = m_cam.m_acq_frame_nb;
								continueFlag = buffer_mgr.newFrameReady(frame_info);
								DEB_TRACE() << "acqThread::threadFunction() newframe ready ";

								++m_cam.m_acq_frame_nb;

								DEB_TRACE() << "acquired " << m_cam.m_acq_frame_nb << " frames, required " << m_cam.m_nb_frames << " frames";
							}
							else
							{
								continueFlag = false;
								DEB_TRACE() << "ABORT detected";
							}
						}
						m_cam.getDataExposeReturn();
					}
					else
					{

						DEB_TRACE() << m_cam.m_acq_frame_nb;
						DEB_TRACE() << m_cam.m_image_size.getWidth() << " " << m_cam.m_image_size.getHeight();

						uint numData = m_cam.m_image_size.getWidth() * m_cam.m_image_size.getHeight();

						uint16_t *buffer_short;
						uint32_t *buffer_int;

						while (continueFlag && (!m_cam.m_nb_frames || m_cam.m_acq_frame_nb < m_cam.m_nb_frames) && m_cam.m_quit == false)
						{

							void *bptr = buffer_mgr.getFrameBufferPtr(m_cam.m_acq_frame_nb);

							if (m_cam.m_pixel_depth == Camera::B2)
								buffer_short = (uint16_t *) bptr;
							else
								buffer_int = (uint32_t *) bptr;

							std::stringstream fileName;

							fileName << "/opt/imXPAD/tmp_corrected/burst_" << m_cam.m_burst_number << "_image_" << m_cam.m_acq_frame_nb  << ".bin";

							while (access( fileName.str().c_str(), F_OK ) == -1)
							{
								if (m_cam.m_quit)
								{
									DEB_TRACE() << "ABORT detected";
									break;
								}
							}

							if (access( fileName.str().c_str(), F_OK ) == 0)
							{

								std::ifstream file(fileName.str().c_str(), std::ios::in | std::ios::binary);

								if (file.is_open())
								{

									DEB_TRACE() << "OPEN FILE : " << (char *) buffer_int;
									file.read((char *) buffer_int, numData * sizeof (uint32_t));

									if (m_cam.m_pixel_depth == Camera::B2)
									{
										uint32_t count = 0;
										int i = 0;
										while (count < numData * sizeof (uint32_t))
										{
											buffer_short[i] = (uint16_t) (buffer_int[i]);
											count += sizeof (uint32_t);
											i++;
										}
									}
									file.close();
								}

								remove(fileName.str().c_str());

								HwFrameInfoType frame_info;
								frame_info.acq_frame_nb = m_cam.m_acq_frame_nb;
								continueFlag = buffer_mgr.newFrameReady(frame_info);
								//DEB_TRACE() << "acqThread::threadFunction() newframe ready ";
								++m_cam.m_acq_frame_nb;

								DEB_TRACE() << "acquired " << m_cam.m_acq_frame_nb << " frames, required " << m_cam.m_nb_frames << " frames";
							}
						}
					}
				}

				break;
			}
			case 1:
			{ //CalibrationOTN
				int ret;
				std::stringstream cmd;

				cmd <<  "CalibrationOTN " << m_cam.m_calibration_configuration;
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (ret == 0)
					DEB_TRACE() << "Calibration Over-The-Noise finished SUCCESFULLY";
				else if (ret == 1)
					DEB_TRACE() << "Calibration Over-The-Noise was ABORTED";
				else
					throw LIMA_HW_EXC(Error, "Calibration Over The Noise FAILED!");

				break;
			}
			case 2:
			{ //CalibrationOTNPulse
				int ret;
				std::stringstream cmd;

				cmd <<  "CalibrationOTNPulse " << m_cam.m_calibration_configuration;
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (ret == 0)
					DEB_TRACE() << "Calibration Over-The-Noise with PULSE finished SUCCESFULLY";
				else if (ret == 1)
					DEB_TRACE() << "Calibration Over-The-Noise was ABORTED";
				else
					throw LIMA_HW_EXC(Error, "Calibration Over The Noise with PULSE FAILED!");

				break;

			}
			case 3:
			{ //CalibrationBEAM
				int ret;
				std::stringstream cmd;

				cmd <<  "CalibrationBEAM " << m_cam.m_time << " " << m_cam.m_ITHL_max << " " << m_cam.m_calibration_configuration;
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (ret == 0)
					DEB_TRACE() << "Calibration BEAM finished SUCCESFULLY";
				else if (ret == 1)
					DEB_TRACE() << "Calibration BEAM was ABORTED";
				else
					throw LIMA_HW_EXC(Error, "Calibration BEAM FAILED!");

				break;

			}
			case 4:
			{ //loadCalibrationFromFile
				int ret1;

				size_t pos = m_cam.m_file_path.find(".cf");

				if (pos == std::string::npos)
				{
					m_cam.m_file_path.append(".cfg");
					pos = m_cam.m_file_path.length() - 4;
				}

				m_cam.m_file_path.replace(pos, 4, ".cfg");
				ret1 = m_cam.loadConfigGFromFile((char *) m_cam.m_file_path.c_str());

				if (ret1 == 0)
				{
					m_cam.m_file_path.replace(pos, 4, ".cfl");
					m_cam.loadConfigLFromFile((char *) m_cam.m_file_path.c_str());
				}

				break;
			}
			case 5:
			{ //saveCalibrationToFile
				int ret1;

				size_t pos = m_cam.m_file_path.find(".cf");

				if (pos == std::string::npos)
				{
					m_cam.m_file_path.append(".cfg");
					pos = m_cam.m_file_path.length() - 4;
				}
				m_cam.m_file_path.replace(pos, 4, ".cfg");
				ret1 = m_cam.saveConfigGToFile((char *) m_cam.m_file_path.c_str());

				if (ret1 == 0)
				{
					m_cam.m_file_path.replace(pos, 4, ".cfl");
					m_cam.saveConfigLToFile((char *) m_cam.m_file_path.c_str());
				}

				break;
			}
			case 6:
			{ //Load Default Config G values
				std::string ret;
				std::stringstream cmd;
				int value, regid;

				for (int i = 0; i < 7; i++)
				{
					switch (i)
					{
						case 0: regid = AMPTP;
							value = 0;
							break;
						case 1: regid = IMFP;
							value = 50;
							break;
						case 2: regid = IOTA;
							value = 40;
							break;
						case 3: regid = IPRE;
							value = 60;
							break;
						case 4: regid = ITHL;
							value = 25;
							break;
						case 5: regid = ITUNE;
							value = 100;
							break;
						case 6: regid = IBUFF;
							value = 0;
							break;
					}

					std::string register_name;
					switch (regid)
					{
						case 31: register_name = "AMPTP";
							break;
						case 59: register_name = "IMFP";
							break;
						case 60: register_name = "IOTA";
							break;
						case 61: register_name = "IPRE";
							break;
						case 62: register_name = "ITHL";
							break;
						case 63: register_name = "ITUNE";
							break;
						case 64: register_name = "IBUFF";
							break;
						default: register_name = "ITHL";
							break;
					}

					cmd.str(std::string());
					cmd << "LoadConfigG " << register_name << " " << value ;
					m_cam.m_xpad->sendWait(cmd.str(), ret);
				}

				if (ret.length() > 1)
					DEB_TRACE() << "Loading global configuratioin with values:\nAMPTP = 0, IMFP = 50, IOTA = 40, IPRE = 60, ITHL = 25, ITUNE = 100, IBUFF = 0";
				else
					throw LIMA_HW_EXC(Error, "Loading default global configuration values FAILED!");

				break;
			}
			case 7:
			{ //ITHL Increase
				int ret;
				std::stringstream cmd;

				cmd.str(std::string());
				cmd << "ITHLIncrease";
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (!ret)
					DEB_TRACE() << "ITHL was increased SUCCESFULLY";
				else
					throw LIMA_HW_EXC(Error, "ITHL increase FAILED!");

				break;
			}
			case 8:
			{  //ITHL Decrease
				int ret;
				std::stringstream cmd;

				cmd.str(std::string());
				cmd << "ITHLDecrease";
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (!ret)
					DEB_TRACE() << "ITHL was decreased SUCCESFULLY";
				else
					throw LIMA_HW_EXC(Error, "ITHL decrease FAILED!");

				break;
			}
			case 9:
			{  //LoadFloatConfigL
				int ret;
				std::stringstream cmd;

				cmd.str(std::string());
				cmd <<  "LoadFlatConfigL " << " " << m_cam.m_flat_value;
				m_cam.m_xpad->sendWait(cmd.str(), ret);

				if (!ret)
					DEB_TRACE() << "Loading local configurations values, with flat value: " <<  m_cam.m_flat_value << " -> OK" ;
				else
					throw LIMA_HW_EXC(Error, "Loading local configuratin with FLAT value FAILED!");

				break;
			}
		}
		aLock.lock();
		m_cam.m_quit = false;
		m_cam.m_wait_flag = true;
		m_cam.m_cond.broadcast();
	}

	DEB_TRACE() << "Acquisition thread finished";
	//cout << "--- End of " << __func__ << endl;
}

Camera::AcqThread::AcqThread(Camera& cam) :
m_cam(cam)
{
	AutoMutex aLock(m_cam.m_cond.mutex());
	m_cam.m_wait_flag = true;
	m_cam.m_quit = false;
	m_cam.m_cond.broadcast();
	aLock.unlock();
	pthread_attr_setscope(&m_thread_attr, PTHREAD_SCOPE_PROCESS);
}

Camera::AcqThread::~AcqThread()
{
	AutoMutex aLock(m_cam.m_cond.mutex());
	m_cam.m_quit = true;
	m_cam.m_cond.broadcast();
	aLock.unlock();
}

void Camera::getImageSize(Size& size)
{
	DEB_MEMBER_FUNCT();
	CHECK_DETECTOR_ACCESS
	std::string message, ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetImageSize";
	m_xpad->sendWait(cmd.str(), ret);
	int pos = ret.find("x");

	int row = atoi(ret.substr(0, pos).c_str());
	int columns = atoi(ret.substr(pos + 1, ret.length() - pos + 1).c_str());

	size = Size(columns, row);
}

void Camera::getPixelSize(double& size_x, double& size_y)
{
	DEB_MEMBER_FUNCT();
	size_x = 130; // pixel size is 130 micron
	size_y = 130;
}

void Camera::getImageType(ImageType& pixel_depth)
{
	DEB_MEMBER_FUNCT();
	switch ( m_pixel_depth )
	{
		case Camera::B2:
			pixel_depth = Bpp16;
			break;

		case Camera::B4:
			pixel_depth = Bpp32;
			break;
	}
}

void Camera::setImageType(ImageType pixel_depth)
{
	//this is what xpad generates but may need Bpp32 for accummulate
	DEB_MEMBER_FUNCT();
	switch ( pixel_depth )
	{
		case Bpp16:
			m_pixel_depth = B2;
			break;

		case Bpp32:
			m_pixel_depth = B4;
			break;
		default:
			DEB_ERROR() << "Pixel Depth is unsupported: only 16S or 32S bits is supported" ;
			throw LIMA_HW_EXC(Error, "Pixel Depth is unsupported: only 16S or 32S bits is supported");
			break;
	}
}

void Camera::getDetectorType(std::string& type)
{
	type = m_xpad_type;
}

void Camera::getDetectorTypeFromHardware(std::string& type)
{
	DEB_MEMBER_FUNCT();
	CHECK_DETECTOR_ACCESS
	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetDetectorType";
	m_xpad->sendWait(cmd.str(), type);

	m_xpad_type = type;
}

void Camera::getDetectorModel(std::string& model)
{
	model = m_xpad_model;
}

void Camera::getDetectorModelFromHardware(std::string& model)
{
	DEB_MEMBER_FUNCT();
	CHECK_DETECTOR_ACCESS
	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetDetectorModel";
	m_xpad->sendWait(cmd.str(), model);

	m_xpad_model = model;
}

HwBufferCtrlObj* Camera::getBufferCtrlObj()
{
	return &m_buffer_ctrl_obj;
}

void Camera::setTrigMode(TrigMode mode)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setTrigMode - " << DEB_VAR1(mode);
	DEB_PARAM() << DEB_VAR1(mode);

	switch ( mode )
	{
		case IntTrig:
			m_xpad_trigger_mode = 0;
			break;
		case ExtGate:
			m_xpad_trigger_mode = 1;
			break;
		case ExtTrigSingle:
			m_xpad_trigger_mode = 3;
			break;
		case ExtTrigMult:
			m_xpad_trigger_mode = 2;
			break;
		default:
			DEB_ERROR() << "Error: Trigger mode unsupported: only IntTrig, ExtGate, ExtTrigSingle and ExtTrigMult" ;
			throw LIMA_HW_EXC(Error, "Trigger mode unsupported: only IntTrig, ExtGate, ExtTrigSingle and ExtTrigMult");
			break;
	}
}

void Camera::getTrigMode(TrigMode& mode)
{
	DEB_MEMBER_FUNCT();
	//mode = m_trigger_mode;
	switch ( m_xpad_trigger_mode )
	{
		case 0:
			mode = IntTrig;
			break;
		case 1:
			mode = ExtGate;
			break;
		case 2:
			mode = ExtTrigSingle;
			break;
		case 3:
			mode = ExtTrigMult;
			break;
		default:
			break;
	}
	DEB_RETURN() << DEB_VAR1(mode);
}

void Camera::setExpTime(double exp_time_sec)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setExpTime - " << DEB_VAR1(exp_time_sec);

	m_exp_time_usec = exp_time_sec * 1e6;
}

void Camera::getExpTime(double& exp_time_sec)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::getExpTime";
	////	AutoMutex aLock(m_cond.mutex());

	exp_time_sec = m_exp_time_usec / 1e6;
	DEB_RETURN() << DEB_VAR1(exp_time_sec);
}

void Camera::setLatTime(double lat_time)
{
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << "Camera::setLatTime - " << DEB_VAR1(lat_time);

	m_lat_time_usec = lat_time * 1e6;
}

void Camera::getLatTime(double& lat_time)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::getLatTime";

	lat_time = m_lat_time_usec / 1e6;
	DEB_RETURN() << DEB_VAR1(lat_time);
}

void Camera::setNbFrames(int nb_frames)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setNbFrames - " << DEB_VAR1(nb_frames);

	m_nb_frames = nb_frames;
}

void Camera::getNbFrames(int& nb_frames)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::getNbFrames";
	DEB_RETURN() << DEB_VAR1(m_nb_frames);
	nb_frames = m_nb_frames;
}

bool Camera::isAcqRunning() const
{
	AutoMutex aLock(m_cond.mutex());
	return m_thread_running;
}

/////////////////////////
// XPAD Specific
/////////////////////////

std::string Camera::getUSBDeviceList(void)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getUSBDeviceList ***********";

	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetUSBDeviceList";
	m_xpad->sendWait(cmd.str(), message);

	DEB_TRACE() << "List of USB devices connected: " << message;

	DEB_TRACE() << "********** Outside of Camera::getUSBDeviceList ***********";

	return message;

}

int Camera::setUSBDevice(unsigned short device)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::setUSBDevice ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "SetUSBDevice " << device;
	m_xpad->sendWait(cmd.str(), ret);

	if (!ret)
		DEB_TRACE() << "Setting active USB device to " << device;
	else
		throw LIMA_HW_EXC(Error, "Setting USB device FAILED!");

	DEB_TRACE() << "********** Outside of Camera::setUSBDevice ***********";

	return ret;

}

/*int Camera::defineDetectorModel(unsigned short model){
DEB_MEMBER_FUNCT();
DEB_TRACE() << "********** Inside of Camera::DefineDetectorModel ***********";

int ret;
std::stringstream cmd;

cmd.str(std::string());
cmd << "DefineDetectorModel " << model;
m_xpad->sendWait(cmd.str(), ret);

if(!ret)
DEB_TRACE() << "Defining detector model to " << model;
else
throw LIMA_HW_EXC(Error, "Defining detector model FAILED");


DEB_TRACE() << "********** Outside of Camera::DefineDetectorModel ***********";

return ret;

}*/

int Camera::setModuleMask(unsigned int moduleMask)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::setModuleMask ***********";

	int ret;
	std::stringstream cmd;

	m_module_mask = moduleMask;

	cmd.str(std::string());
	cmd << "SetModuleMask " << moduleMask;
	m_xpad->sendWait(cmd.str(), ret);

	if (!ret)
		DEB_TRACE() << "Setting module mask to " << moduleMask;
	else
		throw LIMA_HW_EXC(Error, "Setting module mask FAILED!");

	DEB_TRACE() << "********** Outside of Camera::setModuleMask ***********";

	return ret;

}

void Camera::getModuleMask()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getModuleMask ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetModuleMask";
	m_xpad->sendWait(cmd.str(), ret);

	m_module_mask = ret;

	DEB_TRACE() << "********** Outside of Camera::getModuleMask ***********";
}

void Camera::getModuleNumber()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getModuleNumber ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetModuleNumber";
	m_xpad->sendWait(cmd.str(), ret);

	m_module_number = ret;

	DEB_TRACE() << "********** Outside of Camera::getModuleNumber ***********";
}

void Camera::getChipMask()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getChipMask ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetChipMask";
	m_xpad->sendWait(cmd.str(), ret);

	m_chip_mask = ret;

	DEB_TRACE() << "********** Outside of Camera::getChipMask ***********";
}

void Camera::getChipNumber()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getChipNumber ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetChipNumber";
	m_xpad->sendWait(cmd.str(), ret);

	m_chip_number = ret;

	DEB_TRACE() << "********** Outside of Camera::getChipNumber ***********";
}

int Camera::askReady()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::askReady ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "AskReady";
	m_xpad->sendWait(cmd.str(), ret);

	if (!ret)
	{
		DEB_TRACE() << "Module " << m_module_mask << " is READY";
	}
	else
		throw LIMA_HW_EXC(Error, "AskReady FAILED!");

	DEB_TRACE() << "********** Outside of Camera::askRead ***********";

	return ret;
}

int Camera::digitalTest(unsigned short mode)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::digitalTest ***********";

	int ret;
	std::stringstream cmd;

	std::string mode_name;

	switch (mode)
	{
		case 0: mode_name = "flat";
			break;
		case 1: mode_name = "strip";
			break;
		case 2: mode_name = "gradient";
			break;
		default: mode_name = "strip";
	}

	cmd.str(std::string());
	cmd << "SetGeometricalCorrectionFlag " << "false";
	m_xpad->sendWait(cmd.str(), ret);

	cmd.str(std::string());
	cmd << "DigitalTest " << mode_name.c_str();
	m_xpad->sendNoWait(cmd.str());

	int rows = IMG_LINE * m_module_number;
	int columns = IMG_COLUMN * m_chip_number;

	uint32_t buff[rows * columns];
	readFrameExpose(buff, 1);

	uint32_t val;
	//Saving Digital Test image to disk
	mkdir("./Images", S_IRWXU |  S_IRWXG |  S_IRWXO);
	std::ofstream file("./DigitalTest.bin", std::ios::out | std::ios::binary);
	if (file.is_open())
	{
		for (int i = 0;i < rows;i++)
		{
			for (int j = 0;j < columns;j++)
			{
				val = buff[i * columns + j];
				file.write((char *) &val, sizeof (uint32_t));
			}
		}
	}
	file.close();

	ret = getDataExposeReturn();

	if (!ret)
		DEB_TRACE() << "Digital Test performed SUCCESFULLY";
	else
		throw LIMA_HW_EXC(Error, "Digital Test FAILED!");

	DEB_TRACE() << "********** Outside of Camera::digitalTest ***********";

	return ret;
}

int Camera::loadConfigGFromFile(char *fpath)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::loadConfigGFromFile ***********";

	DEB_TRACE() << "LoadConfigGFromFile : " << fpath;
	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "LoadConfigGFromFile ";

	m_xpad->sendNoWait(cmd.str());

	ret = m_xpad->sendParametersFile(fpath);

	if (ret == 0)
		DEB_TRACE() << "Global configuration loaded from file SUCCESFULLY";
	else if (ret == 1)
		DEB_TRACE() << "Global configuration loaded from file was ABORTED";
	else
		throw LIMA_HW_EXC(Error, "Loading global configuration from file FAILED!");

	DEB_TRACE() << "********** Outside of Camera::loadConfigGFromFile ***********";

	return ret;
}

int Camera::saveConfigGToFile(char *fpath)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::saveConfigGToFile ***********";
	DEB_TRACE() << "saveConfigGToFile : " << fpath;
	int ret;
	unsigned short  regid;

	std::stringstream cmd;

	//File is being open to be writen
	std::ofstream file(fpath, std::ios::out);
	if (file.is_open())
	{

		std::string          retString;

		//All registers are being read
		for (unsigned short registro = 0; registro < 7; registro++)
		{
			switch (registro)
			{
				case 0: regid = AMPTP;
					break;
				case 1: regid = IMFP;
					break;
				case 2: regid = IOTA;
					break;
				case 3: regid = IPRE;
					break;
				case 4: regid = ITHL;
					break;
				case 5: regid = ITUNE;
					break;
				case 6: regid = IBUFF;
			}

			std::string register_name;
			switch (regid)
			{
				case 31: register_name = "AMPTP";
					break;
				case 59: register_name = "IMFP";
					break;
				case 60: register_name = "IOTA";
					break;
				case 61: register_name = "IPRE";
					break;
				case 62: register_name = "ITHL";
					break;
				case 63: register_name = "ITUNE";
					break;
				case 64: register_name = "IBUFF";
					break;
				default: register_name = "ITHL";
			}

			//Each register value is read from the detector
			cmd.str(std::string());
			cmd << "ReadConfigG " << register_name;
			m_xpad->sendWait(cmd.str(), retString);

			std::stringstream stream(retString.c_str());
			int length = retString.length();

			unsigned mdMask = 1;

			while (mdMask <= m_module_mask)
			{
				if (length > 1)
				{
					//Register values are being stored in the file
					file << mdMask << " ";
					file << regid << " ";
					for (int count = 0; count < 8; count++)
					{
						stream >> retString;
						if (count > 0)
							file << retString << " ";
					}
					file << std::endl;
					mdMask = mdMask << 1;
					ret = 0;
					if (mdMask <= m_module_mask)
					{
						stream >> retString;
					}

				}
				else
				{
					ret = -1;
					file.close();
					exit();
				}
			}
		}
		file.close();
	}

	if (ret == 0)
		DEB_TRACE() << "Global configuration saved to file SUCCESFULLY";
	else if (ret == 1)
		DEB_TRACE() << "Global configuration saved to file was ABORTED";
	else
		throw LIMA_HW_EXC(Error, "Saving global configuration to file FAILED!");


	DEB_TRACE() << "********** Outside of Camera::saveConfigGToFile ***********";

	return ret;
}

int Camera::loadConfigG(char *regID, unsigned short value)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::loadConfigG ***********";

	std::string ret;
	std::stringstream cmd;

	std::string register_name;
	register_name.append(regID);

	cmd.str(std::string());
	cmd << "LoadConfigG " << register_name.c_str() << " " << value ;
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::loadConfigG ***********";

	if (ret.length() > 1)
	{
		DEB_TRACE() << "Loading global register: " << register_name << " with value= " << value;
		return 0;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Loading global configuration FAILED!");
		return -1;
	}
}

int Camera::readConfigG(char *regID)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::readConfigG ***********";

	std::string message;
	std::stringstream cmd;
	int reg;

	unsigned short *ret = new unsigned short[8];

	std::string register_name;
	register_name.append(regID);

	if (register_name == "AMPTP")
		reg = 31;
	else if (register_name == "IMFP")
		reg = 59;
	else if (register_name == "IOTA")
		reg = 60;
	else if (register_name == "IPRE")
		reg = 61;
	else if (register_name == "ITHL")
		reg = 62;
	else if (register_name == "ITUNE")
		reg = 63;
	else if (register_name == "IBUFF")
		reg = 64;
	else
		reg = 62;

	cmd.str(std::string());
	cmd << "ReadConfigG " << register_name;
	m_xpad->sendWait(cmd.str(), message);

	if (message.length() > 1)
	{
		char *value = new char[message.length() + 1];
		strcpy(value, message.c_str());

		char *p = strtok(value, " ,.");
		ret[0] = reg;
		for (int i = 1; i < 8 ; i++)
		{
			p = strtok(NULL, " ,.");
			ret[i] = (unsigned short) atoi(p);
		}
	}
	else
	{
		ret = NULL;
	}

	delete[] ret;

	DEB_TRACE() << "********** Outside of Camera::readConfigG ***********";

	if (ret != NULL)
	{
		DEB_TRACE() << "Reading global configuration performed SUCCESFULLY";
		return 0;
	}
	else
	{
		throw LIMA_HW_EXC(Error, "Reading global configuration FAILED!");
		return -1;
	}


}

int Camera::loadDefaultConfigGValues()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::LoadDefaultConfigGValues ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_process_id = 6;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	return 0;
}

int Camera::ITHLIncrease()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::ITHLIncrease ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_process_id = 7;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::ITHLIncrease ***********";

	return 0; //ret;
}

int Camera::ITHLDecrease()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::ITHLDecrease ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_process_id = 8;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::ITHLDecrease ***********";

	return 0; //ret;
}

int Camera::loadFlatConfigL(unsigned short flat_value)
{
	DEB_MEMBER_FUNCT();

	DEB_TRACE() << "********** Inside of Camera::loadFlatConfig ***********";

	waitAcqEnd();

	m_flat_value = flat_value;
	m_wait_flag = false;
	m_quit = false;
	m_process_id = 9;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::loadFlatConfig ***********";

	return 0; //ret;
}

int Camera::loadConfigLFromFile(char *fpath)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::loadConfigLFromFile ***********";
	DEB_TRACE() << "LoadConfigLFromFile : " << fpath;
	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "LoadConfigLFromFile ";

	m_xpad->sendNoWait(cmd.str());

	ret = m_xpad->sendParametersFile(fpath);

	if (ret == 0)
		DEB_TRACE() << "Local configuration loaded from file SUCCESFULLY";
	else if (ret == 1)
		DEB_TRACE() << "Local configuration loaded from file was ABORTED";
	else
		throw LIMA_HW_EXC(Error, "Loading local configuration from file FAILED!");

	DEB_TRACE() << "********** Outside of Camera::loadConfigLFromFile ***********";

	return ret;


}

int Camera::saveConfigLToFile(char *fpath)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::saveConfigLToFile ***********";

	std::stringstream cmd;
	//std::string str;
	int ret;

	cmd << "ReadConfigL";
	m_xpad->sendNoWait(cmd.str());

	if (!m_xpad->receiveParametersFile(fpath))
		ret = 0;
	else
		ret = -1;

	if (ret == 0)
		DEB_TRACE() << "Local configuration was saved to file SUCCESFULLY";
	else
		throw LIMA_HW_EXC(Error, "Saving local configuration to file FAILED!");

	DEB_TRACE() << "********** Outside of Camera::saveConfigLToFile ***********";

	return ret;
}

void Camera::setOutputSignalMode(unsigned short mode)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setTrigMode - " << DEB_VAR1(mode);
	DEB_PARAM() << DEB_VAR1(mode);

	if (mode >= 0 && mode < 9 )
	{
		m_xpad_output_signal_mode = mode;
	}
	else
	{
		DEB_ERROR() << "Error: Output Signal mode unsupported" ;
		throw LIMA_HW_EXC(Error, "Output Signal mode unsupported");
	}
}

void Camera::getOutputSignalMode(unsigned short& mode)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::getOutputSignalMode - " << DEB_VAR1(mode);
	DEB_PARAM() << DEB_VAR1(mode);

	mode = m_xpad_output_signal_mode;
}

void Camera::setGeometricalCorrectionFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setGeometricalCorrectionFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	m_geometrical_correction_flag = flag;

	int ret;
	std::string message, flag_state;
	std::stringstream cmd;
	Size size;

	switch (flag)
	{
		case 0: flag_state = "false";
			break;
		default: flag_state = "true";
	}

	cmd.str(std::string());
	cmd << "SetGeometricalCorrectionFlag " << flag_state.c_str();
	m_xpad->sendWait(cmd.str(), ret);

	if ( ret == 0)
		getImageSize(size);

	m_image_size = size;
	ImageType pixel_depth;
	getImageType(pixel_depth);
	maxImageSizeChanged(m_image_size, pixel_depth);
}

unsigned short Camera::getGeometricalCorrectionFlag()
{
	DEB_MEMBER_FUNCT();

	return m_geometrical_correction_flag;
}

void Camera::setFlatFieldCorrectionFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setFlatFieldCorrectionFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	m_flat_field_correction_flag = flag;
}

unsigned short Camera::getFlatFieldCorrectionFlag()
{
	DEB_MEMBER_FUNCT();

	return m_flat_field_correction_flag;
}

void Camera::setDeadNoisyPixelCorrectionFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setDeadNoisyPixelCorretctionFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	setNoisyPixelCorrectionFlag(flag);
	setDeadPixelCorrectionFlag(flag);
}

unsigned short Camera::getDeadNoisyPixelCorrectionFlag()
{
	DEB_MEMBER_FUNCT();

	unsigned short ret1, ret2;

	ret1 = getNoisyPixelCorrectionFlag();
	ret2 = getDeadPixelCorrectionFlag();

	return ret1 & ret2;
}

void Camera::setNoisyPixelCorrectionFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setNoisyPixelCorretctionFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	m_noisy_pixel_correction_flag = flag;

	int ret;
	std::string message, flag_state;
	std::stringstream cmd;
	Size size;

	switch (flag)
	{
		case 0: flag_state = "false";
			break;
		default: flag_state = "true";
			break;
	}

	cmd.str(std::string());
	cmd << "SetNoisyPixelCorrectionFlag " << flag_state.c_str();
	m_xpad->sendWait(cmd.str(), ret);
}

unsigned short Camera::getNoisyPixelCorrectionFlag()
{
	DEB_MEMBER_FUNCT();

	std::string ret;
	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetNoisyPixelCorrectionFlag ";
	m_xpad->sendWait(cmd.str(), ret);

	if (ret.compare("false"))
		m_noisy_pixel_correction_flag = 1;
	else
		m_noisy_pixel_correction_flag = 0;

	return m_noisy_pixel_correction_flag;
}

void Camera::setDeadPixelCorrectionFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setDeadPixelCorrectionFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	m_flat_field_correction_flag = flag;

	int ret;
	std::string message, flag_state;
	std::stringstream cmd;

	switch (flag)
	{
		case 0: flag_state = "false";
			break;
		default: flag_state = "true";
			break;
	}

	cmd.str(std::string());
	cmd << "SetDeadPixelCorrectionFlag " << flag_state.c_str();
	m_xpad->sendWait(cmd.str(), ret);
}

unsigned short Camera::getDeadPixelCorrectionFlag()
{
	DEB_MEMBER_FUNCT();

	std::string ret;
	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetDeadPixelCorrectionFlag ";
	m_xpad->sendWait(cmd.str(), ret);

	if (ret.compare("false"))
		m_dead_pixel_correction_flag = 1;
	else
		m_dead_pixel_correction_flag = 0;

	return m_dead_pixel_correction_flag;
}

void Camera::setAcquisitionMode(unsigned int mode)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setAcquisitionMode - " << DEB_VAR1(mode);
	DEB_PARAM() << DEB_VAR1(mode);

	m_acquisition_mode = mode;
}

unsigned int Camera::getAcquisitionMode()
{
	DEB_MEMBER_FUNCT();

	return m_acquisition_mode;
}

void Camera::setImageTransferFlag(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setTransferFlag - " << DEB_VAR1(flag);
	DEB_PARAM() << DEB_VAR1(flag);

	m_image_transfer_flag = flag;
}

unsigned short Camera::getImageTransferFlag()
{
	DEB_MEMBER_FUNCT();

	return m_image_transfer_flag;
}

void Camera::setImageFileFormat(unsigned short format)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setImageFileFormat - " << DEB_VAR1(format);
	DEB_PARAM() << DEB_VAR1(format);

	m_image_file_format = format;
}

unsigned short Camera::getImageFileFormat()
{
	DEB_MEMBER_FUNCT();

	return m_image_file_format;
}

void Camera::setOverflowTime(unsigned int value)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setOverflowTime - " << DEB_VAR1(value);
	DEB_PARAM() << DEB_VAR1(value);

	m_overflow_time = value;
}

unsigned int Camera::getOverflowTime()
{
	DEB_MEMBER_FUNCT();

	return m_overflow_time;
}

void Camera::setStackImages(unsigned int value)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "Camera::setStackImages - " << DEB_VAR1(value);
	DEB_PARAM() << DEB_VAR1(value);

	m_stack_images = value;
}

unsigned int Camera::getStackImages()
{
	DEB_MEMBER_FUNCT();

	return m_stack_images;
}

int Camera::calibrationOTN(unsigned short calibrationConfiguration)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::calibrationOTN ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_calibration_configuration = calibrationConfiguration;
	m_process_id = 1;
	//m_process_param1 = calibrationConfiguration;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::calibrationOTN ***********";

	return 0;
}

int Camera::calibrationOTNPulse(unsigned short calibrationConfiguration)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::calibrationOTNPulse ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_calibration_configuration = calibrationConfiguration;
	m_process_id = 2;
	//m_process_param1 = calibrationConfiguration;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::calibrationOTNPulse ***********";

	return 0;

}

int Camera::calibrationBEAM(unsigned int time, unsigned int ITHLmax, unsigned short calibrationConfiguration)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::calibrationBEAM ***********";

	waitAcqEnd();

	m_wait_flag = false;
	m_quit = false;
	m_time = time;
	m_ITHL_max = ITHLmax;
	m_calibration_configuration = calibrationConfiguration;
	m_process_id = 3;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::calibrationBEAM ***********";

	return 0;

}

int Camera::loadCalibrationFromFile(char *fpath)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::loadCalibrationFromFile ***********";

	waitAcqEnd();

	m_file_path.clear();
	m_file_path.append(fpath);
	m_wait_flag = false;
	m_quit = false;
	m_process_id = 4;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::loadCalibrationFromFile ***********";

	return 0; //ret1 & ret2;
}

int Camera::saveCalibrationToFile(char *fpath)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::saveCalibrationToFile ***********";

	waitAcqEnd();

	m_file_path.clear();
	m_file_path.append(fpath);
	m_wait_flag = false;
	m_quit = false;
	m_process_id = 5;
	m_cond.broadcast();

	while (!m_thread_running)
		m_cond.wait();

	DEB_TRACE() << "********** Outside of Camera::saveCalibrationToFile ***********";

	return 0; //ret1 & ret2;
}

void Camera::abortCurrentProcess()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::abortCurrentProcess ***********";

	std::stringstream cmd;

	m_quit = true;
	m_cond.broadcast();

	cmd <<  "AbortCurrentProcess";
	m_xpad_alt->sendNoWait(cmd.str());

	DEB_TRACE() << "********** Outside of Camera::abortCurrentProcess ***********";
}

int Camera::increaseBurstNumber()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::decreaseBurstNumber ***********";
	std::stringstream cmd;
	int ret;

	cmd << "IncreaseBurstNumber";
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::decreaseBurstNumber ***********";

	return ret;
}

int Camera::decreaseBurstNumber()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::decreaseBurstNumber ***********";
	std::stringstream cmd;
	int ret;

	cmd << "DecreaseBurstNumber";
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::decreaseBurstNumber ***********";

	return ret;
}

int Camera::getBurstNumber()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getBurstNumber ***********";
	std::stringstream cmd;
	int ret;

	cmd << "GetBurstNumber";
	m_xpad->sendWait(cmd.str(), ret);

	m_burst_number = ret;

	DEB_TRACE() << "********** Outside of Camera::getBurstNumber ***********";

	return ret;
}

int Camera::resetBurstNumber()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::resetBurstNumber ***********";
	std::stringstream cmd;
	int ret;

	cmd << "ResetBurstNumber";
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::resetBurstNumber ***********";

	if (!ret)
	{
		m_burst_number = -1;
		return 0;
	}
	else
		return -1;
}

void Camera::exit()
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::exit ***********";
	std::stringstream cmd;

	cmd << "Exit";
	m_xpad->sendNoWait(cmd.str());

	DEB_TRACE() << "********** Outside of Camera::exit ***********";
}

int Camera::getConnectionID()
{
	return getBurstNumber();
}

int Camera::createWhiteImage(char* fileName)
{

	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::createWhiteImage ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "CreateWhiteImage " << fileName;
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::createWhiteImage ***********";

	return ret;
}

int Camera::deleteWhiteImage(char* fileName)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::deleteWhiteImage ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "DeleteWhiteImage " << fileName;
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::deleteWhiteImage ***********";

	return ret;
}

int Camera::setWhiteImage(char* fileName)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::setWhiteImage ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "SetWhiteImage " << fileName;
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::setWhiteImage ***********";

	return ret;
}

std::string Camera::getWhiteImagesInDir()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::getWhiteImagesInDir ***********";

	std::string message;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "GetWhiteImagesInDir";
	m_xpad->sendWait(cmd.str(), message);

	DEB_TRACE() << "List of White Images In DIR: " << message;

	DEB_TRACE() << "********** Outside of Camera::getWhiteImagesInDir ***********";
	return message;
}

void Camera::readDetectorTemperature()
{
}

int Camera::setDebugMode(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::setDebugMode ***********";

	int ret;
	std::string flag_state;
	std::stringstream cmd;

	switch (flag)
	{
		case 0: flag_state = "false";
			break;
		default: flag_state = "true";
			break;
	}

	cmd.str(std::string());
	cmd << "SetDebugMode " << flag_state.c_str();
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::setDebugMode ***********";

	return ret;
}

int Camera::showTimers(unsigned short flag)
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::showTimers ***********";

	int ret;
	std::string flag_state;
	std::stringstream cmd;

	switch (flag)
	{
		case 0: flag_state = "false";
			break;
		default: flag_state = "true";
			break;
	}

	cmd.str(std::string());
	cmd << "ShowTimers " << flag_state.c_str();
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::showTimers ***********";

	return ret;
}

int Camera::createDeadNoisyMask()
{
	DEB_MEMBER_FUNCT();
	DEB_TRACE() << "********** Inside of Camera::showTimers ***********";

	int ret;
	std::stringstream cmd;

	cmd.str(std::string());
	cmd << "CreateDeadNoisyMask ";
	m_xpad->sendWait(cmd.str(), ret);

	DEB_TRACE() << "********** Outside of Camera::showTimers ***********";

	return ret;
}
