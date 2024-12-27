# Computer Network Course Final Project PHASE 2 ---- Socket Programming
## Project Member
B11902037 劉丞浩
B11902073 蘇翊軒

## Project Description
This project develops a streamlined client-server system that enables user registration, login, and communication. After logging in, users can exchange messages and files with other clients. Additionally, the system supports basic audio and video streaming.

## Project Build
1. Compile the server and client using `make clean` and `make`
2. Run the server and client by typing `./server` and `./client`, respectively.

## Features
### User Register/Login

Upon running the client, a menu displaying all available commands will appear. To register a new user, enter REGISTER <username> <password>. After successful registration, log in by typing LOGIN <username> <password> <p2pPort>, where <p2pPort> specifies the port for peer-to-peer communication.

### Client-to-Client Messaging

This project offers two methods for sending messages: Relay mode and Peer-to-Peer (P2P) mode. In Relay mode, messages are routed through the server, while in P2P mode, messages are sent directly to the port specified during login. To send a message, use RELAY_MSG <username> <message> for Relay mode or DIRECT_MSG <username> <message> for P2P mode.

### File Transfer

This project supports file transfer. To send a file to another client, type SEND_FILE <username> <filepath>. Received files are automatically saved in the received_files folder.

### Audio/Video Streaming 

This project supports audio and video streaming. To start streaming, type `STREAM_AUDIO <filename>` for audio or `STREAM VIDEO <filename>` for video. The server begins streaming upon receiving the command. Audio streaming uses the PortAudio package, while video streaming relies on OpenCV. Currently, audio streaming supports WAV files only.

### OpenSSL Encryption

Every message sent by clients and servers will be encrypted by ssl. 

## Requirement
- C++ 17
- pkg-config (Used for compiling)
- Portaudio
- Opencv4