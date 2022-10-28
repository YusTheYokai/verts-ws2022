#include <algorithm>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <netinet/in.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

#include "command.h"
#include "messageUtils.h"

namespace fs = std::filesystem;

#define BUFFER 1024

std::string OK = "OK";
std::string ERR = "ERR";

std::string directoryName;

void sendCommand(int clientSocketFD, std::vector<std::string>& message) {
    // 0 = command, 1 = sender, 2 = receiver, 3 = subject, 4 = content
    fs::create_directory(directoryName + "/" + message[2]);
    std::string fileName = directoryName + "/" + message[2] + "/" + message[3];

    std::ofstream file(fileName);
    file << message[1] << std::endl << message[3] << std::endl << message[4];
    file.close();

    message.clear();
    message.push_back(OK);
}

void listCommand(int clientSocketFD, std::vector<std::string>& message) {
    int count = 0;

    message.clear();
    message.push_back("");
    
    try {
        fs::directory_iterator dir(directoryName + "/" + message[1]);
        for (auto file : dir) {
            message.push_back(file.path().filename());
            count++;
        }
    } catch (fs::filesystem_error& e) {
        // noop
    }
    message[0] = std::to_string(count);
}

void readCommand(int clientSocketFD, std::vector<std::string>& message) {
    if (!fs::is_directory(directoryName + "/" + message[1])) {
        message.clear();
        message.push_back(ERR);
        return;
    }

    fs::path filePath = "";

    int count = 0;
    fs::directory_iterator dir(directoryName + "/" + message[1]);
    for (auto file : dir) {
        if (count == std::stoi(message[2])) {
            filePath = file.path();
        }
        count++;
    }

    if (filePath == "") {
        message.clear();
        message.push_back(ERR);
        return;
    }

    std::ifstream file(filePath);
    std::string line;
    message.push_back(OK);
    while (std::getline(file, line)) {
        message.push_back(line);
    }
    file.close();
}

void deleteCommand(int clientSocketFD, std::vector<std::string>& message) {
    if (!fs::is_directory(directoryName + "/" + message[1])) {
        message.clear();
        message.push_back(ERR);
        return;
    }

    fs::path filePath = "";

    int count = 0;
    fs::directory_iterator dir(directoryName + "/" + message[1]);
    for (auto file : dir) {
        if (count == std::stoi(message[2])) {
            filePath = file.path();
        }
        count++;
    }

    if (filePath == "") {
        message.clear();
        message.push_back(ERR);
        return;
    }

    if (!fs::remove(filePath)) {
        message.clear();
        message.push_back(ERR);
        return;
    }

    message.clear();
    message.push_back(OK);
}

void quitCommand(int clientSocketFD, std::vector<std::string>& message) {
    exit(0);
}

int main(int argc, char* argv[]) {
    std::map<std::string, Command> commands;
    commands.insert(std::pair<std::string, Command>("SEND", Command("", "", sendCommand)));
    commands.insert(std::pair<std::string, Command>("LIST", Command("", "", listCommand)));
    commands.insert(std::pair<std::string, Command>("READ", Command("", "", readCommand)));
    commands.insert(std::pair<std::string, Command>("DEL", Command("", "", deleteCommand)));
    commands.insert(std::pair<std::string, Command>("QUIT", Command("", "", quitCommand)));

    int port;

    char c;

    while ((c = getopt(argc, argv, "")) != EOF) {
        switch (c) {
            case '?':
                std::cerr << "Unknown option" << std::endl;
                exit(1);
        }
    }

    if (optind < argc) {
        port = atoi(argv[optind++]);
        directoryName = argv[optind];
        // TODO: catch exception
    } else {
        std::cerr << "No port or spool directory name given" << std::endl;
        exit(1);
    }

    fs::create_directory(directoryName);

    int reuseValue = 1;
    int size;
    char buffer[BUFFER];
    socklen_t addressLength;
    struct sockaddr_in address;
    struct sockaddr_in clientAddress;

    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1) {
        std::cerr << "Could not create socket" << std::endl;
        exit(1);
    }

    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(int)) < 0) {
        std::cerr << "Could not set socket options" << std::endl;
        exit(1);
    }

    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEPORT, &reuseValue, sizeof(int)) < 0) {
        std::cerr << "Could not set socket options" << std::endl;
        exit(1);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketFD, (struct sockaddr*) &address, sizeof(address)) == -1) {
        std::cerr << "Bind has failed" << std::endl;
        exit(1);
    }

    if (listen(socketFD, 5) == -1) {
        std::cerr << "Could not listen" << std::endl;
        exit(1);
    }

    addressLength = sizeof(struct sockaddr_in);
    int clientSocketFD = accept(socketFD, (struct sockaddr*) &clientAddress, &addressLength);
    if (clientSocketFD == -1) {
        std::cerr << "Could not accept" << std::endl;
        exit(1);
    }

    std::cout << "Connection established" << std::endl;

    std::vector<std::string> lines;
    do {
        size = recv(clientSocketFD, buffer, BUFFER - 1, 0);
        MessageUtils::validateMessage(size);
        MessageUtils::parseMessage(buffer, size, lines);

        auto command = commands.at(lines[0]);
        command.getCommand()(clientSocketFD, lines);
        std::string response = MessageUtils::toString(lines);

        if (send(clientSocketFD, response.c_str(), response.size(), 0) == -1) {
            std::cerr << "Could not send" << std::endl;
            exit(1);
        }
    } while (1);
}
