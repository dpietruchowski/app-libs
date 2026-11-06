#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

std::string g_capturePath;

std::string escaped(const std::string& text)
{
    std::string result;
    for (const unsigned char character : text)
    {
        switch (character)
        {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
                    result += buffer;
                }
                else
                    result += static_cast<char>(character);
        }
    }
    return result;
}

std::string jsonQuoted(const std::string& text) { return "\"" + escaped(text) + "\""; }

void record(const std::string& json)
{
    std::ofstream file(g_capturePath, std::ios::app);
    file << json << '\n';
}

std::string currentDirectory()
{
    char buffer[4096];
    return getcwd(buffer, sizeof(buffer)) ? std::string(buffer) : std::string();
}

int startsSoFar()
{
    std::ifstream file(g_capturePath);
    std::string line;
    int starts = 0;
    while (std::getline(file, line))
    {
        if (line.rfind("{\"argv\":", 0) == 0)
            ++starts;
    }
    return starts;
}

void recordStart(int argc, char** argv)
{
    std::string arguments;
    for (int i = 3; i < argc; ++i)
        arguments += (arguments.empty() ? "" : ",") + jsonQuoted(argv[i]);

    record("{\"argv\":[" + arguments + "],\"cwd\":" + jsonQuoted(currentDirectory())
           + ",\"pid\":" + std::to_string(getpid()) + "}");
}

void bufferLine(const std::string& line) { std::cout << line << '\n'; }

void writeLine(const std::string& line)
{
    bufferLine(line);
    std::cout << std::flush;
}

void waitForInputLine()
{
    std::string line;
    if (!std::getline(std::cin, line))
    {
        record("{\"stdin\":null}");
        std::exit(3);
    }
    record("{\"stdin\":" + jsonQuoted(line) + "}");
}

void waitForEndOfInput()
{
    std::string line;
    while (std::getline(std::cin, line))
        record("{\"stdin\":" + jsonQuoted(line) + "}");
    record("{\"eof\":true}");
}

void sleepMilliseconds(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void hangForever()
{
    for (;;)
        pause();
}

void spawnChild()
{
    const pid_t child = fork();
    if (child == 0)
        hangForever();
    record("{\"child\":" + std::to_string(child) + "}");
}

void recordEnvironment(const std::string& name)
{
    const char* value = std::getenv(name.c_str());
    record("{\"env\":" + jsonQuoted(name) + ",\"value\":" + (value ? jsonQuoted(value) : "null")
           + "}");
}

void recordIdentity()
{
    int deathSignal = 0;
    prctl(PR_GET_PDEATHSIG, &deathSignal);
    record("{\"pid\":" + std::to_string(getpid()) + ",\"pgid\":" + std::to_string(getpgrp())
           + ",\"sid\":" + std::to_string(getsid(0))
           + ",\"pdeathsig\":" + std::to_string(deathSignal) + "}");
}

void runCommand(const std::string& command)
{
    const std::size_t space = command.find(' ');
    const std::string name = command.substr(0, space);
    const std::string argument = space == std::string::npos ? "" : command.substr(space + 1);

    if (name == "sleep")
        sleepMilliseconds(std::stoi(argument));
    else if (name == "exit")
        std::exit(std::stoi(argument));
    else if (name == "crash")
        std::abort();
    else if (name == "hang")
        hangForever();
    else if (name == "spawn-child")
        spawnChild();
    else if (name == "env")
        recordEnvironment(argument);
    else if (name == "identity")
        recordIdentity();
    else if (name == "eof")
        waitForEndOfInput();
}

}

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;

    g_capturePath = argv[2];
    const int start = startsSoFar();
    recordStart(argc, argv);

    std::ifstream script(argv[1]);
    std::string line;
    int section = 0;
    while (std::getline(script, line))
    {
        if (line == "---")
        {
            ++section;
            continue;
        }
        if (section != start)
            continue;

        if (line.rfind("< ", 0) == 0)
            writeLine(line.substr(2));
        else if (line.rfind("+ ", 0) == 0)
            bufferLine(line.substr(2));
        else if (line.rfind(">", 0) == 0)
            waitForInputLine();
        else if (line.rfind("! ", 0) == 0)
            runCommand(line.substr(2));
    }
    return 0;
}
