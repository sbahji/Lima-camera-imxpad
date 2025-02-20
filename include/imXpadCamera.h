//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
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
/*
 * XpadCamera.h
 * Created on: August 01, 2013
 * Author: Hector PEREZ PONCE
 */
#ifndef IMXPADCAMERA_H_
#define IMXPADCAMERA_H_

#include <stdlib.h>
#include <limits>
#include <stdarg.h>
#include <strings.h>
#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"
#include "lima/HwInterface.h"
#include "imXpadInterface.h"
#include "lima/Debug.h"
#include "imXpadClient.h"
#include <unistd.h>
#include <sys/time.h>

namespace lima
{
namespace imXpad
{

class BufferCtrlObj;

/*******************************************************************
 * \class Camera
 * \brief object controlling the Xpad camera
 *******************************************************************/
class Camera : public  HwMaxImageSizeCallbackGen
{
    DEB_CLASS_NAMESPC(DebModCamera, "Camera", "ImXpad");

public:

    struct XpadStatus
    {
    public:

        enum XpadState
        {
            Idle, ///< The detector is not acquiringing data
            Acquiring, ///< The detector is acquiring data
            CalibrationManipulation, ///< The detector is loading or saving calibrations
            Calibrating,
            DigitalTest, ///< The detector is performing a digital test
            Resetting ///< The detector is resetting.
        } ;

        XpadState state;
        int frame_num; ///< The current frame number, within a group, being acquired, only valid when not {@link #Idle}
        int completed_frames; ///< The number of frames completed, only valid when not {@link #Idle}
    } ;

    struct XpadDigitalTest
    {

        enum DigitalTest
        {
            Flat, ///< Test using a flat value all over the detector
            Strips, ///< Test using a strips all over the detector
            Gradient ///< Test using a gradient values all over the detector
        } ;
    } ;

    struct Calibration
    {

        enum Configuration
        {
            Slow,
            Medium,
            Fast
        } ;
    } ;

    struct XpadAcquisitionMode
    {

        enum AcquisitionMode
        {
            Standard,
            DetectorBurst,
            ComputerBurst,
            Stacking16bits,
            Stacking32bits,
            SingleBunch16bits,
            SingleBunch32bits
        } ;
    } ;

    struct XpadOutputSignal
    {

        enum OutputSignal
        {
            ExposureBusy,
            ShutterBusy,
            BusyUpdateOverflow,
            PixelCounterEnabled,
            ExternalGate,
            ExposureReadDone,
            DataTransfer,
            RAMReadyImageBusy,
            XPADToLocalDDR,
            LocalDDRToPC

        } ;
    } ;

    struct XpadImageFileFormat
    {

        enum ImageFileFormat
        {
            Ascii,
            Binary
        } ;
    } ;

    Camera(std::string hostname, int port, unsigned int moduleMask = 0);
    ~Camera();

    int init();
    void quit();
    void reset();
    int prepareAcq();
    void startAcq();
    void stopAcq();
    void waitAcqEnd();
    void setWaitAcqEndTime(unsigned int time);

    // -- detector info object
    void getImageType(ImageType& type);
    void setImageType(ImageType type);
    void getDetectorType(std::string& type);
    void getDetectorTypeFromHardware(std::string& type);
    void getDetectorModel(std::string& model);
    void getDetectorModelFromHardware(std::string& model);
    void getImageSize(Size& size);
    void getPixelSize(double& size_x, double& size_y);
    int setModuleMask(unsigned int moduleMask);
    void getModuleMask();
    void getModuleNumber();
    void getChipMask();
    void getChipNumber();

    // -- Buffer control object
    HwBufferCtrlObj* getBufferCtrlObj();
    void setNbFrames(int nb_frames);
    void getNbFrames(int& nb_frames);
    void sendExposeCommand();
    int readFrameExpose(void *ptr, int frame_nb);
    int getDataExposeReturn();
    int getNbHwAcquiredFrames();

