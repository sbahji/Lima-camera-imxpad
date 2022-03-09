//- C++
#include <iostream>
#include <sstream>
#include <fstream>
#include <istream>
#include <string>
#include <vector>
#include <exception>
#include <cmath>

//- System headers
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

//- LIMA
#include <lima/HwInterface.h>
#include <lima/CtControl.h>
#include <lima/CtAcquisition.h>
#include <lima/CtVideo.h>
#include <lima/CtImage.h>
#include <imXpadInterface.h>
#include <imXpadCamera.h>

enum ServerResponse
{
	CLN_NEXT_PROMPT,		// '> ': at prompt
	CLN_NEXT_ERRMSG, 		// '! ': read error message
	CLN_NEXT_DEBUGMSG,		// '# ': read debugging message
	CLN_NEXT_TIMEBAR,		// '@ ': time-bar message
	CLN_NEXT_UNKNOWN,		// unknown line
	CLN_NEXT_INTRET,		// '* ': read integer ret value
	CLN_NEXT_DBLRET,		// '* ': read double ret value
	CLN_NEXT_STRRET			// '* ': read string ret value
};



const int RD_BUFF = 1000;
bool g_valid;						// true if connected
struct sockaddr_in g_remote_addr;	// address of remote server */
int g_data_port;					// our data port
int g_data_listen_skt;				// data socket we listen on
int g_prompts;						// counts # of prompts received
int g_num_read, g_cur_pos;
char g_rd_buff[RD_BUFF];
int g_just_read;
std::string g_errorMessage;
std::vector<std::string> g_debugMessages;
int g_skt;

const int MAX_ERRMSG = 1024;
const int CR = '\15';				// carriage return
const int LF = '\12';				// line feed
const char QUIT[] = "Exit\n";

std::string g_hostname    = "cegitek";
int g_port                = 3456;
unsigned int g_module_mask = 0;
unsigned int g_nb_frames(20);
float g_exposure_time(10);

void send_cmd(const std::string cmd);
int connect_to_server(const std::string hostname, int port);
void disconnect_from_server();
int receive_from_server();
void hardware_without_lima();
void hardware_with_lima();
int next_line(std::string *errmsg, int *ivalue, double *dvalue, std::string *svalue, int *done, int *outoff);

int get_char();
int wait_for_prompt();
int wait_for_response();

//--------------------------------------------------------------------------------------
// test main:

//- 1st argument is the hostname 
//- 2nd argument is the port 
//- 3rd argument is the moduleMask
//--------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	std::cout << "============================================" << std::endl;
	std::cout << "Usage :./ds_TestLimaImXpad hostname port [moduleMask]" << std::endl << std::endl;

	try
	{

		//read args of main 
		switch (argc)
		{   
            case 2:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> g_hostname;
			}
			break;
            
			case 3:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> g_hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> g_port;
			}
			break;

			case 4:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> g_hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> g_port;

				std::istringstream arg_module_mask(argv[3]);
				arg_module_mask >> g_module_mask;
			}
			break;

			case 6:
			{
				std::istringstream arg_hostname(argv[1]);
				arg_hostname >> g_hostname;

				std::istringstream arg_port(argv[2]);
				arg_port >> g_port;

				std::istringstream arg_module_mask(argv[3]);
				arg_module_mask >> g_module_mask;

				std::istringstream arg_nb_frames(argv[4]);
				arg_nb_frames >> g_nb_frames;

				std::istringstream arg_exposure_time(argv[5]);
				arg_exposure_time >> g_exposure_time;
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
		std::cout << "hostname      = " << g_hostname << std::endl;
		std::cout << "port          = " << g_port << std::endl;
		std::cout << "module_mask   = " << g_module_mask << std::endl;
		std::cout << "============================================" << std::endl;


		if( 0 > connect_to_server(g_hostname, g_port) )
		{
			std::cerr << g_errorMessage << std::endl;
		}
		else
		{
			std::cout << "Connected to ImXpad server" << std::endl;
			wait_for_prompt();
			std::cout << "Ready for acqusition" << std::endl;
		}


		std::cout << "Start exposure, send cmd to server" << std::endl;

		hardware_without_lima();

		disconnect_from_server();

//		sleep(10); //- sleep 10s
//		std::cout << "Wait 80 ms" << std::endl;

//		std::cout << "Snap from Lima to server" << std::endl;
//		hardware_with_lima();


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

void hardware_with_lima()
{
	//initialize imXpad::Camera objects & Lima Objects
	std::cout << "============================================" << std::endl;
	std::cout << "Creating Camera Object..." << std::endl;
	lima::imXpad::Camera my_camera(g_hostname, g_port, g_module_mask);

	std::cout << "Creating Interface Object..." << std::endl;
	lima::imXpad::Interface my_interface(my_camera);
	my_camera.setImageType(lima::Bpp12);

	std::cout << "Creating CtControl Object..." << std::endl;
	lima::CtControl my_control(&my_interface);

	struct timeval _start_time;
	struct timeval now;
	struct timeval interval_begin, interval_end;
	lima::CtControl::Status ct_status;
	lima::imXpad::Camera::XpadStatus xpad_status;

	my_control.getStatus(ct_status);

	std::cout << "Get device status : " << ct_status << std::endl;

	std::cout << "Wait 5s" << std::endl;
	sleep(5); //- sleep 5s

	std::cout << "--------------------------------------------" << std::endl;
	std::cout << "Start snap 1 !!!" << std::endl;

	std::cout << "Set exposure time" << std::endl;

	my_control.acquisition()->setAcqExpoTime(g_exposure_time);

	my_control.acquisition()->setAcqNbFrames(g_nb_frames);

	//my_control.acquisition()->setLatencyTime(0);

	gettimeofday(&_start_time, NULL);

	std::cout << "Prepare acquisition 1 " << std::endl;

	my_control.prepareAcq();

	std::cout << "Start Lima acquisition 1 " << std::endl;

	my_control.startAcq();

	std::cout << "Wait acqusition's end ........." << std::endl;

	my_control.getStatus(ct_status);
	while( lima::AcqRunning == ct_status.AcquisitionStatus )
	{
		my_control.getStatus(ct_status);
	}

	gettimeofday(&now, NULL);

	std::cout << "Snap 1 ( " << g_nb_frames <<" frames," << g_exposure_time <<" seconde : Elapsed time  = "
			<< 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec)
			<< " (ms)" << std::endl;

	std::cout << "============================================" << std::endl;
}


