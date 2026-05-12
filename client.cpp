#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

string call_namenode(string msg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return ""; // Connection fails silently to avoid terminal clutter
    }
    
    send(sock, msg.c_str(), msg.length(), 0);
    char buf[1024] = {0};
    read(sock, buf, 1024);
    close(sock);
    return string(buf);
}

void upload_to_node(int port, string chunk_name, char* data, int size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        string header = "SAVE_CHUNK:" + chunk_name;
        send(sock, header.c_str(), header.length(), 0);
        usleep(100000); 
        send(sock, data, size, 0);
    } else {
        // THE FIX: Actually warn us if a DataNode is unreachable!
        cerr << "[WARNING] Failed to connect to DataNode on port " << port << " for chunk " << chunk_name << endl;
    }
    close(sock);
}

void upload_file(string filename) {
    ifstream infile(filename, ios::binary);
    if (!infile.is_open()) {
        cerr << "Error: File " << filename << " not found!" << endl;
        return;
    }

    char buffer[1024]; 
    int chunk_id = 0;

    while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
        int bytes_read = infile.gcount();
        string chunk_name = filename + "_part" + to_string(chunk_id);
        
        string ports_raw = call_namenode(chunk_name);
        if (ports_raw.empty()) {
            cerr << "Failed to contact NameNode for chunk " << chunk_id << endl;
            break;
        }

        stringstream ss(ports_raw);
        string port_str;
        while (getline(ss, port_str, ',')) {
            if (!port_str.empty()) {
                upload_to_node(stoi(port_str), chunk_name, buffer, bytes_read);
            }
        }
        chunk_id++;
        usleep(50000); // 50ms delay to keep the M4 Mac network stack happy
    }
    cout << "Upload Complete. Total Chunks: " << chunk_id << endl;
}

void download_file(string filename) {
    string port_list_raw = call_namenode("GET_LIST:" + filename);
    if (port_list_raw.empty()) {
        cout << "Could not retrieve chunk list from NameNode." << endl;
        return;
    }

    stringstream ss(port_list_raw);
    vector<int> ports;
    string p;
    while(getline(ss, p, ',')) if(!p.empty()) ports.push_back(stoi(p));

    ofstream final_file("downloaded_" + filename, ios::binary);
    
    // Every 2 ports = 1 chunk (Replication Factor 2)
    for (int i = 0; i < ports.size(); i += 2) {
        int chunk_id = i / 2;
        bool success = false;
        for (int j = 0; j < 2; ++j) {
            int target_port = ports[i + j];
            int ds_sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in ds_addr;
            ds_addr.sin_family = AF_INET;
            ds_addr.sin_port = htons(target_port);
            inet_pton(AF_INET, "127.0.0.1", &ds_addr.sin_addr);

            if (connect(ds_sock, (struct sockaddr *)&ds_addr, sizeof(ds_addr)) == 0) {
                string cmd = "GET_CHUNK:" + filename + "_part" + to_string(chunk_id);
                send(ds_sock, cmd.c_str(), cmd.length(), 0);
                char buf[1024] = {0};
                int bytes = read(ds_sock, buf, 1024);
                final_file.write(buf, bytes);
                close(ds_sock);
                success = true;
                break; 
            }
            close(ds_sock);
        }
    }
    cout << "Download/Reassembly complete." << endl;
}

int main() {
    string file = "hello.txt";
    upload_file(file);
    cout << "System ready. Reassembling..." << endl;
    sleep(3); // Wait for DataNodes to finish disk I/O
    download_file(file);
    return 0;
}