    //-- Synch control object
    void setTrigMode(TrigMode mode);
    void getTrigMode(TrigMode& mode);
    void setOutputSignalMode(unsigned short mode);
    void getOutputSignalMode(unsigned short& mode);
    void setExpTime(double exp_time);
    void getExpTime(double& exp_time);
    void setLatTime(double lat_time);
    void getLatTime(double& lat_time);

    //-- Status
    void getStatus(XpadStatus& status);
    bool isAcqRunning() const;

    //---------------------------------------------------------------
    //- XPAD Stuff
    //! Get list of USB connected devices
    std::string getUSBDeviceList(void);

    //! Connect to a selected QuickUSB device
    int setUSBDevice(unsigned short module);

    //! Define the Detector Model
    //int defineDetectorModel(unsigned short model);

    //! Check if detector is Ready to work
    int askReady();

    //! Perform a Digital Test
    int digitalTest(unsigned short mode);

    //! Read global configuration values from file and load them in the detector
    int loadConfigGFromFile(char *fpath);

    //! Save global configuration values from detector to file
    int saveConfigGToFile(char *fpath);

    //! Send value to register for all chips in global configuration
    int loadConfigG(char *regID, unsigned short value);

    //! Read values from register and from all chips in global configuration
    int readConfigG(char *regID);

    //! Load default values for global configuration
    int loadDefaultConfigGValues();

    //! Increment of one unit of the global ITHL register
    int ITHLIncrease();

    //! Decrement of one unit in the global ITHL register
    int ITHLDecrease();

    //!	Load of flat config of value: flat_value (on each pixel)
    int loadFlatConfigL(unsigned short flat_value);

    //! Read local configuration values from file and save it in detector
    int loadConfigLFromFile(char *fpath);

    //! Save local configuration values from detector to file
    int saveConfigLToFile(char *fpath);

    //! Set flag for geometrical corrections
    void setGeometricalCorrectionFlag(unsigned short flag);

    //! Get flag for geometrical corrections
    unsigned short getGeometricalCorrectionFlag();

    //! Set flag for flat field corrections
    void setFlatFieldCorrectionFlag(unsigned short flag);

    //! Set flag for dead & noisy pixel corrections
    void setDeadNoisyPixelCorrectionFlag(unsigned short flag);

    //! Get flag for dead & noisy pixel corrections
    unsigned short getDeadNoisyPixelCorrectionFlag();

    //! Set flag for noisy pixel corrections
    void setNoisyPixelCorrectionFlag(unsigned short flag);

    //! Get flag for noisy pixel corrections
    unsigned short getNoisyPixelCorrectionFlag();

    //! Set flag for dead pixel corrections
    void setDeadPixelCorrectionFlag(unsigned short flag);

    //! Get flag for dead pixel corrections
    unsigned short getDeadPixelCorrectionFlag();

    //! Get flag for flat field corrections
    unsigned short getFlatFieldCorrectionFlag();

    //! Set acquisition mode
    void setAcquisitionMode(unsigned int mode);

    //! Get acquisition mode
    unsigned int getAcquisitionMode();

    //! Set flag for geometrical corrections
    void setImageTransferFlag(unsigned short flag);

    //! Get flag for geometrical corrections
    unsigned short getImageTransferFlag();

    //! Set flag for geometrical corrections
    void setImageFileFormat(unsigned short format);

    //! Get flag for geometrical corrections
    unsigned short getImageFileFormat();

    //!< Set overflow time
    void setOverflowTime(unsigned int value);

    //!< Get overflow time
    unsigned int getOverflowTime();

    //!< Set the number of images per stack;
    void setStackImages(unsigned int value);

    //!< Get the number of images per stack;
    unsigned int getStackImages();

    //! Perform a Calibration over the noise
    int calibrationOTN(unsigned short calibrationConfiguration);

