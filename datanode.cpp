#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h> // REQUIRED FOR inet_pton

using namespace std;

int main(int argc, char const *argv[]) {
    int port = (argc > 1) ? stoi(argv[1]) : 8081;
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    // THE FIX: Explicitly bind to IPv4 localhost
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr); 
    address.sin_port = htons(port);

    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("DataNode bind failed");
        return 1;
    }
    
    ::listen(server_fd, 20);
    
    string storage_dir = "storage_" + to_string(port);
    system(("mkdir -p " + storage_dir).c_str());

    cout << "DataNode active on port " << port << " (IPv4)..." << endl;

    while(true) {
        new_socket = ::accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        char buffer[1024] = {0};
        int valread = read(new_socket, buffer, 1024);
        string request(buffer);

        if (request.find("GET_CHUNK:") == 0) {
            string chunk_name = request.substr(10);
            ifstream infile(storage_dir + "/" + chunk_name, ios::binary);
            char file_buffer[1024] = {0};
            infile.read(file_buffer, 1024);
            send(new_socket, file_buffer, infile.gcount(), 0);
        } 
        else if (request.find("SAVE_CHUNK:") == 0) {
            string chunk_name = request.substr(11);
            ofstream outfile(storage_dir + "/" + chunk_name, ios::binary);
            
            char data_buf[1024] = {0};
            int bytes = read(new_socket, data_buf, 1024);
            if (bytes > 0) outfile.write(data_buf, bytes);
            
            outfile.close();
            cout << "[LOG] Stored: " << chunk_name << " (" << bytes << " bytes)" << endl;
        }
        close(new_socket);
    }
    return 0;
}