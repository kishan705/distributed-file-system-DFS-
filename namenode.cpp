#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    map<string, vector<int>> file_metadata;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // SO_REUSEPORT is helpful for Mac M4 development
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    // Explicitly bind to 127.0.0.1 to match Client
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    address.sin_port = htons(9000);

    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    listen(server_fd, 20); 

    cout << "NameNode (The Brain) is active on port 9000..." << endl;

    while(true) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        char buffer[1024] = {0};
        read(new_socket, buffer, 1024);
        string request(buffer);

        if (request.empty()) { close(new_socket); continue; }

        if (request.find("GET_LIST:") == 0) {
            string filename = request.substr(9);
            string response = "";
            if (file_metadata.count(filename)) {
                for (int port : file_metadata[filename]) {
                    response += to_string(port) + ",";
                }
            }
            send(new_socket, response.c_str(), response.length(), 0);
        } 
        else {
            string resp = "8081,8082"; 
            size_t part_pos = request.find("_part");
            if (part_pos != string::npos) {
                string original_file = request.substr(0, part_pos);
                file_metadata[original_file].push_back(8081);
                file_metadata[original_file].push_back(8082);
            }
            send(new_socket, resp.c_str(), resp.length(), 0);
            cout << "[LOG] Registered chunk: " << request << endl;
        }
        close(new_socket);
    }
    return 0;
}