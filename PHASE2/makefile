# 編譯器
CXX = g++

# 編譯器選項
CXXFLAGS = -std=c++11 -Wall

# 執行檔名稱
SERVER_EXEC = server
CLIENT_EXEC = client

# 編譯規則
all: $(SERVER_EXEC) $(CLIENT_EXEC)

$(SERVER_EXEC): server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o $(SERVER_EXEC) -lssl -lcrypto

$(CLIENT_EXEC): client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o $(CLIENT_EXEC) -lssl -lcrypto

# 清除編譯生成的檔案
clean:
	rm -f $(SERVER_EXEC) $(CLIENT_EXEC)