void hardware_without_lima()
{
//	if( 0 > connect_to_server(g_hostname, g_port) )
//	{
//		std::cerr << g_errorMessage << std::endl;
//	}


	struct timeval _start_time;
	struct timeval now;

	struct timeval interval_begin, interval_end;

	//SetExposureParameters
	unsigned int latency_time(5000);//µs
	unsigned int overflow_time(4000); //µs
	unsigned short trigger_mode(0);
	unsigned short output_mode(0);
	bool  geometrical_correction_flag(false);
	bool flat_field_correction_flag(false);
	bool image_transfert_flag(true);
	unsigned short output_format_file(0);
	unsigned short acquisition_mode(0);
	unsigned short image_stack(1);
	const std::string& output_path("");

	std::stringstream prepare_cmd;

	prepare_cmd << "SetExposureParameters "
			<< g_nb_frames << " "
			<< g_exposure_time << " "
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

	std::cout << "--------------------------------------------" << std::endl;
	gettimeofday(&_start_time, NULL);
	send_cmd(start_cmd.str());

	do
	{

	}while(0 != wait_for_response());
	gettimeofday(&now, NULL);

	std::cout << "Snap 1 ( " << g_nb_frames <<" frames," << g_exposure_time <<" seconde : Elapsed time  = "
			<< 1e3 * (now.tv_sec - _start_time.tv_sec) + 1e-3 * (now.tv_usec - _start_time.tv_usec)
			<< " (ms)" << std::endl;
}


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
    g_num_read = 0;
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

	std::string command = cmd + "\n";

	len = command.length();
	p = (char*) command.c_str();
	while (len > 0 && (r = send(g_skt, p, len, 0)) > 0)
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

int get_char()
{
    int r;

    if (!g_valid)
    {
        std::cerr << "Not connected to xpad server " << std::endl;
    }
    if (g_num_read == g_cur_pos)
    {
        while ((r = recv(g_skt, g_rd_buff, RD_BUFF, 0)) < 0 && errno == EINTR);
        if (r <= 0)
        {
            return -1;
        }
        g_cur_pos = 0;
        g_num_read = r;
        g_just_read = 1;
    }
    else
    {
        g_just_read = 0;
    }
    return g_rd_buff[g_cur_pos++];

}

