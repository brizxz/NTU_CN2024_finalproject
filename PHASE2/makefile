CXX = g++
CXXFLAGS = -std=c++17 -Wall

# 若系統已安裝 pkg-config，可用下行自動找 OpenCV 參數
# (若沒有 pkg-config 或路徑不同，可以自行改成 -I / -L 形式)
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4)

SERVER_EXEC = server
CLIENT_EXEC = client

AUDIO_OBJ = audio_streaming.o
FILE_OBJ  = file_transfer_relay.o
SSL_OBJ   = ssl.o
VIDEO_OBJ = video_streaming.o

all: $(SERVER_EXEC) $(CLIENT_EXEC)

# ---- [Server] ----
$(SERVER_EXEC): server.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) $(VIDEO_OBJ)
	$(CXX) $(CXXFLAGS) server.cpp \
	    $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) $(VIDEO_OBJ) \
	    -o $(SERVER_EXEC) \
	    -lssl -lcrypto -lportaudio -lmpg123 -lsndfile \
	    $(OPENCV_FLAGS)

# ---- [Client] ----
$(CLIENT_EXEC): client.cpp $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) $(VIDEO_OBJ)
	$(CXX) $(CXXFLAGS) client.cpp \
	    $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) $(VIDEO_OBJ) \
	    -o $(CLIENT_EXEC) \
	    -lssl -lcrypto -lportaudio -lmpg123 -lsndfile \
	    $(OPENCV_FLAGS)

# ---- [Audio Streaming OBJ] ----
$(AUDIO_OBJ): utils/audio_streaming.cpp utils/audio_streaming.hpp
	$(CXX) $(CXXFLAGS) -c utils/audio_streaming.cpp -o $(AUDIO_OBJ)

# ---- [Video Streaming OBJ] ----
$(VIDEO_OBJ): utils/video_streaming.cpp utils/video_streaming.hpp
	$(CXX) $(CXXFLAGS) $(OPENCV_FLAGS) -c utils/video_streaming.cpp -o $(VIDEO_OBJ)

# ---- [File Transfer Relay OBJ] ----
$(FILE_OBJ): utils/file_transfer_relay.cpp utils/file_transfer_relay.hpp
	$(CXX) $(CXXFLAGS) -c utils/file_transfer_relay.cpp -o $(FILE_OBJ)

# ---- [SSL OBJ] ----
$(SSL_OBJ): utils/ssl.cpp utils/ssl.h
	$(CXX) $(CXXFLAGS) -c utils/ssl.cpp -o $(SSL_OBJ)

clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC) \
	      $(AUDIO_OBJ) $(FILE_OBJ) $(SSL_OBJ) $(VIDEO_OBJ)
