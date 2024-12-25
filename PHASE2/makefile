CXX = g++
CXXFLAGS = -std=c++11 -Wall
SERVER_EXEC = server
CLIENT_EXEC = client
DATA_OBJ = audio_streaming.o

all: $(SERVER_EXEC) $(CLIENT_EXEC)

$(SERVER_EXEC): server.cpp $(DATA_OBJ)
	$(CXX) $(CXXFLAGS) server.cpp $(DATA_OBJ) -o $(SERVER_EXEC) -lssl -lcrypto -lportaudio -lmpg123 -lsndfile

$(CLIENT_EXEC): client.cpp $(DATA_OBJ)
	$(CXX) $(CXXFLAGS) client.cpp $(DATA_OBJ) -o $(CLIENT_EXEC) -lssl -lcrypto -lportaudio -lmpg123 -lsndfile

$(DATA_OBJ): audio_streaming.cpp
	$(CXX) $(CXXFLAGS) -c audio_streaming.cpp -o $(DATA_OBJ)

clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC) $(DATA_OBJ)
