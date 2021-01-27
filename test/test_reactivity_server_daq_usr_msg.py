#client example
import socket
import time
import datetime

def recv_timeout(the_socket, timeout = 0.01):
    #make socket non blocking
    the_socket.setblocking(0)
    
    #total data partwise in an array
    total_data=[];
    data='';
    
    #beginning time
    begin=time.time()
    while 1:
        #if you got some data, then break after timeout
        if total_data and time.time()-begin > timeout:
            break
        
        #if you got no data at all, wait a little longer, twice the timeout
        elif time.time()-begin > timeout*2:
            break
        
        #recv something
        try:
            data = the_socket.recv(8192)
            if data:
                total_data.append(data)
                #change the beginning time for measurement
                begin=time.time()
            else:
                #sleep for sometime to indicate a gap
                time.sleep(0.1)
        except:
            pass
    
    #join all parts to make final string
    return ''.join(total_data)
    
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect(('172.16.6.150', 3456))
client_socket.settimeout(1)

while 1:
    try :
        data_out = raw_input ( "ENTER msg to SEND to Server : " )
        print "---------------------"
        ###time.sleep(0.01)#wait 10 ms
        ###print "wait 10ms"
        client_socket.send((data_out).encode())
        horodatage = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]#microsec, [:-3] #millis
        print "["+horodatage+"]" + " >> : " + data_out                
        ####data_in = client_socket.recv(1024)
        data_in = recv_timeout(client_socket);
        horodatage = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]#microsec, [:-3] #millis
        print "["+horodatage+"]" + " << : " + data_in  
    except socket.timeout as e:
        data_in = ""
        print(e)    
    
client_socket.close()    
