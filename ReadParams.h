
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define MEM_ALIGN 64
#include <Windows.h>

using namespace std;
class ReadParams
{
private:
    // A map to store the key-value pairs
    map<string, string> data;

    // A function to trim the leading and trailing whitespaces of a string
    string trim(const string& s);
public:
    ReadParams(const string& filename);

    // A function to get the value of a given key
    // If the key does not exist, return an empty string
    string get(const string& key) const;

    // A function to print the config map
    void print() const;
};

class CameraParams{
public:
    void setParms(ReadParams& params) {
        name = params.get("camera_name");
        rtbuffersize = stoull(params.get("rtbuffersize"));
        framerate = stod(params.get("framerate"));
        width = stoi(params.get("width"));
        height = stoi(params.get("height"));
        bitrate = stoi(params.get("bitrate"));
        encoder = params.get("encoder");
    }

    string get_name() const{
        return name;
    }
        
    uint64_t get_buffer() const{
        return rtbuffersize;
    }
    double get_framerate() const{
        return framerate;
    }
    int get_width() const{
        return width;
    }
    int get_height() const
    {
        return height;
    }
    long get_bitrate() const{
        return bitrate;
    }
    string get_encoder() const{
        return encoder;
    }

private:
    string             name;
    uint64_t           rtbuffersize;       // real time buffer size, in byte
    double             framerate;       // frame rate  
    int                width;            // picture size
    int                height;           // picture height
    long               bitrate;
    string             encoder;          // set encoder for camera frame
};
