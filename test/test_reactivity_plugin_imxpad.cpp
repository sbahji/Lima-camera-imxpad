//- C++
#include <iostream>
#include <string>
#include <time.h> // clock, usleep

//- LIMA
#include <lima/HwInterface.h>
#include <lima/CtControl.h>
#include <lima/CtAcquisition.h>
#include <lima/CtVideo.h>
#include <lima/CtImage.h>
#include <imXpadInterface.h>
#include <imXpadCamera.h>


//--------------------------------------------------------------------------------------
// test main:

//- 1st argument is the hostname 
//- 2nd argument is the port 
//- 3rd argument is the moduleMask
//--------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	std::cout << "============================================" << std::endl;
	std::cout << "Usage :./ds_TestLimaImXpad hostname port [moduleMask] " << std::endl << std::endl;

	try
	{
        std::string hostname    = "cegitek";
        int port                = 3456;
        unsigned int module_mask = 0;

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
        std::string dummy;

        while (1)
        {
            //GetDetectorStatus, GetDetectorType','GetDetectorModel', 'GetModuleMask', 'GetModuleNumber
            std::cout << "########################################################################################################" << std::endl;
            usleep(10000); //- sleep 10 ms

            std::cout << "--------------------------------------------" << std::endl;
            gettimeofday(&_start_time, NULL); 
            my_control.getStatus(ct_status);            
            gettimeofday(&now, NULL);
	        std::cout << "Ctcontrol.getStatus : Elapsed time  = " << 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec) << " (ms)" << std::endl;

            // std::cout << "--------------------------------------------" << std::endl;
            // gettimeofday(&_start_time, NULL);             
            // my_camera.getStatus(xpad_status);            
            // gettimeofday(&now, NULL);
	        // std::cout << "imXpad::Camera.getStatus : Elapsed time  = " << 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec) << " (ms)" << std::endl;

            // std::cout << "--------------------------------------------" << std::endl;
            // gettimeofday(&_start_time, NULL); 
            // my_camera.getDetectorType(dummy);
            // gettimeofday(&now, NULL);
	        // std::cout << "imXpad::Camera.getDetectorType : Elapsed time  = " << 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec) << " (ms)" << std::endl << std::endl;
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