int next_line(std::string *errmsg, int *ivalue, double *dvalue, std::string *svalue, int *done, int *outoff)
{
    int r;
    char timestr[80], *tp;
    char buff[MAX_ERRMSG + 2];
    char *bptr = buff;
    char *bptr2 = buff;
    int n, len;
    // Assume we are either at the beginning of a line now, or we are right
    // at the end of the previous line.
    do
    {
        r = get_char();
    } while (r == CR || r == LF);


    switch (r)
    {
    case -1:						// read error (disconnected?)
        std::cerr << "server read error (disconnected?)" << std::endl;
        break;

    case '>':						// at prompt
        get_char();					// discard ' '
        return CLN_NEXT_PROMPT;

    case '!':						// error message
        get_char();					// discard ' '
        while ((r = get_char()) != -1 && r != CR && r != LF)
        {
            *bptr++ = r;
        }
        *bptr = '\0';
        if(errmsg != 0)
            *errmsg = buff;
        return CLN_NEXT_ERRMSG;

    case '#':						// comment
        get_char();					// discard ' '
        while ((r = get_char()) != -1 && r != CR && r != LF)
        {
            *bptr++ = r;
        }
        *bptr = '\0';
        if(errmsg != 0)
            *errmsg = buff;
        return CLN_NEXT_DEBUGMSG;

    case '@':						// timebar message ("a/b")
        get_char();					// discard ' '
        tp = timestr;				// read time message
        for (;;)
        {
            r = get_char();
            if (r == '\'' || r == '"' || r == CR || r == LF || r == -1)
                break;
            *tp++ = r;
        }
        *tp = '\0';
        if (done != 0 && outoff != 0)
            sscanf(timestr, "%d %d", done, outoff);
        if (r == '\'' || r == '"')
        {
            while ((r = get_char()) != -1 && r != CR && r != LF)
            {
                *bptr++ = r;
            }
        }
        else
        {
            *bptr = '\0';
        }
        *bptr = '\0';
        len = strlen(buff);
        if (len > 0 && (bptr2[len - 1] == '\'' || bptr2[len - 1] == '"'))
        {
            bptr2[len - 1] = 0;
        }
        return CLN_NEXT_TIMEBAR;

    case '*': 						// return value
        get_char(); 					// discard ' '
        r = get_char();				// distinguish strings/nums
        if (r == '(')				// (null)
        {

            while ((r = get_char()) != -1 && r != ')' && r != CR && r != LF)
                ;
            if (svalue != 0)
                *svalue = "";
            return CLN_NEXT_STRRET;
        }

        else if (r == '"') /*		string */
        {
            while ((r = get_char()) != -1 && r != '"')
            {
                *bptr++ = r;
            }
            *bptr = '\0';
            if (svalue != 0)
                *svalue = buff;
            return CLN_NEXT_STRRET;
        }
        else
        {  // integer or double
            char buffer[80], *p;

            buffer[0] = r;
            p = &buffer[1];

            while ((r = get_char()) != -1 && r != CR && r != LF)
                *p++ = r;
            *p = '\0';

            if (dvalue)
            {
                if (strcmp(buffer, "nan") == 0)
                {
                    *dvalue = NAN;
                }
                else
                {
                    *dvalue = atof(buffer);
                }
                return CLN_NEXT_DBLRET;
            }
            if (ivalue)
            {
                *ivalue = atoi(buffer);
            }
            return CLN_NEXT_INTRET;
        }

    default: // don't know: abort to end of line
        n = 0;
        while ((r = get_char()) != -1 && r != CR && r != LF)
        {
            if (++n == MAX_ERRMSG)
                break;
            *bptr++ = r;
        }
        bptr += '\0';
        if (errmsg != 0)
            *errmsg = buff;
        return CLN_NEXT_UNKNOWN;
    }
    return 0;
}

int wait_for_prompt()
{
    int r;
    char tmp;

    if (!g_valid)
    {
        std::cerr << "Not connected to server " << std::endl;
    }
    std::cout << "checking connection with recv" << std::endl;
    r = recv(g_skt, &tmp, 1, MSG_PEEK | MSG_DONTWAIT);
    if (r == 0 || (r < 0 && errno != EWOULDBLOCK))
    {
        std::cerr << "waitForPrompt: Connection broken, r= " << r << " errno=" << errno << std::endl;
        return -1;
    }
    if (g_prompts == 0)
    {
        do
        {
            r = next_line(0, 0, 0, 0, 0, 0);
        } while (r != CLN_NEXT_PROMPT);
    }
    else
    {
        g_prompts--;
    }
    return r;
}

int wait_for_response()
{
    int r, code, done, outoff;
    std::string errmsg;
    g_debugMessages.clear();

    if (!g_valid)
    {
        std::cerr << "Not connected to server " << std::endl;;
    }
    for (;;)
    {
        r = next_line(&errmsg, &code, 0, 0, &done, &outoff);
        switch (r)
        {
        case CLN_NEXT_PROMPT:
        	g_errorMessage = errmsg;
            std::clog << ("(warning) No return code from the server.") << std::endl;;
            g_prompts++;
            return -1;

        case CLN_NEXT_ERRMSG:
        	g_errorMessage = errmsg;
            std::cout << "\t---> message: " << errmsg << std::endl;
            break;

        case CLN_NEXT_DEBUGMSG:
            std::cout << "\t---> message: " << errmsg << std::endl;
            break;

        case CLN_NEXT_UNKNOWN:
            std::cout << "Unknown string from server: "+ errmsg << std::endl;
            break;

        case CLN_NEXT_TIMEBAR:
            std::clog << "Do nothing !!!" << std::endl;
            break;
        case CLN_NEXT_INTRET:
            std::cout << "\t---> value: " << code << std::endl;
            if (code == -1)
            {
                wait_for_response();
            }
            return 0;

        case CLN_NEXT_DBLRET:
            std::clog << "Unexpected double return code." << std::endl;
            return -1;
        case CLN_NEXT_STRRET:
            std::clog << "Server responded with a string." << std::endl;
            return -1;
        default:
            std::clog << "Programming error. Please report" << std::endl;
        }
    }
    return r;
}
