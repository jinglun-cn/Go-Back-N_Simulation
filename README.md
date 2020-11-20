# CNT4504-5505-Project2

#### Developers: Jumana Bukhari, Jinglun Cai, Spencer Cain, Yixin Chen

#### Date Created: 11/01/2020

#### How to Compile:



#### How to Run:



#### How to Exit:



#### Files:
- include/server.h: header file of the server
- include/client.h: header file of the client
- include/mylog.h: header of logging macros
- include/utils.h: header of some util functions for both client and server using
- src/server.cpp: implementation of the server and the main function
- src/client.cpp: implementation of the client and the main function
- src/utils.cpp: implementation of util functions


#### Implementation
- util functions
    - `enum PACKET_TYPE` is a enumerate structure which defines the package type sending between the server and clients. Inside it, some types are only used for Client-to-Server packages, and some only used for Server-to-Client packages. See utils.h for detail.

    - `void InitNormalDistribution(int mean, int stddev)` function is used to set the mean and stddev of a normal distribution function. Other can calls `uint64_t SimulateDelay()` function to get a simulated delay which obey this normal distribution.

    - `class UDP_Package` is a class defined the package format for pack and unpack some message and sending between the server and clients.

- server-slide
    - The server has two thread. One thread `void ReceiverThread(int server_socket);` for receiving packages from clients and pass it to different client handler. The client handler will build the corresponding packages and insert to a global atomic queue `extern AtomicPKGQueue *global_wait_queue;`. Another for thread `void SenderThread(int server_socket);` will retrieve packages from a global submit queue `extern AtomicPKGQueue *global_submit_queue;` and send to corresponding client based on the id inside each package and a global client vector `extern std::map<uint32_t, ClientHandler*> *global_clients;`.

    - `class ClientHandler` is a class to handle one client connection. It stores all packages received from the same client and process different command.

    - `class AtomicPKGQueue` is a simple class for atomic operations of a queue.

- client-slide
    - The client has no other thread for processing. It just accept command from the user and send packages to the server. Then it is waiting for the response from the server. All received packages will store in a vector `std::vector<UDP_Package*> recv_pkgs_;` for later processing and to be integrated to a whole file.

    - When the client receives a package is `PK_EOF` type, the client is going to check all received packages and check which package has not received yet based on the sequence number. If there are some packages are not received, the client will wait it. After all packages are received, the client will integrate all `PK_FILE` related packages and build a file and write to the local. The file will named with a timestamp suffix in case the duplicate of the name.

- LOST ERROR:
    - `#define LOST_ERROR_PERCENT 20` in `server.h`

    - `bool ShouldLost();` function in `server.h`

    - In `void SendPackageWrapper(int server_socket, UDP_Package *pkg, bool re_send)` function, before `sendto`, the server calls `ShouldLost()` function. If the returned value indicate this package should be lost, the server will not call `sendto` function indeed. Otherwise, the package will be sent to the client.

    - If the package is lost, the server cannot receive the ACK from the client. When the server check the timed out packages `void CheckProcessTimeOut(int server_socket);`, it will resend this package but don't care this package is lost or just delayed due to the network issue.

- OUT OF SEQUENCE (OOS):
    - `#define OOS_ERROR_PERCENT 20` in `server.h`

    - `bool ShouldOOS();` function in `server.h`

    - In `UDP_Package *AtomicPKGQueue::PopPKG()` function, when the send thread retrieves a package from the global wait queue, the atomic queue will check whether it need to return an OOS package. If so, a random package within current send window is returned to the send thread and sent to the client.

    - Considering the OOS issue, the client has to build the file based on a real and correct sequence. In `void Client::CheckSeqNumber(uint32_t eof_seq)` function, the client will check whether all file data has been received based on the sequence number. It then  builds the file in `void Client::ProcessGotFile(std::string fname)`. Packages will be sorted in the function based on the sequence number.

- TIMED OUT:
    - We simulate timed out issue in the client-slide.

    - `#define DELAY_MEAN_SECOND` the mean which is the normal distribution's mean which is used to determine the delay time.

    - `#define DELAY_DEV_SECOND` the stddev of the normal distribution which is used to determine the delay time.

    - `#define DELAY_MAX_TIME` the max delay time in case a large delay time generated from the normal distribution.

    - In `void Client::SendPackageWrapper(UDP_Package *pkg)` function, the client will sleep some time to simulate the delay before calling the real `sendto` function. This simulate the network issue and causes the server-slide receives the ACK package later than the threshold.

    - The function `void CheckProcessTimeOut(int server_socket);` in `server.h` will check the timed out package and resend it.

- MORE:
    - Please see the `.h` and `.cpp` files for the detail of the implementation.


    ### Test:
    #### Basics:

    ```
    ---------------- MENU ------------------------
    list               list files in the server
    get <file name>    get the file from the server
    exit               exit this application
    ----------------------------------------------
    ```
    ```
    list
    ...
    Server:
    .  ..  README.md  utils.o  .git  .gitignore  server.o  client.o  server  client  Makefile  testfile.txt  include  src  
    ```
    ```
    get testfile.txt
    ...
    File has retrived from the server, and written to the local, named: testfile.txt.xx.1605563552246042
    ```
    ```
    exit
    cai@linprog2.cs.fsu.edu:~/CNT4504-5505-Project2>ls
    client    README.md  testfile.txt
    client.o  server     testfile.txt.xx.1605563552246042
    include   server.o   utils.o
    Makefile  src
    ```

    #### Error Handling:
    - `LOST_ERROR_PERCENT = 20, OOS_ERROR_PERCENT = 20, PKG_TTL = 100000000 (100 s)`


#### Division of Labors:
- Jumana Bukhari:
- Jinglun Cai:
- Spencer Cain:
- Yixin Chen:
