CXX = g++
CXXFLAGS = -std=c++11 -Wall
SERVER_EXEC = server
CLIENT_EXEC = client
AUDIO_OBJ = audio_streaming.o
FILE_OBJ = file_transfer_relay.o
SSL_OBJ = ssl.o

all: $(SERVER_EXEC) $(CLIENT_EXEC)

$(SERVER_EXEC): server.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ)
	$(CXX) $(CXXFLAGS) server.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) -o $(SERVER_EXEC) -lssl -lcrypto -lportaudio -lmpg123 -lsndfile

$(CLIENT_EXEC): client.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ)
	$(CXX) $(CXXFLAGS) client.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) -o $(CLIENT_EXEC) -lssl -lcrypto -lportaudio -lmpg123 -lsndfile

$(AUDIO_OBJ): audio_streaming.cpp
	$(CXX) $(CXXFLAGS) -c audio_streaming.cpp -o $(AUDIO_OBJ)

$(FILE_OBJ): file_transfer_relay.cpp
	$(CXX) $(CXXFLAGS) -c file_transfer_relay.cpp -o $(FILE_OBJ)

$(SSL_OBJ): ssl.cpp
	$(CXX) $(CXXFLAGS) -c ssl.cpp -o $(SSL_OBJ)


clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC) $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) 
