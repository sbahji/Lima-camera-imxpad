//- C++
#include <iostream>
#include <sstream>
#include <fstream>
#include <istream>
#include <string>
#include <vector>
#include <exception>

#include <time.h> // clock, usleep
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <cstring>
#include <errno.h>



const int RD_BUFF = 1000;
bool g_valid;						// true if connected
struct sockaddr_in g_remote_addr;	// address of remote server */
int g_data_port;					// our data port
int g_data_listen_skt;				// data socket we listen on
int g_prompts;						// counts # of prompts received
int g_nug_read, g_cur_pos;
char g_rd_buff[RD_BUFF];
int g_just_read;
std::string g_errorMessage;
std::vector<std::string> g_debugMessages;
int g_skt;

void send_cmd(const std::string cmd);
int connect_to_server(const std::string hostname, int port);
void disconnect_from_server();
int receive_from_server();

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
                std::cerr << "Error passing parameters: default values used" << std::endl;
			}
			break;
		}

		//        
		std::cout << "============================================" << std::endl;
		std::cout << "hostname      = " << hostname << std::endl;
		std::cout << "port          = " << port << std::endl;
		std::cout << "module_mask   = " << module_mask << std::endl;
		std::cout << "============================================" << std::endl;

		if( 0 > connect_to_server(hostname, port) )
		{
			std::cerr << g_errorMessage << std::endl;
		}


        struct timeval _start_time;
        struct timeval now;

        struct timeval interval_begin, interval_end;

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
            receive_from_server();
            gettimeofday(&now, NULL);
	        std::cout << "Acquisition : Elapsed time  = "
	        		<< 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec)
					<< " (ms)" << std::endl;

	        gettimeofday(&interval_begin, NULL);

	        gettimeofday(&interval_end, NULL);
	        std::cout << "Interval : Elapsed time  = "
	        		<< 1e3 * (interval_end.tv_sec - interval_begin.tv_sec) + 1e-3 * (interval_end.tv_usec - interval_begin.tv_usec)
	        		<< " (ms)" << std::endl;
	        ++current_frame;
        }
        disconnect_from_server();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
    catch (...)
	{
		std::cerr << "Unknown Error" << std::endl;
	}

	return 0;
}
//--------------------------------------------------------------------------------------

int connect_to_server(const std::string hostName, int port)
{
    struct hostent *host;
    struct protoent *protocol;
    int opt;
    int rc = 0;

    if (g_valid)
    {
        g_errorMessage = "Already connected to server";
        return -1;
    }
    if ((host = gethostbyname(hostName.c_str())) == 0)
    {
        g_errorMessage = "can't get gethostbyname";
        endhostent();
        return -1;
    }
    if ((g_skt = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        g_errorMessage = "can't create socket";
        endhostent();
        return -1;
    }
    g_remote_addr.sin_family = host->h_addrtype;
    g_remote_addr.sin_port = htons (port);
    size_t len = host->h_length;
    memcpy(&g_remote_addr.sin_addr.s_addr, host->h_addr, len);
    endhostent();
    if (connect(g_skt, (struct sockaddr *) &g_remote_addr, sizeof(struct sockaddr_in)) == -1)
    {
        close(g_skt);
        g_errorMessage = "Connection to server refused. Is the server running?";
        return -1;
    }
    protocol = getprotobyname("tcp");
    if (protocol == 0)
    {
        g_errorMessage = "Cannot get protocol TCP\n";
        rc = -1;
    }
    else
    {
        opt = 1;
        if (setsockopt(g_skt, protocol->p_proto, TCP_NODELAY, (char *) &opt, 4) < 0)
        {
            g_errorMessage = "Cannot Set socket options";
            rc = -1;
        }
    }
    endprotoent();
    g_valid = 1;
    g_data_port = -1;
    g_data_listen_skt = -1;
    g_prompts = 0;
    g_nug_read = 0;
    g_cur_pos = 0;
    return rc;
}

void disconnect_from_server()
{
    if (g_valid)
    {
        shutdown(g_skt, 2);
        close(g_skt);
        g_valid = 0;
    }
}



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
		std::cerr << "Sending command " << cmd << " to server failed" << std::endl;
	}
}

int receive_from_server()
{
	int r;
	char tmp;
	r = recv(g_skt, &tmp, 1, MSG_PEEK | MSG_DONTWAIT);
	if (r == 0 || (r < 0 && errno != EWOULDBLOCK))
	{
		std::cerr << "Connection broken, r = " << r << " errno = " << errno << std::endl;
		return -1;
	}
	return 0;
}