    //! Perform a Calibration over the noise with PULSE
    int calibrationOTNPulse(unsigned short calibrationConfiguration);

    //! Perform a Calibration over the noise
    int calibrationBEAM(unsigned int time, unsigned int ITHLmax, unsigned short calibrationConfiguration);

    //! Load a Calibration file from disk
    int loadCalibrationFromFile(char *fpath);

    //! Save calibration to a file
    int saveCalibrationToFile(char *fpath);

    //! Cancel current operation
    void abortCurrentProcess();

    //! Increas burst index
    int increaseBurstNumber();

    //! Decrease burst index
    int decreaseBurstNumber();

    //! Get the burst index
    int getBurstNumber();

    //! Reset the burst index
    int resetBurstNumber();

    //! Indicate to Server to liberate thread
    void exit();

    //! Return the burst number used as Server Connection ID
    int getConnectionID();

    //!< Creates a white image for flat field and dead pixel corrections
    int createWhiteImage(char* fileName);

    //!< Deletes an existing white image.
    int deleteWhiteImage(char* fileName);

    //!< Retreives a list of existing white images in server
    int setWhiteImage(char* fileName);

    //!< Get a list of White images stored in the server
    std::string getWhiteImagesInDir();

    //!< Read detector temperature
    void readDetectorTemperature();

    //!< Set debug mode in server
    int setDebugMode(unsigned short flag);

    //!< Show processing and transmission time in server
    int showTimers(unsigned short flag);

    //!< Create the dead noisy pixel mask
    int createDeadNoisyMask();

private:


/*     GLOBAL REGISTERS     */
#define AMPTP                       31
#define IMFP                        59
#define IOTA                        60
#define IPRE                        61
#define ITHL                        62
#define ITUNE                       63
#define IBUFF                       64

#define IMG_LINE                    120
#define IMG_COLUMN                  80

    enum IMG_TYPE
    {
        B2 = 0, 
        B4 = 1
    } ;

    XpadClient              *m_xpad;
    XpadClient              *m_xpad_alt;

    //---------------------------------
    //- LIMA stuff
    std::string             m_host_name;
    int                     m_port;
    int                     m_nb_frames;
    int                     m_acq_frame_nb;

    int					    m_process_id;
    unsigned short		    m_calibration_configuration;
    unsigned int			m_time;
    unsigned int			m_ITHL_max;
    std::string				m_file_path;
    unsigned short			m_flat_value;
    unsigned int            m_dead_time;

    mutable Cond            m_cond;
    bool                    m_quit;
    bool                    m_wait_flag;
    bool                    m_thread_running;

    class                   AcqThread;
    AcqThread               *m_acq_thread;

    //---------------------------------
    //- XPAD stuff
    unsigned int	    	m_module_mask;
    unsigned short          m_chip_mask;
    std::string             m_xpad_model;
    std::string             m_xpad_type;

    unsigned short          m_geometrical_correction_flag;
    unsigned short          m_flat_field_correction_flag;
    unsigned short          m_noisy_pixel_correction_flag;
    unsigned short          m_dead_pixel_correction_flag;
    unsigned short          m_acquisition_mode;
    unsigned short          m_image_transfer_flag;
    unsigned short          m_image_file_format;

    Size                    m_image_size;
    IMG_TYPE                m_pixel_depth;
    unsigned int            m_xpad_trigger_mode;
    unsigned int            m_xpad_output_signal_mode;
    unsigned int            m_exp_time_usec;
    unsigned int            m_lat_time_usec;
    unsigned int            m_overflow_time;
    int                     m_module_number;
    int                     m_chip_number;
    int                     m_burst_number;
    unsigned int            m_stack_images;

    // Buffer control object
    SoftBufferCtrlObj       m_buffer_ctrl_obj;
    XpadStatus              m_state;
} ;

} // namespace imXpad
} // namespace lima

#endif /* IMXPADCAMERA_H_ */
