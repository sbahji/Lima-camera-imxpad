//- C++
#include <iostream>
#include <sstream>
#include <fstream>
#include <istream>
#include <string>
#include <time.h> // clock, usleep

#include <sys/socket.h>

//- LIMA
#include <lima/HwInterface.h>
#include <lima/CtControl.h>
#include <lima/CtAcquisition.h>
#include <lima/CtVideo.h>
#include <lima/CtImage.h>
#include <imXpadInterface.h>
#include <imXpadCamera.h>


void send_cmd(const std::string cmd);

//--------------------------------------------------------------------------------------
// test main:

//- 1st argument is the hostname 
//- 2nd argument is the port 
//- 3rd argument is the moduleMask
//--------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	std::cout << "============================================" << std::endl;
	std::cout << "Usage :./ds_TestLimaImXpad hostname port [moduleMask] nbFrames " << std::endl << std::endl;

	try
	{
        std::string hostname    = "cegitek";
        int port                = 3456;
        unsigned int module_mask = 0;
        unsigned int nb_frames(2);

		//read args of main 
		switch (argc)
		{   
            case 2:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> hostname;
			}
			break;
            
			case 3:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> port;
			}
			break;

			case 4:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> port;

				std::istringstream arg_module_mask(argv[3]);
				arg_module_mask >> module_mask;
			}
			break;

			case 5:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> port;

				std::istringstream arg_module_mask(argv[3]);
				arg_module_mask >> module_mask;

				std::istringstream arg_nb_frames(argv[4]);
				arg_nb_frames >> nb_frames;
			}
			break;

			default:
			{
                std::cout << "Error passing parameters: default values used" << std::endl;
			}
			break;
		}

		//        
		std::cout << "============================================" << std::endl;
		std::cout << "hostname      = " << hostname << std::endl;
		std::cout << "port          = " << port << std::endl;
		std::cout << "module_mask   = " << module_mask << std::endl;
		std::cout << "============================================" << std::endl;

		//initialize imXpad::Camera objects & Lima Objects
		std::cout << "Creating Camera Object..." << std::endl;
		lima::imXpad::Camera my_camera(hostname, port, module_mask);

		std::cout << "Creating Interface Object..." << std::endl;
		lima::imXpad::Interface my_interface(my_camera);

		std::cout << "Creating CtControl Object..." << std::endl;
		lima::CtControl my_control(&my_interface);

		std::cout << "============================================" << std::endl;

        lima::CtControl::Status ct_status;
        lima::imXpad::Camera::XpadStatus xpad_status;
        struct timeval _start_time;
        struct timeval now;

        struct timeval interval_begin, interval_end;
        std::string dummy;

        //SetExposureParameters
        unsigned int exposure_time(10000000); //µs
        unsigned int latency_time(0);//µs
        unsigned int overflow_time(4000); //µs
        unsigned short trigger_mode(0);
        unsigned short output_mode(0);
        bool  geometrical_correction_flag(false);
        bool flat_field_correction_flag(false);
        bool image_transfert_flag(true);
        unsigned short output_format_file(0);
        unsigned short acquisition_mode(0);
        unsigned short image_stack(1);
        const std::string& output_path("./files");

        std::stringstream prepare_cmd;

        prepare_cmd << "SetExposureParameters "
        		<< nb_frames << " "
				<< exposure_time << " "
				<< latency_time << " "
				<< overflow_time << " "
				<< trigger_mode << " "
				<< output_mode << " "
				<< geometrical_correction_flag << " "
				<< flat_field_correction_flag << " "
				<< image_transfert_flag << " "
				<< output_format_file << " "
				<< acquisition_mode << " "
				<< image_stack << " "
				<< output_path;

        send_cmd(prepare_cmd.str());

        unsigned int current_frame(0);

        std::stringstream start_cmd;
        start_cmd << "StartExposure";

        while ( nb_frames != current_frame)
        {
        	//StartExposure
            std::cout << "########################################################################################################" << std::endl;
            usleep(10000); //- sleep 10 ms
            std::cout << "Wait 10 ms" << std::endl;

            std::cout << "--------------------------------------------" << std::endl;
            gettimeofday(&_start_time, NULL); 
            send_cmd(start_cmd.str());
            gettimeofday(&now, NULL);
	        std::cout << "Ctcontrol.getStatus : Elapsed time  = " << 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec) << " (ms)" << std::endl;

	        my_camera.getStatus(xpad_status);

	        gettimeofday(&interval_begin, NULL);

	        while(lima::imXpad::Camera::XpadStatus::XpadState::Idle != xpad_status.state)
	        {
	        	std::cout << "Wait end acquisition !!!" << std::endl;
	        	my_camera.getStatus(xpad_status);
	        }
	        gettimeofday(&interval_end, NULL);
	        ++current_frame;
        }
	}
	catch (lima::Exception e)
	{
		std::cerr << "LIMA Error : " << e << std::endl;
	}
    catch (...)
	{
		std::cerr << "Unknown Error" << std::endl;
	}

	return 0;
}
//--------------------------------------------------------------------------------------





void send_cmd(const std::string cmd)
{
	int r, len;
	char *p;
	int skt;
	std::string command = cmd + "\n";

	len = command.length();
	p = (char*) command.c_str();
	while (len > 0 && (r = send(skt, p, len, 0)) > 0)
		p += r, len -= r;

	if (r <= 0)
	{
		std::cerr << "Sending command " << cmd << " to server failed";
	}
}
